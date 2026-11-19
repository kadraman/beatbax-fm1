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

static void test_entity_name_length(void) {
    printf("Testing entity name length validation...\n");

    bax_song_t song;
    bax_song_init(&song);

    /* A 63-byte name fits (BAX_MAX_NAME_LEN - 1). */
    char fit[BAX_MAX_NAME_LEN];
    memset(fit, 'a', BAX_MAX_NAME_LEN - 1);
    fit[BAX_MAX_NAME_LEN - 1] = '\0';
    bax_instrument_t *inst = bax_song_add_instrument(&song, fit, BAX_VOICE_PULSE1);
    assert(inst != NULL);
    assert(song.instrument_count == 1);
    assert(strcmp(inst->name, fit) == 0);

    /* A 64-byte name is rejected without changing entity counts. */
    char over[BAX_MAX_NAME_LEN + 1];
    memset(over, 'b', BAX_MAX_NAME_LEN);
    over[BAX_MAX_NAME_LEN] = '\0';
    assert(bax_song_add_instrument(&song, over, BAX_VOICE_PULSE1) == NULL);
    assert(song.instrument_count == 1);

    /* The same boundary holds for every other entity helper. */
    assert(bax_song_add_effect(&song, over, BAX_EFFECT_OTHER) == NULL);
    assert(bax_song_add_subpat(&song, over) == NULL);
    assert(bax_song_add_pattern(&song, over) == NULL);
    assert(bax_song_add_sequence(&song, over) == NULL);
    assert(bax_song_add_section(&song, over) == NULL);
    assert(bax_song_add_form(&song, over) == NULL);

    bax_song_free(&song);
    printf("  PASS: entity name length validation\n");
}

static void test_array_capacity(void) {
    printf("Testing macro and wave array capacity limits...\n");

    char src[1024];
    int off = 0;
    bax_song_t song;
    bax_error_t err;

    /* Macro at capacity parses fully without data loss. */
    off = 0;
    off += snprintf(src + off, sizeof(src) - off, "chip gameboy\ninst a type=noise vol_env=[");
    for (int i = 0; i < BAX_MAX_MACRO_LEN; i++) {
        off += snprintf(src + off, sizeof(src) - off, "%s%d", i ? "," : "", i % 16);
    }
    off += snprintf(src + off, sizeof(src) - off, "]\n");
    assert(bax_song_parse(src, strlen(src), &song, &err));
    assert(song.instrument_count == 1);
    assert(song.instruments[0].vol_env.count == BAX_MAX_MACRO_LEN);
    assert(song.instruments[0].vol_env.values[BAX_MAX_MACRO_LEN - 1] == (BAX_MAX_MACRO_LEN - 1) % 16);
    bax_song_free(&song);

    /* One-over macro fails with a capacity error. */
    off = 0;
    off += snprintf(src + off, sizeof(src) - off, "chip gameboy\ninst a type=noise vol_env=[");
    for (int i = 0; i < BAX_MAX_MACRO_LEN + 1; i++) {
        off += snprintf(src + off, sizeof(src) - off, "%s%d", i ? "," : "", i % 16);
    }
    off += snprintf(src + off, sizeof(src) - off, "]\n");
    assert(!bax_song_parse(src, strlen(src), &song, &err));
    assert(err.code == BAX_ERR_CAPACITY);

    /* Wave at capacity parses fully. */
    off = 0;
    off += snprintf(src + off, sizeof(src) - off, "chip gameboy\ninst a type=wave wave=[");
    for (int i = 0; i < BAX_MAX_WAVE_SAMPLES; i++) {
        off += snprintf(src + off, sizeof(src) - off, "%s%d", i ? "," : "", i % 16);
    }
    off += snprintf(src + off, sizeof(src) - off, "]\n");
    assert(bax_song_parse(src, strlen(src), &song, &err));
    assert(song.instruments[0].wave.count == BAX_MAX_WAVE_SAMPLES);
    bax_song_free(&song);

    /* One-over wave fails with a capacity error. */
    off = 0;
    off += snprintf(src + off, sizeof(src) - off, "chip gameboy\ninst a type=wave wave=[");
    for (int i = 0; i < BAX_MAX_WAVE_SAMPLES + 1; i++) {
        off += snprintf(src + off, sizeof(src) - off, "%s%d", i ? "," : "", i % 16);
    }
    off += snprintf(src + off, sizeof(src) - off, "]\n");
    assert(!bax_song_parse(src, strlen(src), &song, &err));
    assert(err.code == BAX_ERR_CAPACITY);

    /* hUGETracker hex wave beyond 32 nibbles fails with a capacity error. */
    {
        char hex[64];
        memset(hex, '0', sizeof(hex));
        for (int i = 0; i < BAX_MAX_WAVE_SAMPLES + 1; i++) hex[i] = "0123456789ABCDEF"[i % 16];
        hex[BAX_MAX_WAVE_SAMPLES + 1] = '\0';
        snprintf(src, sizeof(src), "chip gameboy\ninst a type=wave wave=%s\n", hex);
        assert(!bax_song_parse(src, strlen(src), &song, &err));
        assert(err.code == BAX_ERR_CAPACITY);
    }

    printf("  PASS: macro and wave array capacity limits\n");
}

static void test_unknown_metadata(void) {
    printf("Testing rejection of unknown song metadata...\n");

    const char *src = "chip gameboy\nsong bpm \"120\"\n";
    bax_song_t song;
    bax_error_t err;
    assert(!bax_song_parse(src, strlen(src), &song, &err));
    assert(err.code == BAX_ERR_UNSUPPORTED_CONSTRUCT);
    assert(strcmp(err.construct, "song") == 0);
    assert(strstr(err.message, "bpm") != NULL);

    printf("  PASS: unknown song metadata refused\n");
}

static void test_directives_after_instrument(void) {
    printf("Testing top-level directives immediately after an instrument...\n");

    bax_song_t song;
    bax_error_t err;

    const char *steps = "chip gameboy\ninst a type=noise\nstepsPerBar 8\n";
    assert(bax_song_parse(steps, strlen(steps), &song, &err));
    assert(song.steps_per_bar == 8);
    bax_song_free(&song);

    const char *scale = "chip gameboy\ninst a type=noise\nscale G major\n";
    assert(bax_song_parse(scale, strlen(scale), &song, &err));
    assert(strcmp(song.scale_root, "G") == 0);
    assert(strcmp(song.scale_mode, "major") == 0);
    bax_song_free(&song);

    const char *section = "chip gameboy\ninst a type=noise\nsection s { mel = p }\n";
    assert(bax_song_parse(section, strlen(section), &song, &err));
    assert(song.section_count == 1);
    assert(strcmp(song.sections[0].name, "s") == 0);
    bax_song_free(&song);

    const char *form = "chip gameboy\ninst a type=noise\nform f = s\n";
    assert(bax_song_parse(form, strlen(form), &song, &err));
    assert(song.form_count == 1);
    assert(strcmp(song.forms[0].name, "f") == 0);
    bax_song_free(&song);

    printf("  PASS: directives after instrument\n");
}

static void test_seq_token_capacity(void) {
    printf("Testing sequence-reference token capacity...\n");

    char src[512];
    int off;
    bax_song_t song;
    bax_error_t err;

    /* A 255-byte reference fits item->raw (BAX_MAX_STR_LEN - 1). */
    off = 0;
    off += snprintf(src + off, sizeof(src) - off, "chip gameboy\nseq s = ");
    for (int i = 0; i < BAX_MAX_STR_LEN - 1; i++) src[off++] = 'x';
    src[off++] = '\n';
    src[off] = '\0';
    assert(bax_song_parse(src, strlen(src), &song, &err));
    assert(song.sequence_count == 1);
    assert(song.sequences[0].item_count == 1);
    bax_song_free(&song);

    /* One byte more is refused rather than truncated. */
    off = 0;
    off += snprintf(src + off, sizeof(src) - off, "chip gameboy\nseq s = ");
    for (int i = 0; i < BAX_MAX_STR_LEN; i++) src[off++] = 'x';
    src[off++] = '\n';
    src[off] = '\0';
    assert(!bax_song_parse(src, strlen(src), &song, &err));
    assert(err.code == BAX_ERR_CAPACITY);

    printf("  PASS: sequence-reference token capacity\n");
}

static void test_channel_auto_option(void) {
    printf("Testing rejection of unknown channel-auto options...\n");

    const char *src = "chip gameboy\nchannel auto form f inst=x bogus=1\n";
    bax_song_t song;
    bax_error_t err;
    assert(!bax_song_parse(src, strlen(src), &song, &err));
    assert(err.code == BAX_ERR_UNSUPPORTED_CONSTRUCT);
    assert(strcmp(err.construct, "channel") == 0);
    assert(strstr(err.message, "bogus=1") != NULL);

    printf("  PASS: unknown channel-auto option refused\n");
}

static void test_metadata_escaping(void) {
    printf("Testing metadata escaping round-trip...\n");

    const char *src =
        "chip gameboy\n"
        "song name \"He said \\\"hi\\\"\"\n"
        "song description \"\"\"first line\nsecond line\"\"\"\n"
        "song tags \"tag1, tag2\"\n";
    bax_song_t song;
    bax_error_t err;
    assert(bax_song_parse(src, strlen(src), &song, &err));

    assert(strcmp(song.name, "He said \"hi\"") == 0);
    assert(strcmp(song.description, "first line\nsecond line") == 0);

    char *out1 = NULL;
    size_t len1 = 0;
    assert(bax_song_write_alloc(&song, &out1, &len1));

    bax_song_t song2;
    assert(bax_song_parse(out1, len1, &song2, &err));
    assert(strcmp(song2.name, "He said \"hi\"") == 0);
    assert(strcmp(song2.description, "first line\nsecond line") == 0);

    char *out2 = NULL;
    size_t len2 = 0;
    assert(bax_song_write_alloc(&song2, &out2, &len2));
    assert(len1 == len2 && strcmp(out1, out2) == 0);

    bax_song_free(&song);
    bax_song_free(&song2);
    free(out1);
    free(out2);

    printf("  PASS: metadata escaping round-trip\n");
}

static void test_metadata_triple_quote_escape(void) {
    printf("Testing metadata containing a newline and triple quotes...\n");

    const char *src =
        "chip gameboy\n"
        "song description \"line one\\n\\\"\\\"\\\"quote\\\"\\\"\\\"\\nline two\"\n";
    bax_song_t song;
    bax_error_t err;
    assert(bax_song_parse(src, strlen(src), &song, &err));

    const char *expected = "line one\n\"\"\"quote\"\"\"\nline two";
    assert(strcmp(song.description, expected) == 0);

    char *out1 = NULL;
    size_t len1 = 0;
    assert(bax_song_write_alloc(&song, &out1, &len1));

    bax_song_t song2;
    assert(bax_song_parse(out1, len1, &song2, &err));
    assert(strcmp(song2.description, expected) == 0);

    char *out2 = NULL;
    size_t len2 = 0;
    assert(bax_song_write_alloc(&song2, &out2, &len2));
    assert(len1 == len2 && strcmp(out1, out2) == 0);

    bax_song_free(&song);
    bax_song_free(&song2);
    free(out1);
    free(out2);

    printf("  PASS: metadata with newline and triple quotes\n");
}

int main(int argc, char **argv) {
    if (argc > 1) {
        printf("Testing custom song: %s\n", argv[1]);
        test_fixture_roundtrip(argv[1], "custom.canon.bax", "custom.canon2.bax");
        return 0;
    }

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

    test_fixture_roundtrip("songs/gameboy/grassland_dash.bax",
                           "songs/gameboy/grassland_dash.canon.bax",
                           "songs/gameboy/grassland_dash.canon2.bax");

    test_unsupported_refusals();

    test_entity_name_length();
    test_array_capacity();
    test_unknown_metadata();
    test_directives_after_instrument();
    test_seq_token_capacity();
    test_channel_auto_option();
    test_metadata_escaping();
    test_metadata_triple_quote_escape();

    printf("\nAll unit tests passed successfully!\n");
    return 0;
}
