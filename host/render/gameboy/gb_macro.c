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

#include "gb_macro.h"
#include "gb_fixed.h"
#include <string.h>

static const gb_tick_row_t HALT_ROW = { 0, 0, -1 }; /* volume 0 -> silent */

void gb_tick_program_build(const bax_instrument_t *inst, gb_tick_program_t *out) {
    memset(out, 0, sizeof(*out));
    out->loop_to = -1;
    if (!inst) {
        return;
    }

    const bax_macro_env_t *pitch = &inst->pitch_env;
    const bax_macro_env_t *arp = &inst->arp_env;
    const bax_macro_env_t *vol = &inst->vol_env;
    const bax_macro_env_t *duty = &inst->duty_env;

    /* pitch_env wins over arp_env (both are semitone offsets). */
    const bax_macro_env_t *pitch_lane = (pitch->count > 0) ? pitch
                                       : (arp->count > 0) ? arp : NULL;

    size_t len = pitch_lane ? pitch_lane->count : 0;
    if (vol->count > len) len = vol->count;
    if (duty->count > len) len = duty->count;
    if (len == 0) {
        return;
    }
    if (len > GB_MAX_TICK_ROWS) {
        len = GB_MAX_TICK_ROWS;
    }

    /* Loop point = minimum of the lane loop points (>= 0). */
    int loop_to = -1;
    if (pitch_lane && pitch_lane->loop_point >= 0) loop_to = pitch_lane->loop_point;
    if (vol->count > 0 && vol->loop_point >= 0 &&
        (loop_to < 0 || vol->loop_point < loop_to)) loop_to = vol->loop_point;
    if (duty->count > 0 && duty->loop_point >= 0 &&
        (loop_to < 0 || duty->loop_point < loop_to)) loop_to = duty->loop_point;
    if (loop_to >= (int)len) loop_to = (int)len - 1;

    int last_offset = 0;
    bool saw_pitch = false;
    for (size_t i = 0; i < len; i++) {
        gb_tick_row_t *row = &out->rows[i];
        row->vol = -1;
        row->duty = -1;

        int offset = last_offset;
        if (pitch_lane && i < pitch_lane->count) {
            offset = (int)pitch_lane->values[i];
            last_offset = offset;
            saw_pitch = true;
        } else if (!saw_pitch) {
            offset = 0;
            last_offset = 0;
        }
        row->offset = offset;

        if (vol->count > 0 && i < vol->count) {
            row->vol = (int)vol->values[i];
        }
        if (duty->count > 0 && i < duty->count) {
            row->duty = (int)duty->values[i];
        }
    }

    out->count = (int)len;
    out->loop_to = loop_to;
    out->enabled = true;
}

GB_RAMFN const gb_tick_row_t *gb_tick_program_row(const gb_tick_program_t *p, int tick) {
    if (!p || !p->enabled || p->count <= 0) {
        return NULL;
    }
    if (tick < 0) {
        tick = 0;
    }
    if (tick < p->count) {
        return &p->rows[tick];
    }
    if (p->loop_to >= 0 && p->loop_to < p->count) {
        int cycle_len = p->count - p->loop_to;
        return &p->rows[p->loop_to + (tick - p->count) % cycle_len];
    }
    return &HALT_ROW;
}
