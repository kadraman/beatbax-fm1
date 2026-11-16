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

#include "song_model.h"
#include "song_parse.h"
#include "song_write.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_fixture_roundtrip(const char *fixture_path, const char *out_path1, const char *out_path2) {
    printf("Testing fixture round-trip: %s...\n", fixture_path);

    bax_song_t song1;
    bax_error_t err;
    bool ok = bax_song_parse_file(fixture_path, &song1, &err);
    if (!ok) {
        fprintf(stderr, "FAIL: Parse 1 failed on '%s': [%d:%d] (%s) %s\n",
                fixture_path, err.line, err.column, err.construct, err.message);
        exit(1);
    }

    assert(song1.chip == BAX_CHIP_GAMEBOY);

    /* Write canonical form 1 */
    char *out_str1 = NULL;
    size_t len1 = 0;
    ok = bax_song_write_alloc(&song1, &out_str1, &len1);
    assert(ok && out_str1 != NULL && len1 > 0);

    if (out_path1) {
        ok = bax_song_write_file(&song1, out_path1);
        assert(ok);
    }

    /* Parse canonical form 1 */
    bax_song_t song2;
    ok = bax_song_parse(out_str1, len1, &song2, &err);
    if (!ok) {
        fprintf(stderr, "FAIL: Parse 2 failed on generated text: [%d:%d] (%s) %s\n",
                err.line, err.column, err.construct, err.message);
        fprintf(stderr, "Generated text:\n%s\n", out_str1);
        exit(1);
    }

    /* Write canonical form 2 */
    char *out_str2 = NULL;
    size_t len2 = 0;
    ok = bax_song_write_alloc(&song2, &out_str2, &len2);
    assert(ok && out_str2 != NULL && len2 > 0);

    if (out_path2) {
        ok = bax_song_write_file(&song2, out_path2);
        assert(ok);
    }

    /* Idempotence check: form 1 and form 2 MUST be byte-identical */
    if (len1 != len2 || strcmp(out_str1, out_str2) != 0) {
        fprintf(stderr, "FAIL: Idempotence check failed for '%s'!\n", fixture_path);
        fprintf(stderr, "Form 1 (len %zu):\n%s\n", len1, out_str1);
        fprintf(stderr, "Form 2 (len %zu):\n%s\n", len2, out_str2);
        exit(1);
    }

    printf("  PASS: %s is idempotent (len=%zu bytes)\n", fixture_path, len1);

    bax_song_free(&song1);
    bax_song_free(&song2);
    free(out_str1);
    free(out_str2);
}

static void test_unsupported_refusals(void) {
    printf("Testing refusal of unsupported chips and constructs...\n");

    bax_song_t song;
    bax_error_t err;

    /* 1. Unsupported chip from fixture file */
    bool ok = bax_song_parse_file("songs/unsupported/nes_chip.bax", &song, &err);
    assert(!ok);
    assert(err.code == BAX_ERR_UNSUPPORTED_CHIP);
    assert(strcmp(err.construct, "chip") == 0);
    assert(strstr(err.message, "unsupported chip: nes") != NULL);
    printf("  PASS: Refused unsupported chip: nes (construct: '%s', msg: '%s')\n", err.construct, err.message);

    /* 2. Unsupported chip from string: sms */
    const char *sms_src = "chip sms\nbpm 120\n";
    ok = bax_song_parse(sms_src, strlen(sms_src), &song, &err);
    assert(!ok);
    assert(err.code == BAX_ERR_UNSUPPORTED_CHIP);
    assert(strcmp(err.construct, "chip") == 0);
    assert(strstr(err.message, "unsupported chip: sms") != NULL);
    printf("  PASS: Refused unsupported chip: sms (construct: '%s', msg: '%s')\n", err.construct, err.message);

    /* 3. Unsupported directive from fixture file: import */
    ok = bax_song_parse_file("songs/unsupported/import_directive.bax", &song, &err);
    assert(!ok);
    assert(err.code == BAX_ERR_UNSUPPORTED_CONSTRUCT);
    assert(strcmp(err.construct, "import") == 0);
    assert(strstr(err.message, "unsupported directive: import") != NULL);
    printf("  PASS: Refused unsupported directive: import (construct: '%s', msg: '%s')\n", err.construct, err.message);

    /* 4. Empty file from fixture */
    ok = bax_song_parse_file("songs/unsupported/empty.bax", &song, &err);
    assert(!ok);
    assert(err.code == BAX_ERR_EMPTY_FILE);
    printf("  PASS: Refused empty fixture file (code: %d)\n", err.code);

    /* 5. Invalid channel number from fixture */
    ok = bax_song_parse_file("songs/unsupported/bad_channel.bax", &song, &err);
    assert(!ok);
    assert(err.code == BAX_ERR_INVALID_CHANNEL);
    assert(strcmp(err.construct, "channel") == 0);
    printf("  PASS: Refused out-of-range channel number (construct: '%s')\n", err.construct);
}

int main(void) {
    printf("Running BeatBax FM-1 Song Model Tests...\n\n");

    test_fixture_roundtrip("songs/gameboy/tutorial_groove.bax",
                           "songs/gameboy/tutorial_groove.canon.bax",
                           "songs/gameboy/tutorial_groove.canon2.bax");

    test_fixture_roundtrip("songs/gameboy/gb_sweep_demo.bax",
                           "songs/gameboy/gb_sweep_demo.canon.bax",
                           "songs/gameboy/gb_sweep_demo.canon2.bax");

    test_fixture_roundtrip("songs/gameboy/gb_uge_note_demo.bax",
                           "songs/gameboy/gb_uge_note_demo.canon.bax",
                           "songs/gameboy/gb_uge_note_demo.canon2.bax");

    test_unsupported_refusals();

    printf("\nAll unit tests passed successfully!\n");
    return 0;
}
