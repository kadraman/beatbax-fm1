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

#ifndef BAX_SONG_PARSE_H
#define BAX_SONG_PARSE_H

#include "song_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BAX_OK = 0,
    BAX_ERR_EMPTY_FILE,
    BAX_ERR_IO,
    BAX_ERR_UNSUPPORTED_CHIP,
    BAX_ERR_UNSUPPORTED_CONSTRUCT,
    BAX_ERR_INVALID_CHANNEL,
    BAX_ERR_SYNTAX,
    BAX_ERR_CAPACITY,
    BAX_ERR_OUT_OF_MEMORY
} bax_error_code_t;

typedef struct {
    bax_error_code_t code;
    int line;
    int column;
    char construct[BAX_MAX_NAME_LEN];
    char message[BAX_MAX_STR_LEN];
} bax_error_t;

/* Reset error to OK state */
void bax_error_init(bax_error_t *err);

/* Set error details */
void bax_error_set(bax_error_t *err, bax_error_code_t code, int line, int col,
                  const char *construct, const char *fmt, ...);

/* Parse .bax text buffer into a structured song model */
bool bax_song_parse(const char *text, size_t len, bax_song_t *out_song, bax_error_t *out_err);

/* Parse .bax file from path into a structured song model */
bool bax_song_parse_file(const char *path, bax_song_t *out_song, bax_error_t *out_err);

#ifdef __cplusplus
}
#endif

#endif /* BAX_SONG_PARSE_H */
