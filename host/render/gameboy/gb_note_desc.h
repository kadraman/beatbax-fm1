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

#ifndef BAX_GB_NOTE_DESC_H
#define BAX_GB_NOTE_DESC_H

#include "gb_note_render.h"
#include "song_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Host-only resolver: lower a song instrument + named effect into a float-free
 * `gb_note_desc_t`. Float arithmetic (pulse duty `*10.0f`, bend `atof`) is
 * allowed here — it is not linked into the firmware sample loop.
 */
void gb_note_desc_from_song(gb_note_desc_t *desc, const bax_song_t *song,
                            const bax_instrument_t *inst, const char *effect_name);

/*
 * Host convenience: resolve `inst`/`effect_name` from `song` and begin a note.
 * Equivalent to `gb_note_desc_from_song` + `gb_note_begin_desc`.
 */
void gb_note_begin(gb_note_t *n, const bax_song_t *song, int midi,
                   const bax_instrument_t *inst, const char *effect_name,
                   uint32_t len, uint32_t sample_rate, int vol_override,
                   gb_channel_state_t *state, int channel,
                   bool join, bool hold, uint32_t start_sample);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_NOTE_DESC_H */
