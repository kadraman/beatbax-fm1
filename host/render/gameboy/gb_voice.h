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

#ifndef BAX_GB_VOICE_H
#define BAX_GB_VOICE_H

#include "gb_fixed.h"
#include "gb_period.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Envelope direction, matching the reference `parseEnvelope` ('down'/'up'/'flat'). */
typedef enum {
    GB_ENV_DOWN = 0,
    GB_ENV_UP = 1,
    GB_ENV_FLAT = 2
} gb_env_dir_t;

/*
 * Game Boy volume envelope gain in Q15 (0..32767) at sample index `t` within a
 * note. Linear: level steps ±1 per `period/64` s, clamped [0,15], starting at
 * `initial`. `period == 0` (or FLAT) returns the constant `initial/15`.
 */
int16_t gb_envelope_q15(uint8_t initial, gb_env_dir_t dir, uint8_t period,
                        uint32_t t, uint32_t sample_rate);

/*
 * Duty threshold (Q32) from a duty percentage in tenths (125 / 250 / 500 / 750).
 * The pulse is high for `phase < threshold`.
 */
uint32_t gb_duty_threshold(uint32_t duty_x10);

/* Bipolar pulse square (±1, Q15 full-scale) at the given phase. */
int16_t gb_pulse_bipolar(gb_phase_t phase, uint32_t duty_threshold);

/*
 * Wave sample (Q15) at `phase` (Q32). `table` holds 32 4-bit nibbles; `sum` is
 * their integer sum (for AC coupling, `mean = sum/32`). `vol_div` is the NR32
 * output level divisor: 1 (100%), 2 (50%), 4 (25%), or 0 (mute).
 */
int16_t gb_wave_sample_q15(const uint8_t *table, int sum, int vol_div, gb_phase_t phase);

/* Map a wave instrument volume (0/25/50/100) to its NR32 divisor (0=mute,1,2,4). */
int gb_wave_volume_div(uint8_t volume);

/*
 * Noise channel CH4 LFSR (SameBoy-compatible). `width7` selects the 7-bit
 * polynomial; otherwise 15-bit. Output bit is bit 0.
 */
uint16_t gb_noise_lfsr_step(uint16_t lfsr, bool width7);

/* LFSR state after channel trigger (reset then advance to first bit-0-set). */
uint16_t gb_noise_lfsr_trigger(bool width7);

/* Bipolar noise sample (±1, Q15) from the LFSR output bit. */
int16_t gb_noise_sample(uint16_t lfsr);

/* hUGEDriver get_note_poly: UGE note index → NR43 clock byte (bits 6-4 shift, 2-0 divisor). */
uint8_t gb_noise_note_poly(int note_index);

/* hUGETracker display note ("C-6") → UGE note index (0..72), or -1 if invalid. */
int gb_huge_note_to_index(const char *note_name);

/* LFSR update rate (Hz) from NR43 shift/divisor code (rounds). */
uint32_t gb_noise_lfsr_hz(uint32_t shift, uint32_t divisor);

/*
 * Exact LFSR phase increment (Q32, may exceed 2^32) for a sample rate, computed
 * directly from shift/divisor to avoid integer-Hz rounding:
 *   lfsrHz = GB_CLOCK / (ratio * 2^shift); inc = lfsrHz * 2^32 / sample_rate.
 */
uint64_t gb_noise_lfsr_inc(uint32_t shift, uint32_t divisor, uint32_t sample_rate);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_VOICE_H */
