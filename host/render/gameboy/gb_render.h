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

#ifndef BAX_GB_RENDER_H
#define BAX_GB_RENDER_H

#include "gb_fixed.h"
#include "gb_voice.h"
#include "song_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fixed-point Game Boy (DMG-01) renderer.
 *
 * Consumes the feature 002 song model (`bax_song_t`). The renderer is
 * rate-parameterized: the same shared algorithm runs at 44100 Hz for host
 * CLI comparison and at the device codec rate (44117.6 Hz) in feature 004.
 *
 * Phase 0 (feature 003): the skeleton links against the song model and
 * renders silence deterministically. Voice cores (pulse 1/2, wave, noise),
 * the scheduler/expansion tick, and supported effects land in Phase 1.
 */

/* Comparison rate: matches the BeatBax CLI `wav` exporter default (44100 Hz). */
#define GB_RENDER_COMPARE_RATE 44100u

/*
 * Block size mirroring the device callback shape (256 frames), so the same
 * API can be driven by the FM-1 codec path in feature 004.
 */
#define GB_RENDER_BLOCK_FRAMES 256u

typedef struct gb_render_state {
    const bax_song_t *song; /* borrowed reference; MUST outlive the render state */
    uint32_t sample_rate;   /* samples per second */
    uint64_t frame;         /* monotonic output frame counter (determinism/debug) */
    /*
     * Per-voice state (pulse 1/2, wave, noise), scheduler position, and
     * mixer state are added here in Phase 1 (T010-T014).
     */
} gb_render_state_t;

/* Bind a render state to a parsed song at the given sample rate. */
void gb_render_init(gb_render_state_t *st, const bax_song_t *song, uint32_t sample_rate);

/*
 * Fill `out[0 .. frame_count)` with mono PCM.
 *
 * Phase 0 behavior: emits silence and advances `st->frame`. Output is
 * deterministic for identical initial state and `frame_count`.
 */
void gb_render_block(gb_render_state_t *st, gb_sample_t *out, uint32_t frame_count);

/*
 * Per-note render primitives (mono). These are the fixed-point equivalents of
 * BeatBax `renderPulse` / `renderWave` / `renderNoise` WITHOUT effects/macros
 * (those land in Phase 1 T014). Each writes `duration` samples into `out`.
 */

/*
 * Pulse note. `phase` is the persistent per-channel phase accumulator
 * (continued across notes, matching `channelPhaseState`). `inc` is the Q32
 * phase increment, `duty_thr` the Q32 duty threshold.
 */
void gb_render_pulse(gb_sample_t *out, uint32_t duration,
                     uint32_t inc, uint32_t duty_thr, gb_phase_t *phase,
                     uint8_t env_initial, gb_env_dir_t env_dir, uint8_t env_period,
                     uint32_t sample_rate);

/* Wave note. Phase resets each note (matching `(t*freq)%1`). */
void gb_render_wave(gb_sample_t *out, uint32_t duration,
                    uint32_t inc, const uint8_t *table, int sum, int vol_div,
                    uint32_t sample_rate);

/* Noise note. LFSR state resets each note (matching `triggerGameBoyLfsr`). */
void gb_render_noise(gb_sample_t *out, uint32_t duration,
                     uint32_t lfsr_hz, bool width7,
                     uint8_t env_initial, gb_env_dir_t env_dir, uint8_t env_period,
                     uint32_t sample_rate);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_RENDER_H */
