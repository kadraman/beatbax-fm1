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
 * Host-side lowering of the 002 song model into float-free note descriptors.
 *
 * Everything here touches the song model (and a little float) and is therefore
 * host-only; the firmware never compiles it. The firmware instead reads
 * descriptors that this code serialized into the compiled song blob.
 */

#include "gb_note_desc.h"

#include "gb_voice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Resolved named-effect parameters (mirrors the pre-refactor gb_fx_t). */
typedef struct {
    int vib_depth;
    int vib_rate;
    uint8_t vib_waveform;
    int vib_duration_ms; /* -1 = unset */
    int vib_delay_ms;    /* -1 = unset */
    int arp_semis[4];
    int arp_count;
    int port_speed;
    int32_t bend_semi_q16; /* bend semitones in Q16 (fractional allowed) */
    int bend_delay_ms; /* -1 = default (50% of note) */
    int bend_time_ms;  /* -1 = default (remaining) */
    char bend_curve[8];
} gb_fx_t;

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

/* Parse arp offsets from a `bax_effect_t`. Root (0) is prepended. */
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
    /* Fractional semitones allowed (e.g. bend:+0.5); store as Q16. */
    fx->bend_semi_q16 = (int32_t)(atof(p) * 65536.0);
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

static uint8_t vib_waveform_id(const char *w) {
    if (strcmp(w, "square") == 0) return GB_VIB_SQUARE;
    if (strcmp(w, "saw") == 0 || strcmp(w, "sawtooth") == 0) return GB_VIB_SAW;
    if (strcmp(w, "triangle") == 0 || strcmp(w, "tri") == 0) return GB_VIB_TRIANGLE;
    return GB_VIB_SINE; /* sine (default) */
}

static void resolve_fx(const bax_song_t *song, const char *effect_name, gb_fx_t *fx) {
    memset(fx, 0, sizeof(*fx));
    fx->vib_duration_ms = -1;
    fx->vib_delay_ms = -1;
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
            fx->vib_waveform = vib_waveform_id(eff->params.vib.waveform);
            /* durationRows / delayRows -> ms; tickMs = 15000 / bpm (4 ticks per beat).
             * Float (host-only lowering) keeps the fractional tick duration BeatBax uses. */
            if (song->bpm > 0) {
                double tick_ms = 15000.0 / (double)song->bpm;
                if (eff->params.vib.duration_rows > 0) {
                    fx->vib_duration_ms = (int)((double)eff->params.vib.duration_rows * tick_ms + 0.5);
                }
                if (eff->params.vib.delay_rows > 0) {
                    fx->vib_delay_ms = (int)((double)eff->params.vib.delay_rows * tick_ms + 0.5);
                }
            }
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

void gb_note_desc_from_song(gb_note_desc_t *desc, const bax_song_t *song,
                            const bax_instrument_t *inst, const char *effect_name) {
    if (!desc || !inst) {
        return;
    }
    memset(desc, 0, sizeof(*desc));

    desc->voice = (uint8_t)inst->type;
    desc->duty_x10 = (uint32_t)(inst->duty * 10.0f + 0.5f);
    desc->env_initial = inst->env.present ? inst->env.level : 15;
    desc->env_period = inst->env.present ? inst->env.period : 0;
    desc->env_dir = inst->env.present ? (uint8_t)inst->env.direction : GB_ENV_FLAT;
    desc->sweep_present = inst->sweep.present ? 1 : 0;
    desc->sweep_time = inst->sweep.time;
    desc->sweep_dir = (uint8_t)inst->sweep.direction;
    desc->sweep_shift = inst->sweep.shift;

    build_wave_table(inst, desc->wave_table, &desc->wave_sum);
    desc->wave_volume = inst->wave_volume;

    desc->noise_width = inst->noise_width;
    desc->noise_note_index = inst->uge_note[0] ? gb_huge_note_to_index(inst->uge_note) : 36;
    if (desc->noise_note_index < 0) {
        desc->noise_note_index = 36;
    }
    uint32_t shift, divisor;
    resolve_noise_clock(inst, &shift, &divisor);
    desc->noise_shift = (uint8_t)shift;
    desc->noise_divisor = (uint8_t)divisor;

    gb_tick_program_build(inst, &desc->prog);

    gb_fx_t fx;
    resolve_fx(song, effect_name, &fx);
    desc->vib_depth = fx.vib_depth;
    desc->vib_rate = fx.vib_rate;
    desc->vib_waveform = fx.vib_waveform;
    desc->vib_duration_ms = fx.vib_duration_ms;
    desc->vib_delay_ms = fx.vib_delay_ms;
    for (int i = 0; i < 4; i++) {
        desc->arp_semis[i] = fx.arp_semis[i];
    }
    desc->arp_count = fx.arp_count;
    desc->port_speed = fx.port_speed;
    desc->bend_semi_q16 = fx.bend_semi_q16;
    desc->bend_delay_ms = fx.bend_delay_ms;
    desc->bend_time_ms = fx.bend_time_ms;
    memcpy(desc->bend_curve, fx.bend_curve, sizeof(desc->bend_curve));
}

void gb_note_begin(gb_note_t *n, const bax_song_t *song, int midi,
                   const bax_instrument_t *inst, const char *effect_name,
                   uint32_t len, uint32_t sample_rate, int vol_override,
                   gb_channel_state_t *state, int channel,
                   bool join, bool hold, uint32_t start_sample) {
    gb_note_desc_t desc;
    gb_note_desc_from_song(&desc, song, inst, effect_name);
    gb_note_begin_desc(n, &desc, midi, len, sample_rate, vol_override, state, channel, join, hold, start_sample);
}

uint32_t gb_render_note_full(gb_sample_t *dst, uint32_t len,
                             const bax_song_t *song, int midi,
                             const bax_instrument_t *inst, const char *effect_name,
                             uint32_t sample_rate, int vol_override,
                             gb_channel_state_t *state, int channel,
                             bool join, bool hold, uint32_t start_sample) {
    if (!dst || !inst || len == 0) {
        return 0;
    }
    gb_note_desc_t desc;
    gb_note_desc_from_song(&desc, song, inst, effect_name);
    gb_note_t n;
    gb_note_begin_desc(&n, &desc, midi, len, sample_rate, vol_override, state, channel, join, hold, start_sample);
    gb_note_step(&n, dst, n.len);
    return n.len;
}
