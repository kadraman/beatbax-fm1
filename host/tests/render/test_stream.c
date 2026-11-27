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
 * Feature 004 host tests: the streaming renderer must reproduce the offline
 * renderer sample-for-sample, be independent of block size, and restart cleanly.
 */

#include "gb_embed.h"
#include "gb_song_render.h"
#include "gb_stream.h"
#include "song_parse.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE GB_RENDER_DEVICE_RATE

static gb_sample_t *render_offline(const bax_song_t *song, size_t *out_len) {
    gb_sample_t *out = NULL;
    size_t len = 0;
    char errbuf[256];
    if (!gb_song_render_mono(song, RATE, &out, &len, errbuf, sizeof(errbuf))) {
        fprintf(stderr, "offline render failed: %s\n", errbuf);
        exit(1);
    }
    *out_len = len;
    return out;
}

static gb_sample_t *render_stream(const gb_song_blob_t *blob, uint32_t block,
                                  size_t *out_len) {
    assert(gb_stream_validate(blob) == 0);
    gb_stream_t st;
    gb_stream_init(&st, blob);
    gb_stream_play(&st);

    size_t total = blob->total_frames;
    gb_sample_t *out = (gb_sample_t *)malloc(total * sizeof(gb_sample_t));
    assert(out != NULL);

    size_t done = 0;
    while (done < total) {
        uint32_t n = block;
        if (n > total - done) {
            n = (uint32_t)(total - done);
        }
        gb_stream_render_block(&st, out + done, n);
        done += n;
    }
    assert(st.frame == total);
    *out_len = done;
    return out;
}

static void expect_identical(const gb_sample_t *a, const gb_sample_t *b, size_t n,
                             const char *label) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            fprintf(stderr, "FAIL %s: sample %zu differs: %d vs %d\n",
                    label, i, (int)a[i], (int)b[i]);
            exit(1);
        }
    }
}

static void test_bit_identity_and_blocksize(const bax_song_t *song) {
    printf("Testing streaming bit-identity + block-size independence...\n");

    size_t ref_len = 0;
    gb_sample_t *ref = render_offline(song, &ref_len);

    gb_song_blob_t blob;
    char errbuf[256];
    assert(gb_blob_build(song, RATE, &blob, errbuf, sizeof(errbuf)));
    assert(gb_stream_validate(&blob) == 0);
    assert(blob.total_frames == ref_len);

    static const uint32_t blocks[] = { 256, 128, 64, 1, 3, 97 };
    for (size_t b = 0; b < sizeof(blocks) / sizeof(blocks[0]); b++) {
        size_t n = 0;
        gb_sample_t *got = render_stream(&blob, blocks[b], &n);
        assert(n == ref_len);
        char label[64];
        snprintf(label, sizeof(label), "block=%u", blocks[b]);
        expect_identical(ref, got, n, label);
        free(got);
        printf("  PASS: block=%u matches offline\n", blocks[b]);
    }

    gb_blob_free(&blob);
    free(ref);
}

static void test_stop_reset_replay(const bax_song_t *song) {
    printf("Testing stop -> reset -> replay...\n");

    size_t ref_len = 0;
    gb_sample_t *ref = render_offline(song, &ref_len);

    gb_song_blob_t blob;
    char errbuf[256];
    assert(gb_blob_build(song, RATE, &blob, errbuf, sizeof(errbuf)));

    gb_stream_t st;
    gb_stream_init(&st, &blob);
    gb_stream_play(&st);

    /* Render a prefix, then stop (which rewinds), then replay fully. */
    gb_sample_t prefix[1000];
    gb_stream_render_block(&st, prefix, 1000);
    gb_stream_stop(&st);
    assert(st.frame == 0);

    gb_sample_t *replay = (gb_sample_t *)malloc(ref_len * sizeof(gb_sample_t));
    assert(replay != NULL);
    gb_stream_play(&st);
    size_t done = 0;
    while (done < ref_len) {
        uint32_t n = 128;
        if (n > ref_len - done) n = (uint32_t)(ref_len - done);
        gb_stream_render_block(&st, replay + done, n);
        done += n;
    }
    expect_identical(ref, replay, ref_len, "replay");
    printf("  PASS: replay after stop is identical\n");

    free(replay);
    gb_blob_free(&blob);
    free(ref);
}

int main(void) {
    printf("Running BeatBax FM-1 Stream Render Tests (feature 004)...\n\n");

    bax_song_t song;
    bax_error_t err;
    if (!bax_song_parse_file("songs/gameboy/tutorial_groove.bax", &song, &err)) {
        fprintf(stderr, "parse failed: [%d:%d] (%s) %s\n", err.line, err.column, err.construct, err.message);
        return 1;
    }

    test_bit_identity_and_blocksize(&song);
    test_stop_reset_replay(&song);

    bax_song_free(&song);
    printf("\nAll stream render tests passed successfully!\n");
    return 0;
}
