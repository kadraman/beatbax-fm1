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

#ifndef BAX_GB_EMBED_H
#define BAX_GB_EMBED_H

#include "gb_stream.h"
#include "song_model.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Host-only: build a compiled song blob from a parsed `bax_song_t` at the given
 * sample rate. Lowers instruments/effects, expands + resolves the event schedule
 * (via gb_song_resolve), and bakes the peak-limit scale (via
 * gb_song_peak_scale_q15). The result is heap-allocated; free with
 * gb_blob_free. Returns false with a named reason on error.
 */
bool gb_blob_build(const bax_song_t *song, uint32_t sample_rate,
                   gb_song_blob_t *out, char *errbuf, size_t errbuf_len);

/* Free everything gb_blob_build allocated into `blob`. */
void gb_blob_free(gb_song_blob_t *blob);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_EMBED_H */
