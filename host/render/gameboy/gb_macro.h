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

#ifndef BAX_GB_MACRO_H
#define BAX_GB_MACRO_H

#include "song_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Game Boy instrument "tick program" (feature 003 T014).
 *
 * Lowers `vol_env` / `pitch_env` / `duty_env` / `arp_env` into a row table that
 * advances at 60 Hz (HUGE_TICK_SEC = 1/60), matching BeatBax's
 * `lowerGameBoyInstrumentProgram`. Each row carries a semitone offset (holding
 * across rows), an optional volume (0-15), and an optional duty index (0-3).
 * A loop point makes the tail cycle; otherwise the program freezes silent.
 */

#define GB_TICK_SEC_NUM 1
#define GB_TICK_SEC_DEN 60
#define GB_MAX_TICK_ROWS 64

typedef struct {
    int offset; /* semitone offset (holds across rows) */
    int vol;    /* 0..15, or -1 = hold previous */
    int duty;   /* 0..3, or -1 = hold previous */
} gb_tick_row_t;

typedef struct {
    gb_tick_row_t rows[GB_MAX_TICK_ROWS];
    int count;
    int loop_to; /* -1 = freeze silent after the last row; >=0 = loop back to this row */
    bool enabled;
} gb_tick_program_t;

/* Build a tick program from an instrument's macros. `enabled` is false if the
 * instrument has no lowerable macros. */
void gb_tick_program_build(const bax_instrument_t *inst, gb_tick_program_t *out);

/* Resolve the active row at an absolute 60 Hz tick (handles loop/freeze). */
const gb_tick_row_t *gb_tick_program_row(const gb_tick_program_t *p, int tick);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_MACRO_H */
