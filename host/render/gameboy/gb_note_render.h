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

#ifndef BAX_GB_NOTE_RENDER_H
#define BAX_GB_NOTE_RENDER_H

#include "gb_render.h"
#include "song_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per-channel persistent state for the full render (phase, vibrato LFO, portamento). */
typedef struct {
    gb_phase_t pulse_phase[BAX_MAX_CHANNELS];
    gb_phase_t vib_phase[BAX_MAX_CHANNELS];
    int32_t last_freq_q16[BAX_MAX_CHANNELS];
} gb_channel_state_t;

void gb_channel_state_init(gb_channel_state_t *st);

/*
 * Render one note (mono, overwrites `dst[0..len)`) with software macros
 * (vol_env/pitch_env/duty_env/arp_env) and named effects (vib/arp/port).
 * `effect_name` may be "" for no named effect. `midi` is the resolved note.
 * `vol_override` is a 0..15 volume gain, or -1 for none.
 */
void gb_render_note_full(gb_sample_t *dst, uint32_t len,
                         const bax_song_t *song, int midi,
                         const bax_instrument_t *inst, const char *effect_name,
                         uint32_t sample_rate, int vol_override,
                         gb_channel_state_t *state, int channel);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_NOTE_RENDER_H */
