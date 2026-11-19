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
 * Feature 003 Phase 1 (T010-T012): per-voice synthesis unit tests.
 *
 * Verifies the fixed-point building blocks against the BeatBax reference
 * (`packages/engine/src/chips/gameboy/` + `audio/pcmRenderer.ts`).
 */

#include "gb_voice.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_period_table(void) {
    printf("Testing note/period mapping...\n");

    /* C2 (MIDI 36) = index 0 = 44; C4 (MIDI 60) = index 24 = 1547. */
    assert(gb_midi_to_period(36) == 44);
    assert(gb_midi_to_period(60) == 1547);
    /* freq = 131072 / (2048 - period). */
    assert(gb_period_to_freq_hz(44) == 65);    /* 131072/2004 = 65.4 */
    assert(gb_period_to_freq_hz(1547) == 262); /* 131072/501 = 261.6 */
    /* Out-of-range clamps. */
    assert(gb_midi_to_period(10) == 0);
    assert(gb_midi_to_period(120) == 2047);
    /* Phase increment is positive and monotonic with register. */
    assert(gb_period_to_inc(44, 44100) > 0);
    assert(gb_period_to_inc(1547, 44100) > gb_period_to_inc(44, 44100));

    printf("  PASS: note/period mapping\n");
}

static void test_envelope(void) {
    printf("Testing GB volume envelope...\n");

    /* period 0 (or flat) -> constant initial/15. */
    assert(gb_envelope_q15(8, GB_ENV_FLAT, 3, 0, 44100) == 8 * 32767 / 15);
    assert(gb_envelope_q15(8, GB_ENV_DOWN, 0, 100000, 44100) == 8 * 32767 / 15);

    /* Down: initial 12, period 1 -> 12/15 at t=0, 0 after >= 12 steps (12/64 s). */
    assert(gb_envelope_q15(12, GB_ENV_DOWN, 1, 0, 44100) == 12 * 32767 / 15);
    assert(gb_envelope_q15(12, GB_ENV_DOWN, 1, 44100, 44100) == 0);

    /* Up: initial 4, period 1 -> saturates to 15/15 after 11+ steps. */
    assert(gb_envelope_q15(4, GB_ENV_UP, 1, 0, 44100) == 4 * 32767 / 15);
    assert(gb_envelope_q15(4, GB_ENV_UP, 1, 44100, 44100) == 32767);

    printf("  PASS: volume envelope\n");
}

static void test_pulse(void) {
    printf("Testing pulse duty + bipolar square...\n");

    assert(gb_duty_threshold(500) == 0x80000000u);
    assert(gb_duty_threshold(250) == 0x40000000u);
    assert(gb_duty_threshold(125) == 0x20000000u);
    assert(gb_duty_threshold(750) == 0xC0000000u);

    assert(gb_pulse_bipolar(0, 0x80000000u) == 32767);
    assert(gb_pulse_bipolar(0x7FFFFFFFu, 0x80000000u) == 32767);
    assert(gb_pulse_bipolar(0x80000000u, 0x80000000u) == -32767);

    printf("  PASS: pulse duty + square\n");
}

static void test_wave(void) {
    printf("Testing wave ZOH + AC coupling...\n");

    uint8_t dc[32];
    memset(dc, 8, sizeof(dc));
    /* Constant table is DC; AC coupling removes it entirely. */
    assert(gb_wave_sample_q15(dc, 256, 1, 0) == 0);

    uint8_t ramp[32];
    for (int i = 0; i < 32; i++) ramp[i] = (uint8_t)(i % 16); /* 0..15 twice, sum 240 */
    /* phase 0 -> index 0 (value 0): (32*0 - 240) * 32767 / 480 = -16383 */
    assert(gb_wave_sample_q15(ramp, 240, 1, 0) == -16383);
    /* phase 15/32 -> index 15 (value 15): +16383 */
    assert(gb_wave_sample_q15(ramp, 240, 1, 15u << 27) == 16383);

    assert(gb_wave_volume_div(100) == 1);
    assert(gb_wave_volume_div(50) == 2);
    assert(gb_wave_volume_div(25) == 4);
    assert(gb_wave_volume_div(0) == 0);
    assert(gb_wave_volume_div(99) == 0); /* unknown -> mute */

    printf("  PASS: wave ZOH + AC coupling\n");
}

static void test_noise_lfsr(void) {
    printf("Testing noise LFSR...\n");

    uint16_t lfsr = gb_noise_lfsr_trigger(false);
    assert(lfsr & 1u); /* trigger returns bit-0 set */
    /* Step is deterministic. */
    assert(gb_noise_lfsr_step(lfsr, false) == gb_noise_lfsr_step(lfsr, false));
    assert(gb_noise_sample(1u) == 32767);
    assert(gb_noise_sample(0u) == -32767);

    printf("  PASS: noise LFSR\n");
}

static void test_noise_clock(void) {
    printf("Testing noise clock + note mapping...\n");

    /* hUGEDriver get_note_poly (see research.md table). */
    assert(gb_huge_note_to_index("C-6") == 36);
    assert(gb_noise_note_poly(36) == 0x57); /* shift 5, divisor 7 */
    assert(gb_huge_note_to_index("C-8") == 60);
    assert(gb_noise_note_poly(60) == 0x03); /* shift 0, divisor 3 */
    assert(gb_huge_note_to_index("C-7") == 48);
    assert(gb_noise_note_poly(48) == 0x27); /* shift 2, divisor 7 */
    assert(gb_huge_note_to_index("nope") == -1);
    assert(gb_huge_note_to_index("C-2") == -1); /* below range */

    /* LFSR rate for C-6 (shift 5, divisor 7): ~1170 Hz. */
    uint32_t hz = gb_noise_lfsr_hz(5, 7);
    assert(hz >= 1169 && hz <= 1171);
    /* divisor 0 -> ratio 0.5: 2*GB_CLOCK >> shift. */
    assert(gb_noise_lfsr_hz(0, 0) == 4194304u * 2u);

    printf("  PASS: noise clock + note mapping\n");
}

int main(void) {
    printf("Running BeatBax FM-1 Game Boy Voice Tests (Phase 1)...\n\n");
    test_period_table();
    test_envelope();
    test_pulse();
    test_wave();
    test_noise_lfsr();
    test_noise_clock();
    printf("\nAll voice tests passed successfully!\n");
    return 0;
}
