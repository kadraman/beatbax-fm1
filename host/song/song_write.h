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

#ifndef BAX_SONG_WRITE_H
#define BAX_SONG_WRITE_H

#include "song_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Serialize song into a fixed buffer in canonical .bax format.
 * Returns true if successful and fits into buf_size.
 * *out_len receives the number of bytes written (excluding NUL terminator).
 */
bool bax_song_write(const bax_song_t *song, char *buf, size_t buf_size, size_t *out_len);

/*
 * Serialize song into a newly allocated string buffer in canonical .bax format.
 * Caller must free(*out_str) when done.
 */
bool bax_song_write_alloc(const bax_song_t *song, char **out_str, size_t *out_len);

/*
 * Serialize song and write directly to a file at path in canonical .bax format.
 */
bool bax_song_write_file(const bax_song_t *song, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* BAX_SONG_WRITE_H */
