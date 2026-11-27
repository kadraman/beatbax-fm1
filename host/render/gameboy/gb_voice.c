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

#include "gb_voice.h"
#include "gb_pulse_tables.h"

#define Q15_ONE 32767

GB_RAMFN int16_t gb_envelope_q15(uint8_t initial, gb_env_dir_t dir, uint8_t period,
                        uint32_t t, uint32_t dur, uint32_t sample_rate) {
    uint8_t init = initial > 15 ? 15 : initial;
    if (period == 0 || dir == GB_ENV_FLAT || sample_rate == 0) {
        return (int16_t)(((uint32_t)init * (uint32_t)Q15_ONE) / 15u);
    }

    /* BeatBax getEnvelopeValue (GB): stepPeriod = period/64 s; maxSteps =
     * max(1, floor(noteDur/stepPeriod)); interpolate linearly between the
     * levels initial ± step, then hold once the step count saturates. */
    uint64_t den = (uint64_t)period * sample_rate;        /* period * sr */
    uint64_t max_steps = ((uint64_t)dur * 64u) / den;     /* floor(dur*64/(period*sr)) */
    if (max_steps < 1u) {
        max_steps = 1u;
    }
    uint32_t saturate = (dir == GB_ENV_UP) ? (uint32_t)(15u - init) : (uint32_t)init;
    uint32_t n_steps = (uint32_t)(max_steps < saturate ? max_steps : saturate);

    /* te = min(t, dur); step position in Q16 = te * 64 * 2^16 / (period * sr). */
    uint32_t te = (t < dur) ? t : dur;
    uint64_t step_q16 = ((uint64_t)te * 4194304ull) / den;
    uint64_t nsteps_q16 = (uint64_t)n_steps << 16;
    if (step_q16 > nsteps_q16) {
        step_q16 = nsteps_q16;
    }

    /* level (Q16) = init ± step_q16, within [0, 15 << 16]. */
    int64_t level_q16 = (int64_t)init << 16;
    level_q16 += (dir == GB_ENV_UP) ? (int64_t)step_q16 : -(int64_t)step_q16;

    /* volume (Q15) = level * Q15_ONE / 15. */
    return (int16_t)((level_q16 * (int64_t)Q15_ONE) / (int64_t)(15 << 16));
}

GB_RAMFN uint32_t gb_duty_threshold(uint32_t duty_x10) {
    /* Duty fractions are exact binary fractions of 2^32. */
    switch (duty_x10) {
        case 125: return 0x20000000u; /* 1/8 */
        case 250: return 0x40000000u; /* 1/4 */
        case 750: return 0xC0000000u; /* 3/4 */
        case 500:
        default:  return 0x80000000u; /* 1/2 */
    }
}

GB_RAMFN int16_t gb_pulse_bipolar(gb_phase_t phase, uint32_t duty_threshold) {
    return (phase < duty_threshold) ? Q15_ONE : -Q15_ONE;
}

GB_RAMFN int16_t gb_pulse_sample(gb_phase_t phase, uint32_t duty_x10) {
    const int16_t *tab;
    switch (duty_x10) {
        case 125: tab = gb_pulse_tab_125; break;
        case 250: tab = gb_pulse_tab_250; break;
        case 750: tab = gb_pulse_tab_750; break;
        case 500:
        default:  tab = gb_pulse_tab_500; break;
    }
    return tab[phase >> GB_PULSE_TABLE_SHIFT];
}

GB_RAMFN int16_t gb_wave_sample_q15(const uint8_t *table, int sum, int vol_div, gb_phase_t phase) {
    if (!table || vol_div <= 0) {
        return 0;
    }
    uint32_t index = phase >> 27u; /* floor(phase * 32 / 2^32) */
    int num = 32 * (int)table[index & 31u] - sum;
    /* (table - mean)/15 in Q15, then ×0.54 = ×0.9 (WAVE_OUTPUT_SCALE buffer fill)
     * ×0.6 (WAVE_PLAYBACK_GAIN). 480 = 32×15; 54/100 = 0.54. */
    int32_t q15 = (int32_t)(((int64_t)num * Q15_ONE * 54) / 48000);
    return (int16_t)(q15 / vol_div);
}

GB_RAMFN int gb_wave_volume_div(uint8_t volume) {
    switch (volume) {
        case 25: return 4;
        case 50: return 2;
        case 100: return 1;
        default: return 0; /* 0 (and unknown) -> mute */
    }
}

GB_RAMFN uint16_t gb_noise_lfsr_step(uint16_t lfsr, bool width7) {
    uint16_t high_bit_mask = width7 ? 0x4040u : 0x4000u;
    uint16_t new_high_bit = (uint16_t)(((lfsr ^ (lfsr >> 1)) ^ 1) & 1u);
    uint16_t next = (uint16_t)((lfsr >> 1) & 0x7FFFu);
    if (new_high_bit) {
        next |= high_bit_mask;
    } else {
        next &= (uint16_t)~high_bit_mask;
    }
    return next;
}

GB_RAMFN uint16_t gb_noise_lfsr_trigger(bool width7) {
    uint16_t lfsr = gb_noise_lfsr_step(0, width7);
    if (lfsr & 1u) {
        return lfsr;
    }
    for (int i = 0; i < 126; i++) {
        lfsr = gb_noise_lfsr_step(lfsr, width7);
        if (lfsr & 1u) {
            return lfsr;
        }
    }
    return lfsr;
}

GB_RAMFN int16_t gb_noise_sample(uint16_t lfsr) {
    return (lfsr & 1u) ? Q15_ONE : -Q15_ONE;
}

GB_RAMFN uint8_t gb_noise_note_poly(int note_index) {
    int a = (((note_index + 192) & 0xFF) ^ 0xFF) & 0xFF;
    if (a < 7) {
        return (uint8_t)a;
    }
    int h = a;
    int b = ((a >> 2) - 1) & 0xFF;
    int c = (h & 3) + 4;
    a = (c | ((b << 4) & 0xF0)) & 0xFF;
    return (uint8_t)a;
}

int gb_huge_note_to_index(const char *note_name) {
    static const char *const names[12] = {
        "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
    };
    if (!note_name) {
        return -1;
    }
    /* Uppercase, trim a trailing 'b' is not used in hUGE names (flats are '-'). */
    char upper[8];
    unsigned len = 0;
    while (note_name[len] != '\0' && len < sizeof(upper)) {
        len++;
    }
    if (len >= sizeof(upper)) {
        return -1;
    }
    for (unsigned i = 0; i < len; i++) {
        char c = note_name[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        upper[i] = c;
    }
    upper[len] = '\0';

    /* Expect 3 chars: note (2) + octave digit (1). */
    if (len != 3) {
        return -1;
    }
    if (upper[2] < '3' || upper[2] > '9') {
        return -1;
    }
    int note_index = -1;
    for (int i = 0; i < 12; i++) {
        if (upper[0] == names[i][0] && upper[1] == names[i][1]) {
            note_index = i;
            break;
        }
    }
    if (note_index < 0) {
        return -1;
    }
    int octave = upper[2] - '0';
    int uge = (octave - 3) * 12 + note_index;
    if (uge < 0 || uge > 72) {
        return -1;
    }
    return uge;
}

uint32_t gb_noise_lfsr_hz(uint32_t shift, uint32_t divisor) {
    if (divisor == 0) {
        /* ratio = 0.5 -> lfsrHz = GB_CLOCK / (0.5 * 2^shift) = 2*GB_CLOCK / 2^shift */
        return (GB_CLOCK * 2u) >> (shift & 0x7u);
    }
    uint32_t ratio = (divisor & 0x7u) * 16u;
    uint32_t den = ratio << (shift & 0x7u);
    return (GB_CLOCK + den / 2u) / den; /* rounded */
}

GB_RAMFN uint64_t gb_noise_lfsr_inc(uint32_t shift, uint32_t divisor, uint32_t sample_rate) {
    if (sample_rate == 0) {
        return 0;
    }
    uint64_t num;
    uint64_t den;
    if (divisor == 0) {
        /* ratio = 0.5: lfsrHz = 2*GB_CLOCK / 2^shift. */
        num = (uint64_t)GB_CLOCK << 33; /* 2 * GB_CLOCK * 2^32 */
        den = ((uint64_t)1 << (shift & 0x7u)) * (uint64_t)sample_rate;
    } else {
        num = (uint64_t)GB_CLOCK << 32;
        den = ((uint64_t)(divisor & 0x7u) << (4u + (shift & 0x7u))) * (uint64_t)sample_rate;
    }
    return (num + den / 2u) / den;
}
