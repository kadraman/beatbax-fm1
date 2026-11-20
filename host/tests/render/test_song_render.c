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
 * Feature 003 Phase 1 (T013): end-to-end scheduler/render smoke test.
 * Renders the simple fixtures and asserts non-silent output.
 */

#include "gb_song_render.h"
#include "song_parse.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static void test_render_fixture(const char *path, size_t min_samples) {
    printf("Rendering %s...\n", path);

    bax_song_t song;
    bax_error_t err;
    bool ok = bax_song_parse_file(path, &song, &err);
    if (!ok) {
        fprintf(stderr, "FAIL: parse failed: [%d:%d] (%s) %s\n",
                err.line, err.column, err.construct, err.message);
        exit(1);
    }

    gb_sample_t *out = NULL;
    size_t len = 0;
    char errbuf[256];
    ok = gb_song_render_mono(&song, 44100, &out, &len, errbuf, sizeof(errbuf));
    if (!ok) {
        fprintf(stderr, "FAIL: render failed: %s\n", errbuf);
        exit(1);
    }
    assert(out != NULL);
    assert(len >= min_samples);

    bool non_silent = false;
    int64_t energy = 0;
    for (size_t i = 0; i < len; i++) {
        if (out[i] != 0) {
            non_silent = true;
        }
        energy += (int64_t)out[i] * out[i];
    }
    assert(non_silent);
    assert(energy > 0);

    printf("  PASS: %s -> %zu samples (non-silent)\n", path, len);
    free(out);
    bax_song_free(&song);
}

int main(void) {
    printf("Running BeatBax FM-1 Song Render Tests (Phase 1)...\n\n");
    test_render_fixture("songs/gameboy/tutorial_groove.bax", 100000);
    test_render_fixture("songs/gameboy/gb_sweep_demo.bax", 100000);
    test_render_fixture("songs/gameboy/gb_uge_note_demo.bax", 50000);
    test_render_fixture("songs/gameboy/grassland_dash.bax", 500000);
    printf("\nAll song render tests passed successfully!\n");
    return 0;
}
