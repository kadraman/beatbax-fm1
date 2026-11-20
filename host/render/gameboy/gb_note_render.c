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

#include "gb_note_render.h"

#include "gb_macro.h"
#include "gb_period.h"
#include "gb_tables.h"
#include "gb_voice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Q16 fixed-point helpers. */
static int32_t q16_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

/* 2^(semi_q16 / 2^16 / 12) in Q16, direct lookup in the fine table. */
static int32_t pow2_semi_q16(int32_t semi_q16) {
    int32_t idx = (semi_q16 >> 8) + GB_POW2_FINE_OFFSET;
    if (idx < 0) idx = 0;
    if (idx >= GB_POW2_FINE_COUNT) idx = GB_POW2_FINE_COUNT - 1;
    return gb_pow2_fine_q16[idx];
}

/* Tile the wave table (16 or 32 nibbles) to 32 and return the integer sum. */
static void build_wave_table(const bax_instrument_t *inst, uint8_t table[32], int *sum) {
    size_t count = inst->wave.count;
    int s = 0;
    for (int i = 0; i < 32; i++) {
        uint8_t v = (count == 0) ? 0 : inst->wave.samples[i % count];
        table[i] = v;
        s += v;
    }
    *sum = s;
}

static void resolve_noise_clock(const bax_instrument_t *inst, uint32_t *shift, uint32_t *divisor) {
    *shift = 4;   /* DEFAULT_NOISE_SHIFT */
    *divisor = 3; /* DEFAULT_NOISE_DIVISOR */
    if (inst->uge_note[0]) {
        int idx = gb_huge_note_to_index(inst->uge_note);
        if (idx >= 0) {
            uint8_t poly = gb_noise_note_poly(idx);
            *shift = (poly >> 4) & 7u;
            *divisor = poly & 7u;
        }
    }
}

static uint32_t freq_q16_to_inc(int32_t freq_q16, uint32_t sample_rate) {
    if (freq_q16 <= 0 || sample_rate == 0) {
        return 0;
    }
    uint64_t inc = ((uint64_t)freq_q16 << 16) / sample_rate;
    if (inc > 0xFFFFFFFFull) {
        inc = 0xFFFFFFFFull;
    }
    return (uint32_t)inc;
}

static uint32_t duty_index_to_x10(int idx) {
    switch (idx) {
        case 0: return 125;
        case 1: return 250;
        case 3: return 750;
        default: return 500;
    }
}

/* Resolved named-effect parameters. */
typedef struct {
    int vib_depth;
    int vib_rate;
    int arp_semis[4];
    int arp_count;
    int port_speed;
    int bend_semi;
    int bend_delay_ms; /* -1 = default (50% of note) */
    int bend_time_ms;  /* -1 = default (remaining) */
    char bend_curve[8];
} gb_fx_t;

/* Parse arp offsets from a `bax_effect_t`. Root (0) is prepended, matching
 * BeatBax `arpCycleOffsets`. */
static void resolve_arp(const bax_effect_t *eff, gb_fx_t *fx) {
    fx->arp_semis[0] = 0; /* root */
    fx->arp_count = 1;
    const char *p = eff->raw_expr;
    const char *colon = strchr(p, ':');
    if (!colon) {
        return;
    }
    p = colon + 1;
    while (*p && fx->arp_count < 4) {
        const char *comma = strchr(p, ',');
        int semi = (int)strtol(p, NULL, 10);
        fx->arp_semis[fx->arp_count++] = semi;
        if (!comma) {
            break;
        }
        p = comma + 1;
    }
}

/* Parse "bend:semitones[,curve[,delay[,time]]]" (delay/time in seconds). */
static void resolve_bend(const bax_effect_t *eff, gb_fx_t *fx) {
    fx->bend_delay_ms = -1;
    fx->bend_time_ms = -1;
    snprintf(fx->bend_curve, sizeof(fx->bend_curve), "linear");
    const char *colon = strchr(eff->raw_expr, ':');
    if (!colon) {
        return;
    }
    const char *p = colon + 1;
    fx->bend_semi = (int)strtol(p, NULL, 10);
    const char *c1 = strchr(p, ',');
    if (!c1) {
        return;
    }
    p = c1 + 1;
    const char *c2 = strchr(p, ',');
    size_t n = c2 ? (size_t)(c2 - p) : strlen(p);
    if (n >= sizeof(fx->bend_curve)) {
        n = sizeof(fx->bend_curve) - 1;
    }
    memcpy(fx->bend_curve, p, n);
    fx->bend_curve[n] = '\0';
    if (c2) {
        p = c2 + 1;
        fx->bend_delay_ms = (int)(atof(p) * 1000.0);
        const char *c3 = strchr(p, ',');
        if (c3) {
            fx->bend_time_ms = (int)(atof(c3 + 1) * 1000.0);
        }
    }
}

static void resolve_fx(const bax_song_t *song, const char *effect_name, gb_fx_t *fx) {
    memset(fx, 0, sizeof(*fx));
    if (!effect_name || !effect_name[0]) {
        return;
    }
    const bax_effect_t *eff = bax_song_find_effect(song, effect_name);
    if (!eff) {
        return;
    }
    switch (eff->type) {
        case BAX_EFFECT_VIBRATO:
            /* BeatBax renderer: params[0] = depth (amplitude), params[1] = rate (Hz). */
            fx->vib_depth = eff->params.vib.speed;
            fx->vib_rate = eff->params.vib.depth;
            break;
        case BAX_EFFECT_ARPEGGIO:
            resolve_arp(eff, fx);
            break;
        case BAX_EFFECT_PORTAMENTO:
            fx->port_speed = eff->params.port.speed;
            break;
        case BAX_EFFECT_BEND:
            resolve_bend(eff, fx);
            break;
        default:
            break;
    }
}

void gb_channel_state_init(gb_channel_state_t *st) {
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
}

/* True if the tick program writes volume anywhere. */
static bool prog_has_volume(const gb_tick_program_t *p) {
    for (int i = 0; i < p->count; i++) {
        if (p->rows[i].vol >= 0) {
            return true;
        }
    }
    return false;
}

/* Advance the tick program and return the active row (or NULL). */
static const gb_tick_row_t *tick_row_at(const gb_tick_program_t *p, int *last_tick,
                                        uint32_t i, uint32_t sample_rate) {
    int tick = (int)(((uint64_t)i * GB_TICK_SEC_DEN) / sample_rate);
    if (tick == *last_tick) {
        return NULL;
    }
    *last_tick = tick;
    return gb_tick_program_row(p, tick);
}

static void render_pulse_full(gb_sample_t *dst, uint32_t len,
                              const bax_song_t *song, int midi,
                              const bax_instrument_t *inst, const gb_fx_t *fx,
                              uint32_t sample_rate, gb_channel_state_t *state, int channel) {
    (void)song;
    uint32_t current_reg = gb_midi_to_period(midi);
    uint32_t duty_thr = gb_duty_threshold((uint32_t)(inst->duty * 10.0f + 0.5f));

    /* Frequency sweep (NR10, pulse 1 only). Skipped when a tick program owns pitch. */
    bool has_sweep = inst->sweep.present && inst->sweep.time > 0;
    uint32_t sweep_interval = has_sweep
                                  ? (uint32_t)((uint64_t)inst->sweep.time * sample_rate / 128u)
                                  : 0;

    gb_tick_program_t prog;
    gb_tick_program_build(inst, &prog);
    bool use_prog_vol = prog.enabled && prog_has_volume(&prog);
    if (prog.enabled) {
        has_sweep = false;
    }

    uint8_t env_initial = inst->env.present ? inst->env.level : 15;
    uint8_t env_period = inst->env.present ? inst->env.period : 0;
    gb_env_dir_t env_dir = inst->env.present ? (gb_env_dir_t)inst->env.direction : GB_ENV_FLAT;

    gb_phase_t *phase = &state->pulse_phase[channel];
    gb_phase_t *vib_phase = &state->vib_phase[channel];
    int32_t last_freq = state->last_freq_q16[channel];

    int current_offset = 0;
    int vol_scale = 15;
    int last_tick = -1;
    if (prog.enabled) {
        const gb_tick_row_t *r0 = gb_tick_program_row(&prog, 0);
        if (r0) {
            current_offset = r0->offset;
            if (use_prog_vol && r0->vol >= 0) vol_scale = r0->vol;
            if (r0->duty >= 0) duty_thr = gb_duty_threshold(duty_index_to_x10(r0->duty));
        }
    }

    int32_t port_dur_samples = 0;
    if (fx->port_speed > 0) {
        int spd = fx->port_speed > 255 ? 255 : fx->port_speed;
        port_dur_samples = (int32_t)((uint64_t)(256 - spd) * len * 6u / 2560u);
        if (port_dur_samples < 1) port_dur_samples = 1;
    }

    int32_t bend_delay_samples = 0;
    int32_t bend_time_samples = 0;
    if (fx->bend_semi != 0) {
        bend_delay_samples = (fx->bend_delay_ms >= 0)
                                 ? (int32_t)((uint64_t)fx->bend_delay_ms * sample_rate / 1000u)
                                 : (int32_t)len / 2;
        bend_time_samples = (fx->bend_time_ms >= 0)
                                ? (int32_t)((uint64_t)fx->bend_time_ms * sample_rate / 1000u)
                                : (int32_t)len - bend_delay_samples;
        if (bend_time_samples < 1) bend_time_samples = 1;
    }

    for (uint32_t i = 0; i < len; i++) {
        if (prog.enabled) {
            const gb_tick_row_t *row = tick_row_at(&prog, &last_tick, i, sample_rate);
            if (row) {
                if (row->offset != current_offset) {
                    current_offset = row->offset;
                }
                if (use_prog_vol && row->vol >= 0) {
                    vol_scale = row->vol;
                }
                if (row->duty >= 0) {
                    duty_thr = gb_duty_threshold(duty_index_to_x10(row->duty));
                }
            }
        }

        /* Apply frequency sweep (NR10 register step) every sweep_interval samples. */
        if (has_sweep && sweep_interval > 0 && i > 0 && i % sweep_interval == 0) {
            uint32_t delta = current_reg >> (inst->sweep.shift & 7u);
            if (inst->sweep.direction == BAX_SWEEP_DIR_UP) {
                current_reg += delta;
            } else if (current_reg > delta) {
                current_reg -= delta;
            } else {
                current_reg = 0;
            }
        }
        int32_t base_freq = (current_reg > 2047u) ? 0 : gb_period_to_freq_q16(current_reg);
        int32_t freq = base_freq;
        if (current_offset != 0) {
            int idx = current_offset + GB_POW2_OFFSET;
            if (idx < 0) idx = 0;
            if (idx >= GB_POW2_COUNT) idx = GB_POW2_COUNT - 1;
            freq = q16_mul(freq, gb_pow2_q16[idx]);
        }

        /* Portamento: smoothstep glide from the previous note's frequency. */
        if (fx->port_speed > 0 && last_freq > 0 && freq != last_freq &&
            (int32_t)i <= port_dur_samples) {
            uint32_t progress_q16 = (uint32_t)(((uint64_t)i << 16) / (uint64_t)port_dur_samples);
            if (progress_q16 > 65536) progress_q16 = 65536;
            uint32_t p = progress_q16;
            uint32_t eased = (uint32_t)(((uint64_t)p * p >> 16) * (3u * 65536u - 2u * p) >> 16);
            freq = last_freq + (int32_t)(((int64_t)(freq - last_freq) * (int64_t)eased) >> 16);
        }

        /* Vibrato: sine LFO, deviation = depth * freq^2 / 131072. */
        if (fx->vib_depth != 0 && fx->vib_rate > 0 && freq > 0) {
            int sin_idx = (int)((*vib_phase >> 24) & 0xFF);
            int16_t s = gb_sin_q15[sin_idx];
            int64_t f2 = (int64_t)freq * freq;
            int32_t amp = (int32_t)((f2 >> 33) * fx->vib_depth);
            freq += (int32_t)(((int64_t)s * amp) >> 15);
            *vib_phase += (uint32_t)(((uint64_t)fx->vib_rate << 32) / sample_rate);
        }

        /* Arpeggio: cycle offsets at 60 Hz. */
        if (fx->arp_count > 0 && freq > 0) {
            int frame = (int)(((uint64_t)i * 60u) / sample_rate);
            int semi = fx->arp_semis[frame % fx->arp_count];
            if (semi != 0) {
                int idx = semi + GB_POW2_OFFSET;
                if (idx < 0) idx = 0;
                if (idx >= GB_POW2_COUNT) idx = GB_POW2_COUNT - 1;
                freq = q16_mul(freq, gb_pow2_q16[idx]);
            }
        }

        /* Pitch bend: freq *= 2^(semi * progress / 12) with curve shaping. */
        if (fx->bend_semi != 0 && bend_time_samples > 0 &&
            (int32_t)i >= bend_delay_samples &&
            (int32_t)i <= bend_delay_samples + bend_time_samples) {
            int32_t t_rel = (int32_t)i - bend_delay_samples;
            uint32_t p = (uint32_t)(((uint64_t)t_rel << 16) / (uint64_t)bend_time_samples);
            if (p > 65536) p = 65536;
            if (strcmp(fx->bend_curve, "exp") == 0 || strcmp(fx->bend_curve, "exponential") == 0) {
                p = (uint32_t)(((uint64_t)p * p) >> 16);
            } else if (strcmp(fx->bend_curve, "log") == 0 || strcmp(fx->bend_curve, "logarithmic") == 0) {
                uint32_t q = 65536u - p;
                p = 65536u - (uint32_t)(((uint64_t)q * q) >> 16);
            } else if (strcmp(fx->bend_curve, "sine") == 0 || strcmp(fx->bend_curve, "sin") == 0) {
                uint32_t sp = (uint32_t)(((uint64_t)p * p >> 16) * (3u * 65536u - 2u * p) >> 16);
                p = sp;
            }
            int32_t semi_q16 = (int32_t)((int64_t)fx->bend_semi * (int64_t)p);
            freq = q16_mul(freq, pow2_semi_q16(semi_q16));
        }

        uint32_t inc = freq_q16_to_inc(freq, sample_rate);
        if (inc == 0) {
            dst[i] = 0;
            continue;
        }
        *phase += inc;
        int16_t bip = gb_pulse_bipolar(*phase, duty_thr);

        int16_t amp;
        if (use_prog_vol) {
            amp = (int16_t)((vol_scale * 32767) / 15);
        } else {
            amp = gb_envelope_q15(env_initial, env_dir, env_period, i, sample_rate);
        }
        int16_t level = (int16_t)(amp >> 2); /* * 0.25 */
        dst[i] = (bip > 0) ? level : (gb_sample_t)(-level);
    }

    state->last_freq_q16[channel] = gb_period_to_freq_q16(gb_midi_to_period(midi));
}

static void render_wave_full(gb_sample_t *dst, uint32_t len,
                             const bax_song_t *song, int midi,
                             const bax_instrument_t *inst, const gb_fx_t *fx,
                             uint32_t sample_rate, gb_channel_state_t *state, int channel) {
    (void)song;
    uint32_t reg = gb_midi_to_period(midi);
    int32_t base_freq = gb_period_to_freq_q16(reg);

    uint8_t table[32];
    int sum;
    build_wave_table(inst, table, &sum);
    int vol_div = gb_wave_volume_div(inst->wave_volume);

    gb_tick_program_t prog;
    gb_tick_program_build(inst, &prog);
    bool use_prog_vol = prog.enabled && prog_has_volume(&prog);

    int current_offset = 0;
    int vol_scale = 15;
    int last_tick = -1;
    if (prog.enabled) {
        const gb_tick_row_t *r0 = gb_tick_program_row(&prog, 0);
        if (r0) {
            current_offset = r0->offset;
            if (use_prog_vol && r0->vol >= 0) vol_scale = r0->vol;
        }
    }

    int32_t last_freq = state->last_freq_q16[channel];
    int32_t port_dur_samples = 0;
    if (fx->port_speed > 0) {
        int spd = fx->port_speed > 255 ? 255 : fx->port_speed;
        port_dur_samples = (int32_t)((uint64_t)(256 - spd) * len * 6u / 2560u);
        if (port_dur_samples < 1) port_dur_samples = 1;
    }

    gb_phase_t phase = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (prog.enabled) {
            const gb_tick_row_t *row = tick_row_at(&prog, &last_tick, i, sample_rate);
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

        if (fx->port_speed > 0 && last_freq > 0 && freq != last_freq &&
            (int32_t)i <= port_dur_samples) {
            uint32_t progress_q16 = (uint32_t)(((uint64_t)i << 16) / (uint64_t)port_dur_samples);
            if (progress_q16 > 65536) progress_q16 = 65536;
            uint32_t p = progress_q16;
            uint32_t eased = (uint32_t)(((uint64_t)p * p >> 16) * (3u * 65536u - 2u * p) >> 16);
            freq = last_freq + (int32_t)(((int64_t)(freq - last_freq) * (int64_t)eased) >> 16);
        }

        /* BeatBax computes wave phase directly: phase = (t * effFreq) % 1. */
        uint64_t ph = (uint64_t)i * (uint64_t)freq * 65536ull / (uint64_t)sample_rate;
        phase = (gb_phase_t)(ph & 0xFFFFFFFFull);
        int16_t v = gb_wave_sample_q15(table, sum, vol_div, phase);
        int16_t sample = v;
        if (use_prog_vol) {
            sample = (int16_t)((int32_t)v * vol_scale / 15);
        }
        dst[i] = sample;
    }

    state->last_freq_q16[channel] = base_freq;
}

static void render_noise_full(gb_sample_t *dst, uint32_t len,
                              const bax_song_t *song, int midi,
                              const bax_instrument_t *inst, const gb_fx_t *fx,
                              uint32_t sample_rate, gb_channel_state_t *state, int channel) {
    (void)song;
    (void)midi;
    (void)fx;
    (void)state;
    (void)channel;

    int base_index = inst->uge_note[0] ? gb_huge_note_to_index(inst->uge_note) : 36;
    if (base_index < 0) base_index = 36;

    uint32_t shift, divisor;
    resolve_noise_clock(inst, &shift, &divisor);
    bool width7 = (inst->noise_width == 7);

    uint8_t env_initial = inst->env.present ? inst->env.level : 15;
    uint8_t env_period = inst->env.present ? inst->env.period : 0;
    gb_env_dir_t env_dir = inst->env.present ? (gb_env_dir_t)inst->env.direction : GB_ENV_FLAT;

    gb_tick_program_t prog;
    gb_tick_program_build(inst, &prog);
    bool use_prog_vol = prog.enabled && prog_has_volume(&prog);

    int current_offset = 0;
    int vol_scale = 15;
    int last_tick = -1;
    if (prog.enabled) {
        const gb_tick_row_t *r0 = gb_tick_program_row(&prog, 0);
        if (r0) {
            current_offset = r0->offset;
            if (use_prog_vol && r0->vol >= 0) vol_scale = r0->vol;
        }
    }

    uint64_t lfsr_inc = gb_noise_lfsr_inc(shift, divisor, sample_rate);
    uint64_t nphase = 0;
    uint16_t lfsr = gb_noise_lfsr_trigger(width7);

    for (uint32_t i = 0; i < len; i++) {
        if (prog.enabled) {
            const gb_tick_row_t *row = tick_row_at(&prog, &last_tick, i, sample_rate);
            if (row) {
                if (row->offset != current_offset) {
                    current_offset = row->offset;
                    int note_index = base_index + current_offset;
                    if (note_index < 0) note_index = 0;
                    if (note_index > 72) note_index = 72;
                    uint8_t poly = gb_noise_note_poly(note_index);
                    uint32_t s2 = (poly >> 4) & 7u;
                    uint32_t d2 = poly & 7u;
                    lfsr_inc = gb_noise_lfsr_inc(s2, d2, sample_rate);
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
        int16_t amp = use_prog_vol
                          ? (int16_t)((vol_scale * 32767) / 15)
                          : gb_envelope_q15(env_initial, env_dir, env_period, i, sample_rate);
        int16_t level = (int16_t)(amp >> 2);
        dst[i] = (bip > 0) ? level : (gb_sample_t)(-level);
    }
}

void gb_render_note_full(gb_sample_t *dst, uint32_t len,
                         const bax_song_t *song, int midi,
                         const bax_instrument_t *inst, const char *effect_name,
                         uint32_t sample_rate,
                         gb_channel_state_t *state, int channel) {
    if (!dst || !inst || len == 0) {
        return;
    }
    gb_fx_t fx;
    resolve_fx(song, effect_name, &fx);

    switch (inst->type) {
        case BAX_VOICE_PULSE1:
        case BAX_VOICE_PULSE2:
            render_pulse_full(dst, len, song, midi, inst, &fx, sample_rate, state, channel);
            break;
        case BAX_VOICE_WAVE:
            render_wave_full(dst, len, song, midi, inst, &fx, sample_rate, state, channel);
            break;
        case BAX_VOICE_NOISE:
            render_noise_full(dst, len, song, midi, inst, &fx, sample_rate, state, channel);
            break;
        default:
            break;
    }
}
