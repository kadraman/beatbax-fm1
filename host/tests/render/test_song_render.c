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
#include <string.h>

static int64_t render_src_energy(const char *src) {
    bax_song_t song;
    bax_error_t err;
    bool ok = bax_song_parse(src, strlen(src), &song, &err);
    assert(ok);
    gb_sample_t *out = NULL;
    size_t len = 0;
    char errbuf[256];
    assert(gb_song_render_mono(&song, 44100, &out, &len, errbuf, sizeof(errbuf)));
    int64_t energy = 0;
    for (size_t i = 0; i < len; i++) {
        energy += (int64_t)out[i] * out[i];
    }
    free(out);
    bax_song_free(&song);
    return energy;
}

static void test_volume_override(void) {
    printf("Testing volume override...\n");
    const char *base =
        "chip gameboy\ninst a type=pulse1 duty=50 env=gb:15,down,1\n"
        "pat p = C4:4\nchannel 1 => inst a pat p\n";
    const char *muted =
        "chip gameboy\ninst a type=pulse1 duty=50 env=gb:15,down,1\n"
        "pat p = vol:0 C4:4\nchannel 1 => inst a pat p\n";
    assert(render_src_energy(base) > 0);
    assert(render_src_energy(muted) == 0);
    printf("  PASS: inline vol:0 silences the note\n");
}

static void test_hold_sustain(void) {
    printf("Testing hold sustain...\n");
    const char *one =
        "chip gameboy\ninst a type=pulse1 duty=50 env=gb:15,flat\n"
        "pat p = C4\nchannel 1 => inst a pat p\n";
    const char *held =
        "chip gameboy\ninst a type=pulse1 duty=50 env=gb:15,flat\n"
        "pat p = C4 - - -\nchannel 1 => inst a pat p\n";
    int64_t e1 = render_src_energy(one);
    int64_t e4 = render_src_energy(held);
    assert(e1 > 0);
    assert(e4 > e1 * 3);
    printf("  PASS: holds sustain the preceding note\n");
}

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

static void test_temp_instrument_restore(void) {
    printf("Testing temporary instrument restore...\n");
    /* a (level 15 -> 8191) is the default; b (level 8 -> 4368) is a one-note
     * temporary override. The note after the override must return to `a`. */
    const char *src =
        "chip gameboy\n"
        "inst a type=pulse1 duty=50 env=gb:15,flat\n"
        "inst b type=pulse1 duty=50 env=gb:8,flat\n"
        "pat p = C4 inst(b,1) D4 E4\n"
        "channel 1 => inst a pat p\n";
    bax_song_t song;
    bax_error_t err;
    assert(bax_song_parse(src, strlen(src), &song, &err));
    gb_sample_t *out = NULL;
    size_t len = 0;
    char errbuf[256];
    assert(gb_song_render_mono(&song, 44100, &out, &len, errbuf, sizeof(errbuf)));

    int peak1 = 0, peak2 = 0, peak3 = 0;
    for (int i = 0; i < 5000; i++) {
        int a = out[i] < 0 ? -out[i] : out[i];
        if (a > peak1) peak1 = a;
    }
    for (int i = 6000; i < 10000; i++) {
        int a = out[i] < 0 ? -out[i] : out[i];
        if (a > peak2) peak2 = a;
    }
    for (int i = 11000; i < 15500; i++) {
        int a = out[i] < 0 ? -out[i] : out[i];
        if (a > peak3) peak3 = a;
    }
    assert(peak1 == 8191); /* note 1: default `a` */
    assert(peak2 == 4368); /* note 2: temporary `b` */
    assert(peak3 == 8191); /* note 3: restored to `a` */

    free(out);
    bax_song_free(&song);
    printf("  PASS: temporary instrument restores the default\n");
}

int main(void) {
    printf("Running BeatBax FM-1 Song Render Tests (Phase 1)...\n\n");
    test_render_fixture("songs/gameboy/tutorial_groove.bax", 100000);
    test_render_fixture("songs/gameboy/gb_sweep_demo.bax", 100000);
    test_render_fixture("songs/gameboy/gb_uge_note_demo.bax", 50000);
    test_render_fixture("songs/gameboy/grassland_dash.bax", 500000);
    test_volume_override();
    test_hold_sustain();
    test_temp_instrument_restore();
    printf("\nAll song render tests passed successfully!\n");
    return 0;
}
