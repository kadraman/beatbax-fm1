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
 * Feature 003 Phase 0 checkpoint:
 *   "empty renderer links and renders silence deterministically."
 *
 * Proves the renderer skeleton links against the feature 002 song model,
 * binds a parsed fixture, and emits silence deterministically.
 */

#include "gb_render.h"
#include "song_parse.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_silence_determinism(void) {
    printf("Testing renderer silence + determinism...\n");

    bax_song_t song;
    bax_error_t err;
    bool ok = bax_song_parse_file("songs/gameboy/tutorial_groove.bax", &song, &err);
    if (!ok) {
        fprintf(stderr, "FAIL: parse failed: [%d:%d] (%s) %s\n",
                err.line, err.column, err.construct, err.message);
        exit(1);
    }
    assert(song.chip == BAX_CHIP_GAMEBOY);

    enum { FRAMES = GB_RENDER_BLOCK_FRAMES * 2 };

    gb_sample_t buf_a[FRAMES];
    gb_sample_t buf_b[FRAMES];

    gb_render_state_t st;
    gb_render_init(&st, &song, GB_RENDER_COMPARE_RATE);
    gb_render_block(&st, buf_a, FRAMES);

    /* Phase 0 output is silence and the frame counter advanced. */
    for (int i = 0; i < FRAMES; i++) {
        assert(buf_a[i] == 0);
    }
    assert(st.frame == FRAMES);

    /* A fresh, identically-configured state must produce identical output. */
    gb_render_state_t st2;
    gb_render_init(&st2, &song, GB_RENDER_COMPARE_RATE);
    gb_render_block(&st2, buf_b, FRAMES);
    assert(st2.frame == FRAMES);
    assert(memcmp(buf_a, buf_b, sizeof(buf_a)) == 0);

    bax_song_free(&song);
    printf("  PASS: renderer links, renders silence, deterministic\n");
}

int main(void) {
    printf("Running BeatBax FM-1 Game Boy Renderer Tests (Phase 0)...\n\n");
    test_silence_determinism();
    printf("\nAll renderer tests passed successfully!\n");
    return 0;
}
