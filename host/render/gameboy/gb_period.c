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

#include "gb_period.h"

/*
 * hUGETracker Game Boy period table (packages/engine/src/chips/gameboy/periodTables.ts).
 * Index 0 = C2 (MIDI 36); index 12 = C3, etc. Each value is a raw period-register
 * value; frequency = 131072 / (2048 - period).
 */
const uint16_t gb_period_table[GB_PERIOD_TABLE_LEN] = {
    44, 156, 262, 363, 457, 547, 631, 711, 786, 856, 923, 986,
    1046, 1102, 1155, 1205, 1253, 1297, 1339, 1379, 1417, 1452, 1486, 1517,
    1547, 1575, 1601, 1627, 1650, 1673, 1694, 1714, 1732, 1750, 1767, 1783,
    1797, 1811, 1825, 1837, 1849, 1860, 1871, 1881, 1890, 1899, 1907, 1915,
    1923, 1930, 1936, 1943, 1949, 1954, 1959, 1964, 1969, 1973, 1977, 1981,
    1985, 1989, 1992, 1995, 1998, 2001, 2004, 2006, 2008, 2011, 2013, 2015
};

uint32_t gb_midi_to_period(int midi) {
    if (midi < 36) {
        return 0; /* below C2: lowest representable period */
    }
    if (midi >= 36 + (int)GB_PERIOD_TABLE_LEN) {
        return 2047; /* above B7: highest period, silenced by the sr/2 guard */
    }
    return gb_period_table[midi - 36];
}

uint32_t gb_period_to_inc(uint32_t reg, uint32_t sample_rate) {
    if (sample_rate == 0) {
        return 0;
    }
    /* freq = 131072 / (2048 - reg); inc = round(freq / sr * 2^32)
       = round(131072 * 2^32 / ((2048 - reg) * sr)). */
    uint32_t r = reg & 0x7FFu; /* clamp 0..2047 */
    uint32_t period = 2048u - r; /* 1..2048 */
    uint64_t den = (uint64_t)period * (uint64_t)sample_rate;
    if (den == 0) {
        return 0;
    }
    uint64_t num = (uint64_t)131072u << 32; /* 2^49 */
    uint64_t q = (num + den / 2u) / den;
    if (q > 0xFFFFFFFFull) {
        q = 0xFFFFFFFFull;
    }
    return (uint32_t)q;
}

uint32_t gb_period_to_freq_hz(uint32_t reg) {
    uint32_t r = reg & 0x7FFu;
    uint32_t period = 2048u - r; /* 1..2048 */
    return (131072u + period / 2u) / period;
}
