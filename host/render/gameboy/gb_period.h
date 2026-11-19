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

#ifndef BAX_GB_PERIOD_H
#define BAX_GB_PERIOD_H

#include "gb_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Game Boy master clock, 4.194304 MHz. */
#define GB_CLOCK 4194304u

/* Number of entries in the hUGETracker period table (6 octaves, C2..B7). */
#define GB_PERIOD_TABLE_LEN 72u

/* hUGETracker Game Boy period table. Index 0 = C2 (MIDI 36). */
extern const uint16_t gb_period_table[GB_PERIOD_TABLE_LEN];

/*
 * Map a MIDI note number (0..127) to a Game Boy frequency period register
 * (0..2047). Notes in [36, 108) use the hUGETracker period table; notes below
 * C2 clamp to period 0 (64 Hz) and notes above B7 clamp to 2047, matching the
 * reference renderer's `registerFromFreq(midiToFreq(midi))` behavior for the
 * Game Boy subset.
 */
uint32_t gb_midi_to_period(int midi);

/*
 * Phase increment (Q32) for a period register at the given sample rate.
 * Equivalent to round(freq / sample_rate * 2^32) where
 * freq = 131072 / (2048 - reg). Saturates to UINT32_MAX for inaudibly high
 * registers.
 */
uint32_t gb_period_to_inc(uint32_t reg, uint32_t sample_rate);

/*
 * Frequency (Hz) for a period register, rounded: freq = 131072 / (2048 - reg).
 */
uint32_t gb_period_to_freq_hz(uint32_t reg);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_PERIOD_H */
