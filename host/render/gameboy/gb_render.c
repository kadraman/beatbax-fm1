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

#include "gb_render.h"
#include <string.h>

void gb_render_init(gb_render_state_t *st, const bax_song_t *song, uint32_t sample_rate) {
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
    st->song = song;
    st->sample_rate = sample_rate;
}

void gb_render_block(gb_render_state_t *st, gb_sample_t *out, uint32_t frame_count) {
    if (!st || !out || frame_count == 0) {
        return;
    }

    /*
     * Phase 0: silence. The voice cores and scheduler/expansion tick land in
     * Phase 1 (T010-T014); until then the renderer outputs silence while
     * remaining deterministic and allocation-free in the hot path.
     */
    memset(out, 0, (size_t)frame_count * sizeof(gb_sample_t));
    st->frame += frame_count;
}
