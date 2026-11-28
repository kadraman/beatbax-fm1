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

#ifndef BAX_GB_SONG_RENDER_H
#define BAX_GB_SONG_RENDER_H

#include "gb_render.h"
#include "song_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Expand a `bax_song_t` into a flat per-channel event stream and render it to
 * mono PCM at `sample_rate`. The returned buffer is malloc'd; the caller owns it.
 *
 * Returns true on success. On failure, writes a named reason to `errbuf` and
 * returns false. This is the host-side scheduler/expansion (feature 003 T013).
 *
 * Supported (this increment): `channel N => inst X (seq|pat) <refs>`,
 * `seq S = <pattern_ref>...` (with `:inst`, `:oct`, `:vol` modifiers),
 * patterns of notes/rests/holds with `:duration` and `<effect>` tags, inline
 * `inst <name>`, and named instrument hits. Named effects (vib/arp/port/bend)
 * and software macros are resolved in T014; sections/forms/channel-auto are a
 * later increment.
 */
bool gb_song_render_mono(const bax_song_t *song, uint32_t sample_rate,
                         gb_sample_t **out_mono, size_t *out_len,
                         char *errbuf, size_t errbuf_len);

/*
 * Same as gb_song_render_mono, but renders only the channels whose bit is set in
 * `channel_mask` (bit 0 = channel 1). Pass 0 to render all channels.
 */
bool gb_song_render_mono_mask(const bax_song_t *song, uint32_t sample_rate,
                              uint32_t channel_mask,
                              gb_sample_t **out_mono, size_t *out_len,
                              char *errbuf, size_t errbuf_len);

/*
 * Host-only: compute the 0.95 peak-limit scale (Q15) the offline renderer would
 * apply to `song` at `sample_rate`. Returns 0 when no scaling is needed
 * (peak <= the ceiling). Used by the compiled-song embedder to bake the scale.
 */
int32_t gb_song_peak_scale_q15(const bax_song_t *song, uint32_t sample_rate);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_SONG_RENDER_H */
