/*
 * Copyright 2026 BeatBax FM-1 Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Float-free, allocation-free note rendering core (feature 004).
 *
 * The per-sample loops here are the exact fixed-point equivalents of BeatBax's
 * renderPulse / renderWave / renderNoise. They are driven by a `gb_note_t` so the
 * same code renders a note incrementally across audio-callback blocks on-device.
 * No `float`/`double`/`atof` appears in this file (those stay in the host-side
 * `gb_note_desc.c` resolver), and no heap allocation occurs in the hot path.
 */

#include "gb_note_render.h"

#include "gb_tables.h"
#include "gb_voice.h"

#include <string.h>

/*
 * Wave exponential release decay in Q30. The wave tail is round(20 ms × sr)
 * samples; at 44100 Hz and 44118 Hz that rounds to 882 samples, so the per-sample
 * decay is exp(-1/882) and a single constant serves both supported rates.
 */
#define GB_WAVE_EXP_DECAY_Q30 1072525120

/* libc-free string equality (firmware's minimal libc has no strcmp). */
static GB_RAMFN bool streq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/* Q16 fixed-point helpers. */
static GB_RAMFN int32_t q16_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

/* 2^(semi_q16 / 2^16 / 12) in Q16, direct lookup in the fine table. */
GB_RAMFN int32_t gb_pow2_semi_q16(int32_t semi_q16) {
    int32_t idx = (semi_q16 >> 8) + GB_POW2_FINE_OFFSET;
    if (idx < 0) idx = 0;
    if (idx >= GB_POW2_FINE_COUNT) idx = GB_POW2_FINE_COUNT - 1;
    return gb_pow2_fine_q16[idx];
}

/* log2(v) in Q16 for v > 0. Normalizes to [2^23, 2^24), then table + linear interp. */
GB_RAMFN int32_t gb_log2_q16(uint32_t v) {
    if (v == 0) {
        return (int32_t)0x80000000;
    }
    int e = 0;
    while (v < 0x800000u) { v <<= 1; e--; }
    while (v >= 0x1000000u) { v >>= 1; e++; }
    uint32_t frac23 = v & 0x7FFFFFu;
    uint32_t idx = frac23 >> 15;
    uint32_t rem = frac23 & 0x7FFFu;
    int32_t lo = gb_log2_frac_q16[idx];
    int32_t hi = gb_log2_frac_q16[idx + 1];
    int32_t interp = lo + (((hi - lo) * (int32_t)rem) >> 15);
    return ((int32_t)(e + 23) << 16) + interp;
}

static GB_RAMFN uint32_t freq_q16_to_inc(int32_t freq_q16, uint32_t sample_rate) {
    if (freq_q16 <= 0 || sample_rate == 0) {
        return 0;
    }
    uint64_t inc = ((uint64_t)freq_q16 << 16) / sample_rate;
    if (inc > 0xFFFFFFFFull) {
        inc = 0xFFFFFFFFull;
    }
    return (uint32_t)inc;
}

/* Vibrato LFO sample in Q15 for one cycle fraction `phase` (Q32). Matches BeatBax
 * lfoSample: phase 0 = square high, triangle -1, sine 0. */
static GB_RAMFN int32_t gb_lfo_q15(uint32_t phase, uint8_t waveform) {
    uint32_t p16 = (phase >> 16) & 0xFFFFu; /* Q16 cycle fraction [0,1) */
    switch (waveform) {
        case GB_VIB_SQUARE:
            return p16 < 0x8000u ? 32767 : -32768;
        case GB_VIB_SAW:
            return (int32_t)p16 - 32768; /* 2p - 1 */
        case GB_VIB_TRIANGLE: {
            int32_t v = (p16 < 0x8000u) ? ((int32_t)(2u * p16) - 32768)  /* 4p - 1 */
                                          : (98304 - (int32_t)(2u * p16)); /* 3 - 4p */
            return v > 32767 ? 32767 : v; /* clamp the +1.0 peak into Q15 */
        }
        default: { /* sine */
            int sin_idx = (int)((phase >> 24) & 0xFF);
            return gb_sin_q15[sin_idx];
        }
    }
}

static GB_RAMFN uint32_t duty_index_to_x10(int idx) {
    switch (idx) {
        case 0: return 125;
        case 1: return 250;
        case 3: return 750;
        default: return 500;
    }
}

/* LFSR phase increment for a noise clock shifted by a macro offset. */
static GB_RAMFN uint64_t noise_lfsr_inc_for_offset(int base_index, int offset, uint32_t sample_rate) {
    int note_index = base_index + offset;
    if (note_index < 0) note_index = 0;
    if (note_index > 72) note_index = 72;
    uint8_t poly = gb_noise_note_poly(note_index);
    uint32_t s2 = (poly >> 4) & 7u;
    uint32_t d2 = poly & 7u;
    return gb_noise_lfsr_inc(s2, d2, sample_rate);
}

/* True if the tick program writes volume anywhere. */
static GB_RAMFN bool prog_has_volume(const gb_tick_program_t *p) {
    for (int i = 0; i < p->count; i++) {
        if (p->rows[i].vol >= 0) {
            return true;
        }
    }
    return false;
}

/* Advance the tick program and return the active row (or NULL). */
static GB_RAMFN const gb_tick_row_t *tick_row_at(const gb_tick_program_t *p, int *last_tick,
                                        uint32_t i, uint32_t sample_rate) {
    int tick = (int)(((uint64_t)i * GB_TICK_SEC_DEN) / sample_rate);
    if (tick == *last_tick) {
        return NULL;
    }
    *last_tick = tick;
    return gb_tick_program_row(p, tick);
}

GB_RAMFN void gb_channel_state_init(gb_channel_state_t *st) {
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
}

/* Set up the incremental envelope for a pulse/noise note.  env_den == 0 means a
 * constant level (period 0 / flat dir / bad rate).  The per-sample step is an
 * add + carry, bit-identical to gb_envelope_q15's `step_q16 = te*64/den << 16`
 * but without the per-sample 64-bit division. */
static GB_RAMFN void env_begin(gb_note_t *n, uint32_t sample_rate) {
    uint8_t init = n->env_initial > 15 ? 15 : n->env_initial;
    if (n->env_period == 0 || n->env_dir == GB_ENV_FLAT || sample_rate == 0) {
        n->env_den = 0;
        n->env_step_q16 = 0;
        n->env_rem = 0;
        return;
    }
    n->env_den = (uint32_t)n->env_period * sample_rate;
    uint64_t max_steps = ((uint64_t)n->nominal_len * 64u) / n->env_den;
    if (max_steps < 1u) {
        max_steps = 1u;
    }
    uint32_t saturate = (n->env_dir == GB_ENV_UP) ? (uint32_t)(15u - init) : (uint32_t)init;
    uint32_t n_steps = (uint32_t)(max_steps < saturate ? max_steps : saturate);
    n->env_nsteps_q16 = n_steps << 16;
    n->env_q = 4194304u / n->env_den;
    n->env_r = 4194304u % n->env_den;
    n->env_step_q16 = 0;
    n->env_rem = 0;
}

/* Advance the incremental envelope one sample (only while i < nominal_len). */
static GB_RAMFN void env_advance(gb_note_t *n) {
    if (n->env_den == 0) {
        return;
    }
    uint32_t step_q16 = n->env_step_q16 + n->env_q;
    uint32_t rem = n->env_rem + n->env_r;
    if (rem >= n->env_den) {
        rem -= n->env_den;
        step_q16++;
    }
    if (step_q16 > n->env_nsteps_q16) {
        step_q16 = n->env_nsteps_q16;
    }
    n->env_step_q16 = step_q16;
    n->env_rem = rem;
}

/* Current envelope level in Q15 (bit-identical to gb_envelope_q15). */
static GB_RAMFN int16_t env_volume(const gb_note_t *n) {
    uint8_t init = n->env_initial > 15 ? 15 : n->env_initial;
    int64_t level_q16 = (int64_t)init << 16;
    level_q16 += (n->env_dir == GB_ENV_UP) ? (int64_t)n->env_step_q16 : -(int64_t)n->env_step_q16;
    return (int16_t)((level_q16 * 32767) / (int64_t)(15 << 16));
}

static GB_RAMFN void begin_pulse(gb_note_t *n, const gb_note_desc_t *desc, int midi,
                        uint32_t sample_rate, gb_channel_state_t *state, int channel) {
    n->current_reg = gb_midi_to_period(midi);
    n->duty_x10 = desc->duty_x10;

    bool has_sweep = desc->sweep_present && desc->sweep_time > 0;
    uint32_t sweep_interval = has_sweep
                                  ? (uint32_t)((uint64_t)desc->sweep_time * sample_rate / 128u)
                                  : 0;
    bool use_prog_vol = n->prog.enabled && prog_has_volume(&n->prog);
    if (n->prog.enabled) {
        has_sweep = false;
    }
    n->has_sweep = has_sweep;
    n->sweep_interval = sweep_interval;
    n->sweep_shift = desc->sweep_shift;
    n->sweep_dir = desc->sweep_dir;

    n->env_initial = desc->env_initial;
    n->env_period = desc->env_period;
    n->env_dir = desc->env_dir;
    n->use_prog_vol = use_prog_vol;
    env_begin(n, sample_rate);
    n->arp_semi = n->arp_semis[0]; /* arp_frame / arp_rem start at 0 (memset) */

    n->last_tick = -1;
    n->current_offset = 0;
    n->vol_scale = 15;
    if (n->prog.enabled) {
        const gb_tick_row_t *r0 = gb_tick_program_row(&n->prog, 0);
        if (r0) {
            n->current_offset = r0->offset;
            if (use_prog_vol && r0->vol >= 0) n->vol_scale = r0->vol;
            if (r0->duty >= 0) n->duty_x10 = duty_index_to_x10(r0->duty);
        }
    }

    n->last_freq = state->last_freq_q16[channel];
    state->last_freq_q16[channel] = gb_period_to_freq_q16(gb_midi_to_period(midi));
    /* BeatBax continuePhase: a portamento note (legato) or a joined portamento
     * voice continues the oscillator phase; a normal note starts from phase 0.
     * The resolver marks every port note legato, so a single C5<slide> still
     * carries the previous note's phase. */
    {
        bool continue_phase = n->join || n->port_speed > 0;
        n->phase = continue_phase ? state->pulse_phase[channel] : 0;
        n->vib_phase = continue_phase ? state->vib_phase[channel] : 0;
    }
    n->log2_last_q16 = (n->last_freq > 0) ? gb_log2_q16((uint32_t)n->last_freq) : 0;
}

static GB_RAMFN void begin_wave(gb_note_t *n, const gb_note_desc_t *desc, int midi,
                       uint32_t sample_rate, gb_channel_state_t *state, int channel) {
    (void)sample_rate;
    n->base_freq = gb_period_to_freq_q16(gb_midi_to_period(midi));
    n->wave_phase = 0;
    memcpy(n->wave_table, desc->wave_table, sizeof(n->wave_table));
    n->wave_sum = desc->wave_sum;
    n->vol_div = gb_wave_volume_div(desc->wave_volume);

    n->use_prog_vol = n->prog.enabled && prog_has_volume(&n->prog);
    n->last_tick = -1;
    n->current_offset = 0;
    n->vol_scale = 15;
    if (n->prog.enabled) {
        const gb_tick_row_t *r0 = gb_tick_program_row(&n->prog, 0);
        if (r0) {
            n->current_offset = r0->offset;
            if (n->use_prog_vol && r0->vol >= 0) n->vol_scale = r0->vol;
        }
    }

    n->last_freq = state->last_freq_q16[channel];
    state->last_freq_q16[channel] = n->base_freq;
}

static GB_RAMFN void begin_noise(gb_note_t *n, const gb_note_desc_t *desc, uint32_t sample_rate) {
    n->noise_base_index = desc->noise_note_index;
    n->noise_shift = desc->noise_shift;
    n->noise_divisor = desc->noise_divisor;
    n->width7 = (desc->noise_width == 7);

    n->env_initial = desc->env_initial;
    n->env_period = desc->env_period;
    n->env_dir = desc->env_dir;
    n->use_prog_vol = n->prog.enabled && prog_has_volume(&n->prog);
    env_begin(n, sample_rate);

    n->last_tick = -1;
    n->current_offset = 0;
    n->vol_scale = 15;
    if (n->prog.enabled) {
        const gb_tick_row_t *r0 = gb_tick_program_row(&n->prog, 0);
        if (r0) {
            n->current_offset = r0->offset;
            if (n->use_prog_vol && r0->vol >= 0) n->vol_scale = r0->vol;
        }
    }

    if (n->prog.enabled) {
        n->lfsr_inc = noise_lfsr_inc_for_offset(n->noise_base_index, n->current_offset, sample_rate);
    } else {
        n->lfsr_inc = gb_noise_lfsr_inc(n->noise_shift, n->noise_divisor, sample_rate);
    }
    n->nphase = 0;
    n->lfsr = gb_noise_lfsr_trigger(n->width7);
}

static GB_RAMFN void pulse_step(gb_note_t *n, gb_sample_t *dst, uint32_t count) {
    gb_channel_state_t *state = n->chan;
    int channel = n->channel;
    gb_phase_t phase = n->phase;
    gb_phase_t vib_phase = n->vib_phase;
    uint32_t sample_rate = n->sample_rate;
    int32_t last_freq = n->last_freq;

    uint32_t current_reg = n->current_reg;
    uint32_t duty_x10 = n->duty_x10;
    int current_offset = n->current_offset;
    int vol_scale = n->vol_scale;
    int last_tick = n->last_tick;
    const gb_tick_program_t *prog = &n->prog;
    bool use_prog_vol = n->use_prog_vol;
    bool has_sweep = n->has_sweep;
    uint32_t sweep_interval = n->sweep_interval;
    uint32_t nominal_len = n->nominal_len;
    uint32_t release_tail = n->len - n->nominal_len;
    bool port_flat = n->join || n->hold;
    /* A single portamento note (legato, not part of a run) sustains the previous
     * note's final envelope level instead of re-attacking (BeatBax legato). */
    bool legato = n->port_speed > 0 && !n->join && !n->hold;
    uint32_t chrome_default = n->chrome_default;
    int32_t cached_osc_freq = n->cached_osc_freq;
    uint32_t cached_inc = n->cached_inc;

    for (uint32_t k = 0; k < count; k++) {
        uint32_t i = n->pos + k;

        if (prog->enabled) {
            const gb_tick_row_t *row = tick_row_at(prog, &last_tick, i, sample_rate);
            if (row) {
                if (row->offset != current_offset) {
                    current_offset = row->offset;
                }
                if (use_prog_vol && row->vol >= 0) {
                    vol_scale = row->vol;
                }
                if (row->duty >= 0) {
                    duty_x10 = duty_index_to_x10(row->duty);
                }
            }
        }

        /* NR10 frequency sweep step. */
        if (has_sweep && sweep_interval > 0 && i > 0 && i % sweep_interval == 0) {
            uint32_t delta = current_reg >> (n->sweep_shift & 7u);
            if (n->sweep_dir == BAX_SWEEP_DIR_UP) {
                current_reg += delta;
            } else if (current_reg > delta) {
                current_reg -= delta;
            } else {
                current_reg = 0;
            }
        }

        int32_t base_freq = (current_reg > 2047u) ? 0 : gb_period_to_freq_q16(current_reg);
        int32_t base_for_mod = base_freq;
        if (current_offset != 0) {
            int idx = current_offset + GB_POW2_OFFSET;
            if (idx < 0) idx = 0;
            if (idx >= GB_POW2_COUNT) idx = GB_POW2_COUNT - 1;
            base_for_mod = q16_mul(base_for_mod, gb_pow2_q16[idx]);
        }
        int32_t freq = base_for_mod;

        /* Portamento: exponential (geometric) glide from the previous note's
         * frequency to the current (matches exponentialRampToValueAtTime). */
        if (n->port_speed > 0 && last_freq > 0 && freq > 0 && freq != last_freq &&
            (int32_t)i <= n->port_dur_samples) {
            uint32_t p_q16 = n->port_p_q16;
            n->port_p_q16 += n->port_q;
            uint32_t pr = n->port_rem + n->port_r;
            if (pr >= (uint32_t)n->port_dur_samples) {
                pr -= (uint32_t)n->port_dur_samples;
                n->port_p_q16++;
            }
            n->port_rem = pr;
            int32_t semi_diff_q16 = 12 * (gb_log2_q16((uint32_t)freq) - n->log2_last_q16);
            int32_t semi_q16 = (int32_t)(((int64_t)semi_diff_q16 * (int64_t)p_q16) >> 16);
            int octave = semi_q16 / (12 << 16);
            int32_t frac_semi_q16 = semi_q16 - octave * (12 << 16);
            int64_t f64 = (int64_t)q16_mul(last_freq, gb_pow2_semi_q16(frac_semi_q16));
            if (octave >= 0) {
                f64 <<= octave;
            } else {
                f64 >>= -octave;
            }
            freq = (int32_t)f64;
        }

        /* Arpeggio: cycle offsets at 60 Hz. */
        if (n->arp_count > 0 && freq > 0) {
            int semi = n->arp_semi;
            n->arp_rem += 60u;
            if (n->arp_rem >= sample_rate) {
                n->arp_rem -= sample_rate;
                n->arp_frame++;
                n->arp_semi = n->arp_semis[n->arp_frame % n->arp_count];
            }
            if (semi != 0) {
                int idx = semi + GB_POW2_OFFSET;
                if (idx < 0) idx = 0;
                if (idx >= GB_POW2_COUNT) idx = GB_POW2_COUNT - 1;
                freq = q16_mul(freq, gb_pow2_q16[idx]);
            }
        }

        /* Pitch bend. */
        if (n->bend_semi_q16 != 0 && n->bend_time_samples > 0 &&
            (int32_t)i >= n->bend_delay_samples) {
            uint32_t p = n->bend_p_q16;
            if (n->bend_p_q16 < 65536u) {
                n->bend_p_q16 += n->bend_q;
                n->bend_rem += n->bend_r;
                if (n->bend_rem >= (uint32_t)n->bend_time_samples) {
                    n->bend_rem -= (uint32_t)n->bend_time_samples;
                    n->bend_p_q16++;
                }
                if (n->bend_p_q16 > 65536u) n->bend_p_q16 = 65536u;
            }
            if (streq(n->bend_curve, "exp") || streq(n->bend_curve, "exponential")) {
                p = (uint32_t)(((uint64_t)p * p) >> 16);
            } else if (streq(n->bend_curve, "log") || streq(n->bend_curve, "logarithmic")) {
                uint32_t q = 65536u - p;
                p = 65536u - (uint32_t)(((uint64_t)q * q) >> 16);
            } else if (streq(n->bend_curve, "sine") || streq(n->bend_curve, "sin")) {
                /* BeatBax: (1 - cos(pi*p)) / 2. cos(pi*p) = sin(pi*(p+0.5)); look up
                 * the 256-entry sine table with linear interpolation (index
                 * (p_q16/512 + 64) + 9-bit fraction). */
                uint32_t idx = (p >> 9) + 64u;
                uint32_t frac = p & 0x1FFu;
                int32_t s0 = gb_sin_q15[idx];
                int32_t s1 = gb_sin_q15[idx + 1u];
                int32_t cos_q15 = s0 + (((s1 - s0) * (int32_t)frac) >> 9);
                int32_t sp = 32768 - cos_q15; /* (1 - cos)/2 in Q16 */
                if (sp < 0) sp = 0;
                if (sp > 65536) sp = 65536;
                p = (uint32_t)sp;
            }
            int32_t semi_q16 = (int32_t)(((int64_t)n->bend_semi_q16 * (int64_t)p) >> 16);
            freq = q16_mul(freq, gb_pow2_semi_q16(semi_q16));
        }

        /* Vibrato: additive sine LFO keyed off the un-arpeggiated base frequency. */
        /* Vibrato: additive LFO keyed off the un-arpeggiated base frequency,
         * gated by onset delay and duration (BeatBax modulatePitchHz). */
        bool vib_after_delay = (int32_t)i >= n->vib_delay_samples;
        bool vib_within_dur = n->vib_duration_samples < 0 || (int32_t)i < n->vib_duration_samples;
        if (n->vib_depth != 0 && n->vib_rate > 0 && freq > 0 && vib_after_delay && vib_within_dur) {
            int32_t s = gb_lfo_q15(vib_phase, n->vib_waveform);
            int64_t f2 = (int64_t)base_for_mod * base_for_mod;
            int32_t amp = (int32_t)((f2 >> 33) * n->vib_depth);
            freq += (int32_t)(((int64_t)s * amp) >> 15);
            vib_phase += (uint32_t)(((uint64_t)n->vib_rate << 32) / sample_rate);
        }

        int32_t osc_freq = freq;
        if (i < chrome_default) {
            osc_freq = 440 << 16; /* 440 Hz in Q16 (Chromium startup) */
        }
        uint32_t inc;
        if (osc_freq == cached_osc_freq) {
            inc = cached_inc;
        } else {
            inc = freq_q16_to_inc(osc_freq, sample_rate);
            cached_osc_freq = osc_freq;
            cached_inc = inc;
        }
        if (inc == 0) {
            /* Silence: freeze phase, but still capture it at the nominal end. */
            if (i == nominal_len - 1) {
                state->pulse_phase[channel] = phase;
                state->vib_phase[channel] = vib_phase;
            }
            dst[k] = 0;
            continue;
        }
        /* Sample at the current phase, then advance (matches BeatBax: sample =
         * f(phase); phase += oscFreq/sr). Advancing first shifted every pulse note
         * by one phase increment. */
        int16_t bip = gb_pulse_sample(phase, duty_x10);
        phase += inc;
        /* Save the phase at the nominal end (BeatBax phaseAtNominalEnd); the
         * release tail continues with the local phase and must not disturb the
         * next note's start phase. */
        if (i == nominal_len - 1) {
            state->pulse_phase[channel] = phase;
            state->vib_phase[channel] = vib_phase;
        }

        int16_t amp;
        if (port_flat) {
            /* A held/joined portamento voice stays at the opening envelope level. */
            amp = (int16_t)((n->env_initial * 32767) / 15);
        } else if (legato) {
            /* A single portamento note sustains the previous note's final level. */
            amp = state->env_last_q15[channel];
        } else if (use_prog_vol) {
            amp = (int16_t)((vol_scale * 32767) / 15);
        } else {
            if (i < nominal_len) {
                env_advance(n);
            }
            amp = env_volume(n);
        }
        if (i == nominal_len - 1) {
            state->env_last_q15[channel] = amp;
        }
        int16_t level = (int16_t)(amp >> 2); /* * 0.25 */
        if (n->vol_override >= 0) {
            level = (int16_t)((int32_t)level * n->vol_override / 15);
        }
        /* Linear release tail past the nominal end (5 ms). */
        if (release_tail > 0 && i >= nominal_len) {
            uint32_t r = i - nominal_len;
            int32_t g = (int32_t)(((release_tail - r) * 32767) / release_tail);
            level = (int16_t)(((int32_t)level * g) >> 15);
        }
        dst[k] = (gb_sample_t)(((int32_t)bip * (int32_t)level) >> 15);
    }

    n->current_reg = current_reg;
    n->duty_x10 = duty_x10;
    n->current_offset = current_offset;
    n->vol_scale = vol_scale;
    n->last_tick = last_tick;
    n->phase = phase;
    n->vib_phase = vib_phase;
    n->cached_osc_freq = cached_osc_freq;
    n->cached_inc = cached_inc;
    n->pos += count;
}

static GB_RAMFN void wave_step(gb_note_t *n, gb_sample_t *dst, uint32_t count) {
    uint32_t sample_rate = n->sample_rate;
    int32_t base_freq = n->base_freq;
    int current_offset = n->current_offset;
    int vol_scale = n->vol_scale;
    int last_tick = n->last_tick;
    const gb_tick_program_t *prog = &n->prog;
    bool use_prog_vol = n->use_prog_vol;
    int32_t last_freq = n->last_freq;
    uint32_t nominal_len = n->nominal_len;
    uint32_t release_tail = n->len - n->nominal_len;
    uint8_t release_kind = n->release_kind;
    int16_t floor_q15 = n->release_floor_q15;
    int32_t exp_q30 = n->release_exp_q30;
    gb_phase_t wave_phase = n->wave_phase;
    gb_phase_t vib_phase = n->vib_phase;

    for (uint32_t k = 0; k < count; k++) {
        uint32_t i = n->pos + k;

        if (prog->enabled) {
            const gb_tick_row_t *row = tick_row_at(prog, &last_tick, i, sample_rate);
            if (row) {
                if (row->offset != current_offset) current_offset = row->offset;
                if (use_prog_vol && row->vol >= 0) vol_scale = row->vol;
            }
        }

        int32_t freq = base_freq;
        if (current_offset != 0) {
            int idx = current_offset + GB_POW2_OFFSET;
            if (idx < 0) idx = 0;
            if (idx >= GB_POW2_COUNT) idx = GB_POW2_COUNT - 1;
            freq = q16_mul(freq, gb_pow2_q16[idx]);
        }
        int32_t base_for_mod = freq; /* un-arpeggiated base (for the vibrato amplitude) */

        /* Portamento: linear (Hz) glide from the previous note's frequency
         * (matches the wave path's linearRampToValueAtTime). Chromium holds the
         * wave playbackRate for each 128-frame quantum, so the glide is stepped
         * at 128-sample boundaries (portAtSec = quantum-start time), not per-sample. */
        if (n->port_speed > 0 && last_freq > 0 && freq != last_freq &&
            (int32_t)i <= n->port_dur_samples) {
            int64_t portAt = (int64_t)(((uint64_t)(n->start_off + i) >> 7) << 7) - (int64_t)n->start_off;
            if (portAt < 0) portAt = 0;
            uint32_t p_q16 = (portAt >= (int64_t)n->port_dur_samples)
                                 ? 65536u
                                 : (uint32_t)(((uint64_t)portAt << 16) / (uint32_t)n->port_dur_samples);
            freq = last_freq + (int32_t)(((int64_t)(freq - last_freq) * (int64_t)p_q16) >> 16);
        }

        /* Vibrato: additive LFO keyed off the un-arpeggiated base frequency,
         * gated by onset delay and duration (BeatBax modulatePitchHz). */
        {
            bool vib_after_delay = (int32_t)i >= n->vib_delay_samples;
            bool vib_within_dur = n->vib_duration_samples < 0 || (int32_t)i < n->vib_duration_samples;
            if (n->vib_depth != 0 && n->vib_rate > 0 && freq > 0 && vib_after_delay && vib_within_dur) {
                int32_t s = gb_lfo_q15(vib_phase, n->vib_waveform);
                int64_t f2 = (int64_t)base_for_mod * base_for_mod;
                int32_t amp = (int32_t)((f2 >> 33) * n->vib_depth);
                freq += (int32_t)(((int64_t)s * amp) >> 15);
                vib_phase += (uint32_t)(((uint64_t)n->vib_rate << 32) / sample_rate);
            }
        }

        /* Wave phase: sample at the current accumulator, then advance. BeatBax
         * integrates playbackRate per sample (readPos += rate), so a portamento
         * glide accumulates correctly (a direct i*freq/sr would assume a constant
         * frequency for the whole note). */
        int16_t v = gb_wave_sample_q15(n->wave_table, n->wave_sum, n->vol_div, wave_phase);
        wave_phase += freq_q16_to_inc(freq, sample_rate);
        int16_t sample = v;
        if (use_prog_vol) {
            sample = (int16_t)((int32_t)v * vol_scale / 15);
        }
        if (n->vol_override >= 0) {
            sample = (int16_t)((int32_t)sample * n->vol_override / 15);
        }
        /* Release tail (5 ms linear, or 20 ms exponential for a bare wave). */
        if (release_kind == GB_RELEASE_LINEAR) {
            if (release_tail > 0 && i >= nominal_len) {
                uint32_t r = i - nominal_len;
                int32_t g = (int32_t)(((release_tail - r) * 32767) / release_tail);
                sample = (int16_t)(((int32_t)sample * g) >> 15);
            }
        } else if (release_kind == GB_RELEASE_EXPONENTIAL) {
            int32_t fade_start = (int32_t)nominal_len - (int32_t)release_tail;
            if ((int32_t)i >= fade_start) {
                int32_t exp_q15 = (int32_t)(((int64_t)exp_q30 * 32767) >> 30);
                int32_t g = floor_q15 + (((32767 - floor_q15) * exp_q15) >> 15);
                sample = (int16_t)(((int32_t)sample * g) >> 15);
                exp_q30 = (int32_t)(((int64_t)exp_q30 * GB_WAVE_EXP_DECAY_Q30) >> 30);
            }
        }
        dst[k] = sample;
    }

    n->current_offset = current_offset;
    n->vol_scale = vol_scale;
    n->last_tick = last_tick;
    n->release_exp_q30 = exp_q30;
    n->wave_phase = wave_phase;
    n->vib_phase = vib_phase;
    n->pos += count;
}

static GB_RAMFN void noise_step(gb_note_t *n, gb_sample_t *dst, uint32_t count) {
    uint32_t sample_rate = n->sample_rate;
    int current_offset = n->current_offset;
    int vol_scale = n->vol_scale;
    int last_tick = n->last_tick;
    const gb_tick_program_t *prog = &n->prog;
    bool use_prog_vol = n->use_prog_vol;
    uint64_t lfsr_inc = n->lfsr_inc;
    uint64_t nphase = n->nphase;
    uint16_t lfsr = n->lfsr;
    bool width7 = n->width7;
    uint32_t nominal_len = n->nominal_len;
    uint32_t release_tail = n->len - n->nominal_len;

    for (uint32_t k = 0; k < count; k++) {
        uint32_t i = n->pos + k;

        if (prog->enabled) {
            const gb_tick_row_t *row = tick_row_at(prog, &last_tick, i, sample_rate);
            if (row) {
                if (row->offset != current_offset) {
                    current_offset = row->offset;
                    lfsr_inc = noise_lfsr_inc_for_offset(n->noise_base_index, current_offset, sample_rate);
                }
                if (use_prog_vol && row->vol >= 0) vol_scale = row->vol;
            }
        }

        nphase += lfsr_inc;
        uint32_t ticks = (uint32_t)(nphase >> 32);
        nphase &= 0xFFFFFFFFull;
        for (uint32_t t = 0; t < ticks; t++) {
            lfsr = gb_noise_lfsr_step(lfsr, width7);
        }

        int16_t bip = gb_noise_sample(lfsr);
        int16_t amp;
        if (use_prog_vol) {
            amp = (int16_t)((vol_scale * 32767) / 15);
        } else {
            if (i < nominal_len) {
                env_advance(n);
            }
            amp = env_volume(n);
        }
        int16_t level = (int16_t)(amp >> 2);
        if (n->vol_override >= 0) {
            level = (int16_t)((int32_t)level * n->vol_override / 15);
        }
        /* Linear release tail past the nominal end (5 ms). */
        if (release_tail > 0 && i >= nominal_len) {
            uint32_t r = i - nominal_len;
            int32_t g = (int32_t)(((release_tail - r) * 32767) / release_tail);
            level = (int16_t)(((int32_t)level * g) >> 15);
        }
        dst[k] = (bip > 0) ? level : (gb_sample_t)(-level);
    }

    n->current_offset = current_offset;
    n->vol_scale = vol_scale;
    n->last_tick = last_tick;
    n->lfsr_inc = lfsr_inc;
    n->nphase = nphase;
    n->lfsr = lfsr;
    n->pos += count;
}

GB_RAMFN void gb_note_begin_desc(gb_note_t *n, const gb_note_desc_t *desc, int midi,
                        uint32_t len, uint32_t sample_rate, int vol_override,
                        gb_channel_state_t *state, int channel,
                        bool join, bool hold, uint32_t start_sample) {
    if (!n || !desc) {
        return;
    }
    memset(n, 0, sizeof(*n));
    n->nominal_len = len;
    n->pos = 0;
    n->sample_rate = sample_rate;
    n->vol_override = vol_override;
    n->chan = state;
    n->channel = channel;
    n->join = join;
    n->hold = hold;
    /* Chromium OscillatorNode startup: a new pulse voice plays the leading
     * sub-quantum samples at 440 Hz. A portamento note (legato) continues the
     * previous oscillator, so BeatBax skips this startup (continuePhase =>
     * chromeDefaultHzSamples = 0). */
    {
        uint32_t q = start_sample & 127u;
        n->chrome_default = (join || desc->port_speed > 0) ? 0u : ((q < 128u - q) ? q : (128u - q));
        n->start_off = q;
    }

    n->vib_depth = desc->vib_depth;
    n->vib_rate = desc->vib_rate;
    n->vib_waveform = desc->vib_waveform;
    n->vib_duration_samples = (desc->vib_duration_ms >= 0)
                                  ? (int32_t)((uint64_t)desc->vib_duration_ms * sample_rate / 1000u)
                                  : -1;
    n->vib_delay_samples = (desc->vib_delay_ms >= 0)
                               ? (int32_t)((uint64_t)desc->vib_delay_ms * sample_rate / 1000u)
                               : 0;
    for (int i = 0; i < 4; i++) n->arp_semis[i] = desc->arp_semis[i];
    n->arp_count = desc->arp_count;
    n->port_speed = desc->port_speed;
    n->bend_semi_q16 = desc->bend_semi_q16;
    n->bend_delay_ms = desc->bend_delay_ms;
    n->bend_time_ms = desc->bend_time_ms;
    memcpy(n->bend_curve, desc->bend_curve, sizeof(n->bend_curve));

    if (n->port_speed > 0) {
        int spd = n->port_speed > 255 ? 255 : n->port_speed;
        n->port_dur_samples = (int32_t)((uint64_t)(256 - spd) * len * 6u / 2560u);
        if (n->port_dur_samples < 1) n->port_dur_samples = 1;
        n->port_q = 65536u / (uint32_t)n->port_dur_samples;
        n->port_r = 65536u % (uint32_t)n->port_dur_samples;
    }
    if (n->bend_semi_q16 != 0) {
        /* BeatBax clamps delay to [0, durSec] and time to (0, durSec − delay]. */
        n->bend_delay_samples = (n->bend_delay_ms >= 0)
                                    ? (int32_t)((uint64_t)n->bend_delay_ms * sample_rate / 1000u)
                                    : (int32_t)len / 2;
        if (n->bend_delay_samples < 0) n->bend_delay_samples = 0;
        if (n->bend_delay_samples > (int32_t)len) n->bend_delay_samples = (int32_t)len;
        n->bend_time_samples = (n->bend_time_ms >= 0)
                                   ? (int32_t)((uint64_t)n->bend_time_ms * sample_rate / 1000u)
                                   : (int32_t)len - n->bend_delay_samples;
        if (n->bend_time_samples < 1) n->bend_time_samples = 1;
        if (n->bend_time_samples > (int32_t)len - n->bend_delay_samples) {
            n->bend_time_samples = (int32_t)len - n->bend_delay_samples;
        }
        if (n->bend_time_samples < 1) n->bend_time_samples = 1;
        n->bend_q = 65536u / (uint32_t)n->bend_time_samples;
        n->bend_r = 65536u % (uint32_t)n->bend_time_samples;
    }

    n->prog = desc->prog;

    switch (desc->voice) {
        case BAX_VOICE_PULSE1:
        case BAX_VOICE_PULSE2:
            n->kind = 0;
            begin_pulse(n, desc, midi, sample_rate, state, channel);
            break;
        case BAX_VOICE_WAVE:
            n->kind = 1;
            begin_wave(n, desc, midi, sample_rate, state, channel);
            break;
        case BAX_VOICE_NOISE:
            n->kind = 2;
            begin_noise(n, desc, sample_rate);
            break;
        default:
            n->kind = 0;
            break;
    }

    /* Release tail (spec-112 parity): pulse/noise fade 5 ms linear; wave fades
     * 5 ms linear when a GB envelope or program volume is active, else a 20 ms
     * exponential (`setTargetAtTime` shape). */
    uint32_t tail = 0;
    uint8_t rk = GB_RELEASE_NONE;
    int16_t floor_q15 = 0;
    switch (desc->voice) {
        case BAX_VOICE_PULSE1:
        case BAX_VOICE_PULSE2:
            /* A held pulse portamento voice does not fade into the next note. */
            if (!hold) {
                tail = gb_ms_to_samples(GB_NOTE_RELEASE_MS, sample_rate);
                rk = GB_RELEASE_LINEAR;
            }
            break;
        case BAX_VOICE_NOISE:
            tail = gb_ms_to_samples(GB_NOTE_RELEASE_MS, sample_rate);
            rk = GB_RELEASE_LINEAR;
            break;
        case BAX_VOICE_WAVE: {
            bool linear = n->use_prog_vol || desc->env_period > 0;
            if (linear) {
                tail = gb_ms_to_samples(GB_NOTE_RELEASE_MS, sample_rate);
                rk = GB_RELEASE_LINEAR;
            } else {
                tail = gb_ms_to_samples(GB_WAVE_TAIL_MS, sample_rate);
                rk = GB_RELEASE_EXPONENTIAL;
                /* floor = 0.0001 / (0.6 / vol_div) in Q15. */
                floor_q15 = (int16_t)((n->vol_div * 546 + 50) / 100);
            }
            break;
        }
        default:
            break;
    }
    n->release_kind = rk;
    n->release_floor_q15 = floor_q15;
    n->release_exp_q30 = 1 << 30;
    n->len = len + tail;
}

GB_RAMFN void gb_note_step(gb_note_t *n, gb_sample_t *dst, uint32_t count) {
    if (!n || !dst || count == 0) {
        return;
    }
    uint32_t remaining = n->len - n->pos;
    if (count > remaining) {
        count = remaining;
    }
    if (count == 0) {
        return;
    }
    switch (n->kind) {
        case 0: pulse_step(n, dst, count); break;
        case 1: wave_step(n, dst, count); break;
        case 2: noise_step(n, dst, count); break;
        default: memset(dst, 0, (size_t)count * sizeof(gb_sample_t)); break;
    }
}
