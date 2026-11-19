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
 * no `float`/`double` in the shared algorithm destined for firmware.
 * Q15 levels and integer phase accumulators are the default approach.
 */

/* Mono PCM output sample, full-scale signed 16-bit (Q0). */
typedef int16_t gb_sample_t;

/*
 * Phase accumulator: 32-bit unsigned fixed-point with 16 fractional bits
 * (16.16). The upper 16 bits are the integer sample position; the lower 16
 * bits accumulate the fractional pitch increment per output sample.
 */
typedef uint32_t gb_phase_t;

#define GB_PHASE_FRAC_BITS 16u
#define GB_PHASE_ONE (1u << GB_PHASE_FRAC_BITS)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_FIXED_H */
