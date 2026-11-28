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

#include "gb_render.h"
#include <string.h>

void gb_render_init(gb_render_state_t *st, const bax_song_t *song, uint32_t sample_rate) {
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
    st->song = song;
    st->sample_rate = sample_rate;
}

void gb_render_block(gb_render_state_t *st, gb_sample_t *out, uint32_t frame_count) {
    if (!st || !out || frame_count == 0) {
        return;
    }

    /*
     * Phase 0: silence. The voice cores and scheduler/expansion tick land in
     * Phase 1 (T010-T014); until then the renderer outputs silence while
     * remaining deterministic and allocation-free in the hot path.
     */
    memset(out, 0, (size_t)frame_count * sizeof(gb_sample_t));
    st->frame += frame_count;
}

void gb_render_pulse(gb_sample_t *out, uint32_t duration,
                     uint32_t inc, uint32_t duty_thr, gb_phase_t *phase,
                     uint8_t env_initial, gb_env_dir_t env_dir, uint8_t env_period,
                     uint32_t sample_rate) {
    if (!out || !phase) {
        return;
    }
    for (uint32_t i = 0; i < duration; i++) {
        *phase += inc;
        int16_t bip = gb_pulse_bipolar(*phase, duty_thr);
        int16_t env = gb_envelope_q15(env_initial, env_dir, env_period, i, duration, sample_rate);
        int16_t amp = (int16_t)(env >> 2); /* * PULSE_OUTPUT_GAIN (0.25) */
        out[i] = (bip > 0) ? amp : (gb_sample_t)(-amp);
    }
}

void gb_render_wave(gb_sample_t *out, uint32_t duration,
                    uint32_t inc, const uint8_t *table, int sum, int vol_div,
                    uint32_t sample_rate) {
    (void)sample_rate;
    if (!out) {
        return;
    }
    gb_phase_t phase = 0;
    for (uint32_t i = 0; i < duration; i++) {
        phase += inc;
        out[i] = gb_wave_sample_q15(table, sum, vol_div, phase);
    }
}

void gb_render_noise(gb_sample_t *out, uint32_t duration,
                     uint32_t lfsr_hz, bool width7,
                     uint8_t env_initial, gb_env_dir_t env_dir, uint8_t env_period,
                     uint32_t sample_rate) {
    if (!out) {
        return;
    }
    uint64_t lfsr_inc = ((uint64_t)lfsr_hz << 32) / (sample_rate ? sample_rate : 1u);
    uint64_t phase = 0;
    uint16_t lfsr = gb_noise_lfsr_trigger(width7);
    for (uint32_t i = 0; i < duration; i++) {
        phase += lfsr_inc;
        uint32_t ticks = (uint32_t)(phase >> 32);
        phase &= 0xFFFFFFFFull;
        for (uint32_t t = 0; t < ticks; t++) {
            lfsr = gb_noise_lfsr_step(lfsr, width7);
        }
        int16_t bip = gb_noise_sample(lfsr);
        int16_t env = gb_envelope_q15(env_initial, env_dir, env_period, i, duration, sample_rate);
        int16_t amp = (int16_t)(env >> 2); /* * NOISE_OUTPUT_GAIN (0.25) */
        out[i] = (bip > 0) ? amp : (gb_sample_t)(-amp);
    }
}
