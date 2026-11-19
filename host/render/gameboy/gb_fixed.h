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

#ifndef BAX_GB_FIXED_H
#define BAX_GB_FIXED_H

#include <stdint.h>

/*
 * Fixed-point types for the Game Boy renderer.
 *
 * The first audio path is fixed-point per `specs/global/audio.md`:
 * no `float`/`double` in the shared sample-generation loop destined for firmware.
 *
 * Phase uses a full-range 32-bit accumulator: the value `0xFFFFFFFF` represents one
 * complete output cycle (phase 1.0 in the reference float renderer). Increments are
 * unsigned and wrap naturally on overflow, which is exactly the reference
 * `phase += freq / sr; phase -= floor(phase)` behavior.
 */

/* Mono PCM output sample, full-scale signed 16-bit. */
typedef int16_t gb_sample_t;

/* Phase accumulator: full-range fraction, one wave cycle == 2^32. */
typedef uint32_t gb_phase_t;

/* Phase increment per output sample, same units as gb_phase_t. */
typedef uint32_t gb_phase_inc_t;

/* Q15 signed fixed-point for normalized amplitudes in [-1, 1). */
typedef int16_t gb_q15_t;

/* One full phase cycle (0x1.0000.0000), expressed for the phase accumulator. */
#define GB_PHASE_ONE_FRAC_BITS 32u

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_FIXED_H */
