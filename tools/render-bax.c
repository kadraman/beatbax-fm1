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

/*
 * tools/render-bax.c
 * Host CLI: render a `.bax` song to a mono 16-bit WAV using the fixed-point
 * Game Boy renderer. Used by the comparison harness (feature 003 Phase 2).
 *
 *   render-bax <input.bax> <output.wav> [sample_rate]
 */

#include "gb_song_render.h"
#include "song_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_wav(const char *path, const gb_sample_t *mono, size_t len, uint32_t rate) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }
    uint32_t data_bytes = (uint32_t)len * 2u;
    uint32_t chunk_size = 36u + data_bytes;

    uint8_t hdr[44];
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "RIFF", 4);
    hdr[4] = (uint8_t)(chunk_size & 0xff);
    hdr[5] = (uint8_t)((chunk_size >> 8) & 0xff);
    hdr[6] = (uint8_t)((chunk_size >> 16) & 0xff);
    hdr[7] = (uint8_t)((chunk_size >> 24) & 0xff);
    memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    hdr[16] = 16; hdr[17] = 0; hdr[18] = 0; hdr[19] = 0; /* fmt chunk size */
    hdr[20] = 1; hdr[21] = 0;                              /* PCM */
    hdr[22] = 1; hdr[23] = 0;                              /* mono */
    hdr[24] = (uint8_t)(rate & 0xff);
    hdr[25] = (uint8_t)((rate >> 8) & 0xff);
    hdr[26] = (uint8_t)((rate >> 16) & 0xff);
    hdr[27] = (uint8_t)((rate >> 24) & 0xff);
    uint32_t byte_rate = rate * 2u;
    hdr[28] = (uint8_t)(byte_rate & 0xff);
    hdr[29] = (uint8_t)((byte_rate >> 8) & 0xff);
    hdr[30] = (uint8_t)((byte_rate >> 16) & 0xff);
    hdr[31] = (uint8_t)((byte_rate >> 24) & 0xff);
    hdr[32] = 2; hdr[33] = 0; /* block align */
    hdr[34] = 16; hdr[35] = 0; /* bits */
    memcpy(hdr + 36, "data", 4);
    hdr[40] = (uint8_t)(data_bytes & 0xff);
    hdr[41] = (uint8_t)((data_bytes >> 8) & 0xff);
    hdr[42] = (uint8_t)((data_bytes >> 16) & 0xff);
    hdr[43] = (uint8_t)((data_bytes >> 24) & 0xff);

    if (fwrite(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return -1;
    }
    /* 16-bit little-endian samples. */
    for (size_t i = 0; i < len; i++) {
        int16_t s = mono[i];
        uint8_t b[2];
        b[0] = (uint8_t)(s & 0xff);
        b[1] = (uint8_t)((s >> 8) & 0xff);
        if (fwrite(b, 1, 2, f) != 2) {
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: render-bax <input.bax> <output.wav> [sample_rate]\n");
        return 2;
    }
    uint32_t rate = 44100;
    if (argc >= 4) {
        rate = (uint32_t)atoi(argv[3]);
        if (rate == 0) {
            rate = 44100;
        }
    }

    bax_song_t song;
    bax_error_t err;
    if (!bax_song_parse_file(argv[1], &song, &err)) {
        fprintf(stderr, "parse error: [%d:%d] (%s) %s\n",
                err.line, err.column, err.construct, err.message);
        return 1;
    }

    gb_sample_t *out = NULL;
    size_t len = 0;
    char errbuf[256];
    if (!gb_song_render_mono(&song, rate, &out, &len, errbuf, sizeof(errbuf))) {
        fprintf(stderr, "render error: %s\n", errbuf);
        bax_song_free(&song);
        return 1;
    }

    if (write_wav(argv[2], out, len, rate) != 0) {
        fprintf(stderr, "write error: %s\n", argv[2]);
        free(out);
        bax_song_free(&song);
        return 1;
    }

    fprintf(stderr, "rendered %zu samples @ %u Hz -> %s\n", len, rate, argv[2]);
    free(out);
    bax_song_free(&song);
    return 0;
}
