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
 * Feature 003 Phase 1: per-note render primitives (base path, no effects).
 */

#include "gb_render.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

static void test_pulse_note(void) {
    printf("Testing pulse note render...\n");

    uint32_t inc = gb_period_to_inc(gb_midi_to_period(60), 44100); /* C4 ~261.6 Hz */
    gb_phase_t phase = 0;
    gb_sample_t buf[512];
    gb_render_pulse(buf, 512, inc, gb_duty_threshold(500), &phase,
                    15, GB_ENV_FLAT, 0, 44100);

    /* Flat env 15 -> constant amplitude 8191 = 32767 * 0.25 (gain). */
    for (int i = 0; i < 512; i++) {
        assert(buf[i] == 8191 || buf[i] == -8191);
    }
    assert(buf[0] == 8191);

    /* Square wave: rising edge at phase 0, falling edge at 50% duty.
     * C4 period ~168.6 samples, so the first negative sample is near 84. */
    int first_neg = -1;
    for (int i = 0; i < 512; i++) {
        if (buf[i] < 0) { first_neg = i; break; }
    }
    assert(first_neg >= 80 && first_neg <= 90);

    printf("  PASS: pulse note render (square wave, duty 50%%)\n");
}

static void test_wave_note(void) {
    printf("Testing wave note render...\n");

    uint8_t ramp[32];
    for (int i = 0; i < 32; i++) ramp[i] = (uint8_t)(i % 16); /* 0..15 twice, sum 240 */
    uint32_t inc = gb_period_to_inc(gb_midi_to_period(60), 44100);
    gb_sample_t buf[64];
    gb_render_wave(buf, 64, inc, ramp, 240, 1, 44100);

    /* First sample: phase ~0 -> index 0 (value 0) -> (0*32-240)*32767*54/48000 = -8847. */
    assert(buf[0] == -8847);

    bool saw_pos = false;
    for (int i = 0; i < 64; i++) {
        if (buf[i] > 0) { saw_pos = true; break; }
    }
    assert(saw_pos);

    printf("  PASS: wave note render (staircase)\n");
}

static void test_noise_note(void) {
    printf("Testing noise note render...\n");

    gb_sample_t buf[1000];
    gb_render_noise(buf, 1000, 1170 /* C-6 clock */, false,
                    15, GB_ENV_FLAT, 0, 44100);

    for (int i = 0; i < 1000; i++) {
        assert(buf[i] == 8191 || buf[i] == -8191);
    }
    bool saw_pos = false, saw_neg = false;
    for (int i = 0; i < 1000; i++) {
        if (buf[i] > 0) saw_pos = true;
        if (buf[i] < 0) saw_neg = true;
    }
    assert(saw_pos && saw_neg);

    printf("  PASS: noise note render (LFSR, non-trivial)\n");
}

int main(void) {
    printf("Running BeatBax FM-1 Note Render Tests (Phase 1)...\n\n");
    test_pulse_note();
    test_wave_note();
    test_noise_note();
    printf("\nAll note render tests passed successfully!\n");
    return 0;
}
