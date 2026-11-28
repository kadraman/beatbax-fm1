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
#include "gb_note_render.h"
#include "gb_expand.h"
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
    /* Note 3 starts at 11025; note 2's 5 ms release tail (through ~11246) rings
     * into it, so sample past the overlap to measure note 3's own level. */
    for (int i = 12000; i < 15500; i++) {
        int a = out[i] < 0 ? -out[i] : out[i];
        if (a > peak3) peak3 = a;
    }
    /* Assert the level relationship rather than exact peaks: `a` is level 15,
     * `b` is level 8, and the note after the override must restore to `a`.
     * Exact peak values shifted slightly with the band-limited pulse (008). */
    assert(peak1 > 0);
    assert(peak3 == peak1);           /* note 3: restored to default `a` */
    assert(peak2 == peak1 * 8 / 15);  /* note 2: temporary `b` (8/15 of level 15) */
    assert(peak2 < peak1);            /* override is quieter than the default */

    free(out);
    bax_song_free(&song);
    printf("  PASS: temporary instrument restores the default\n");
}

static void test_log2(void) {
    printf("Testing log2_q16 + pow2 round-trip...\n");
    assert(gb_log2_q16(1) == 0);
    assert(gb_log2_q16(2) == 65536);
    assert(gb_log2_q16(32768) == 15 * 65536);
    assert(gb_log2_q16(65536) == 16 * 65536);
    assert(gb_log2_q16(131072) == 17 * 65536);
    /* Non-power-of-two values (16-bit mantissa + interpolation). */
    assert(gb_log2_q16(3) > 103870 && gb_log2_q16(3) < 103880);
    assert(gb_log2_q16(10) > 217700 && gb_log2_q16(10) < 217712);
    /* Round-trip: pow2(12 * log2(v)) == v for powers of two in table range. */
    assert(gb_pow2_semi_q16(12 * gb_log2_q16(4)) == 4 * 65536);
    assert(gb_pow2_semi_q16(12 * gb_log2_q16(8)) == 8 * 65536);
    assert(gb_pow2_semi_q16(12 * gb_log2_q16(16)) == 16 * 65536);
    printf("  PASS: log2_q16 + pow2 round-trip\n");
}

/* Instantaneous period (samples) of a bipolar square wave near `idx`. */
static int instant_period(const gb_sample_t *buf, int idx, int limit) {
    int e1 = -1;
    for (int i = idx; i < idx + limit; i++) {
        if (buf[i - 1] < 0 && buf[i] >= 0) { e1 = i; break; }
    }
    if (e1 < 0) return -1;
    for (int i = e1 + 2; i < e1 + limit; i++) {
        if (buf[i - 1] < 0 && buf[i] >= 0) return i - e1;
    }
    return -1;
}

static void test_portamento(void) {
    printf("Testing pulse portamento (exponential) glide...\n");
    const char *src =
        "chip gameboy\n"
        "inst a type=pulse1 duty=50 env=gb:15,flat\n"
        "effect slide = port:16\n"
        "pat p = C4 C5<slide>\n"
        "channel 1 => inst a pat p\n";
    bax_song_t song;
    bax_error_t err;
    assert(bax_song_parse(src, strlen(src), &song, &err));
    gb_sample_t *out = NULL;
    size_t len = 0;
    char errbuf[256];
    assert(gb_song_render_mono(&song, 44100, &out, &len, errbuf, sizeof(errbuf)));

    /* 1 step = 5512 samples; C5<slide> is [5512, 11024), glide duration ~3100. */
    /* Midpoint (~sample 7062) should be the geometric mean, period ~119 samples. */
    int period = instant_period(out, 7060, 400);
    assert(period > 0);
    assert(period >= 116 && period <= 124); /* geometric ~119; linear would be ~112 */

    /* After the glide (~sample 9000) the note holds C5, period ~84 samples. */
    int period_end = instant_period(out, 9000, 400);
    assert(period_end > 0);
    assert(period_end >= 80 && period_end <= 88); /* ~84 samples = 523 Hz */

    free(out);
    bax_song_free(&song);
    printf("  PASS: pulse portamento (exponential) glide\n");
}

static void test_portamento_run(void) {
    printf("Testing pulse portamento run (voice-holding)...\n");
    const char *src =
        "chip gameboy\n"
        "inst a type=pulse1 duty=50 env=gb:15,down,2\n"
        "effect slide = port:16\n"
        "pat p = C4<slide> D4<slide> E4<slide> F4\n"
        "channel 1 => inst a pat p\n";
    bax_song_t song;
    bax_error_t err;
    assert(bax_song_parse(src, strlen(src), &song, &err));
    gb_sample_t *out = NULL;
    size_t len = 0;
    char errbuf[256];
    assert(gb_song_render_mono(&song, 44100, &out, &len, errbuf, sizeof(errbuf)));

    /* 1 step = 5512 samples. C4/D4/E4 form a held portamento run: the envelope
     * stays flat at the opening level 15. F4 is a normal note whose envelope
     * decays from 15. */
    int start_peak = 0;
    for (int i = 0; i < 1000; i++) {
        int a = out[i] < 0 ? -out[i] : out[i];
        if (a > start_peak) start_peak = a;
    }
    int run_peak = 0;
    for (int i = 15500; i < 16500; i++) {
        int a = out[i] < 0 ? -out[i] : out[i];
        if (a > run_peak) run_peak = a;
    }
    /* A normal F4 decays: by its later samples the level is below full. */
    int normal_peak = 0;
    for (int i = 20000; i < 21500; i++) {
        int a = out[i] < 0 ? -out[i] : out[i];
        if (a > normal_peak) normal_peak = a;
    }
    assert(start_peak > 0);
    /* Voice-holding keeps the envelope flat: the end of the run (E4) stays at the
     * opening level. Allow a 2-LSB tolerance for the band-limited table's
     * phase-sampled peak (a real decay would drop it by hundreds). */
    assert(run_peak >= start_peak - 2);
    assert(normal_peak > 0 && normal_peak < run_peak); /* normal note decays */

    free(out);
    bax_song_free(&song);
    printf("  PASS: pulse portamento run (voice-holding)\n");
}

static void test_arp(void) {
    printf("Testing arpeggio (arp) cycles pitch...\n");
    const char *src =
        "chip gameboy\n"
        "inst a type=pulse1 duty=50 env=gb:15,flat\n"
        "effect ar = arp:4,7\n"
        "pat p = C4<ar>:8\n"
        "channel 1 => inst a pat p\n";
    bax_song_t song;
    bax_error_t err;
    assert(bax_song_parse(src, strlen(src), &song, &err));
    gb_sample_t *out = NULL;
    size_t len = 0;
    char errbuf[256];
    assert(gb_song_render_mono(&song, 44100, &out, &len, errbuf, sizeof(errbuf)));
    /* arp cycles root/+4/+7 at 60 Hz (735 samples/step). Sample 1000 is the +4
     * step (E4), sample 1800 is the +7 step (G4): distinct pitches. */
    int p1 = instant_period(out, 1000, 400);
    int p2 = instant_period(out, 1800, 400);
    assert(p1 > 0 && p2 > 0);
    assert(p1 != p2);
    free(out);
    bax_song_free(&song);
    printf("  PASS: arpeggio cycles pitch\n");
}

static void test_bend(void) {
    printf("Testing pitch bend (bend) glides pitch...\n");
    const char *src =
        "chip gameboy\n"
        "inst a type=pulse1 duty=50 env=gb:15,flat\n"
        "effect r = bend:+12,exp,0,1\n"
        "pat p = C4<r>:8\n"
        "channel 1 => inst a pat p\n";
    bax_song_t song;
    bax_error_t err;
    assert(bax_song_parse(src, strlen(src), &song, &err));
    gb_sample_t *out = NULL;
    size_t len = 0;
    char errbuf[256];
    assert(gb_song_render_mono(&song, 44100, &out, &len, errbuf, sizeof(errbuf)));
    /* One octave rise over the note: the period must shrink from start to end. */
    int p_start = instant_period(out, 3000, 400);
    int p_end = instant_period(out, 26000, 400);
    assert(p_start > 0 && p_end > 0);
    assert(p_end < p_start); /* higher pitch = shorter period */
    free(out);
    bax_song_free(&song);
    printf("  PASS: pitch bend glides pitch\n");
}

static void test_vibrato(void) {
    printf("Testing vibrato (vib) modulates pitch...\n");
    const char *src =
        "chip gameboy\n"
        "inst a type=pulse1 duty=50 env=gb:15,flat\n"
        "effect w = vib:6,5,sine\n"
        "pat p = C4<w>:8\n"
        "channel 1 => inst a pat p\n";
    bax_song_t song;
    bax_error_t err;
    assert(bax_song_parse(src, strlen(src), &song, &err));
    gb_sample_t *out = NULL;
    size_t len = 0;
    char errbuf[256];
    assert(gb_song_render_mono(&song, 44100, &out, &len, errbuf, sizeof(errbuf)));
    /* A 5 Hz sine LFO modulates the pitch; the instantaneous period varies. */
    int p0 = instant_period(out, 5000, 400);
    int p1 = instant_period(out, 8000, 400);
    int p2 = instant_period(out, 11000, 400);
    assert(p0 > 0 && p1 > 0 && p2 > 0);
    assert(p0 != p1 || p1 != p2 || p0 != p2); /* not constant */
    free(out);
    bax_song_free(&song);
    printf("  PASS: vibrato modulates pitch\n");
}

static void test_note_resolution(void) {
    printf("Testing accidental octave crossing...\n");
    assert(gb_note_str_to_midi("C4") == 60);
    assert(gb_note_str_to_midi("B4") == 71);
    assert(gb_note_str_to_midi("B#4") == 72); /* crosses into C5 */
    assert(gb_note_str_to_midi("Cb4") == 59); /* crosses into B3 */
    assert(gb_note_str_to_midi("D#4") == 63);
    assert(gb_note_str_to_midi("Eb4") == 63);
    assert(gb_note_str_to_midi("C5") == 72);
    printf("  PASS: accidental octave crossing\n");
}

static void test_pattern_note_octave_crossing(void) {
    printf("Testing pattern note accidental octave crossing...\n");
    /* '#' starts a comment in .bax, so sharps are written as enharmonic flats.
     * Cb4 must resolve to B3 (59) via octave crossing, and Db4 to 61. */
    const char *src =
        "chip gameboy\n"
        "inst a type=pulse1 duty=50 env=gb:15,flat\n"
        "pat p = C4 Cb4 Db4\n"
        "channel 1 => inst a pat p\n";
    bax_song_t song;
    bax_error_t err;
    assert(bax_song_parse(src, strlen(src), &song, &err));
    gb_resolved_list_t list;
    uint32_t total = 0;
    char errbuf[256];
    assert(gb_song_resolve(&song, 44100, &list, &total, errbuf, sizeof(errbuf)));
    assert(list.count == 3);
    assert(list.items[0].midi == 60); /* C4 */
    assert(list.items[1].midi == 59); /* Cb4 -> B3 (octave crossing) */
    assert(list.items[2].midi == 61); /* Db4 (normal flat) */
    gb_resolved_list_free(&list);
    bax_song_free(&song);
    printf("  PASS: pattern note accidental octave crossing\n");
}

static void test_portamento_equal_pitch_progress(void) {
    printf("Testing portamento progress through an equal-pitch interval...\n");
    /* When the portamento target equals the previous note's pitch, the glide
     * position must still advance (so the ramp completes on schedule) rather
     * than freezing and then jumping late. Drive a C4 portamento note whose
     * target equals the previous channel frequency and check the position moved. */
    gb_channel_state_t state;
    gb_channel_state_init(&state);
    state.last_freq_q16[0] = gb_period_to_freq_q16(gb_midi_to_period(60)); /* C4 */

    gb_note_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.voice = BAX_VOICE_PULSE1;
    desc.duty_x10 = 500;
    desc.env_initial = 15;
    desc.env_period = 0; /* flat */
    desc.env_dir = GB_ENV_FLAT;
    desc.port_speed = 16;

    gb_note_t n;
    gb_note_begin_desc(&n, &desc, 60, 5512, 44100, -1, &state, 0, false, false, 0);

    gb_sample_t buf[128];
    gb_note_step(&n, buf, sizeof(buf) / sizeof(buf[0]));
    assert(n.port_p_q16 > 0); /* advanced through the equal-pitch interval */

    printf("  PASS: portamento advances through equal-pitch interval\n");
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
    test_log2();
    test_note_resolution();
    test_pattern_note_octave_crossing();
    test_portamento();
    test_portamento_run();
    test_portamento_equal_pitch_progress();
    test_arp();
    test_bend();
    test_vibrato();
    printf("\nAll song render tests passed successfully!\n");
    return 0;
}
