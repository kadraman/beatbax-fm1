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
 * Offline host renderer (feature 003). Expands + resolves the song via
 * gb_song_resolve, renders each note into a whole-song mix, applies the
 * 0.95 peak ceiling, and converts to int16. Host-only (heap, whole-song buffer).
 */

#include "gb_expand.h"
#include "gb_note_render.h"
#include "gb_song_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool gb_song_render_mono_mask(const bax_song_t *song, uint32_t sample_rate,
                              uint32_t channel_mask,
                              gb_sample_t **out_mono, size_t *out_len,
                              char *errbuf, size_t errbuf_len) {
    if (!song || !out_mono || !out_len) {
        if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "null argument");
        return false;
    }

    gb_resolved_list_t list;
    uint32_t total = 0;
    if (!gb_song_resolve(song, sample_rate, &list, &total, errbuf, errbuf_len)) {
        return false;
    }

    uint32_t max_len = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (channel_mask != 0 && !(channel_mask & (1u << list.items[i].channel))) {
            continue;
        }
        if (list.items[i].len > max_len) {
            max_len = list.items[i].len;
        }
    }

    int32_t *mix = (int32_t *)calloc(total, sizeof(int32_t));
    if (!mix) {
        gb_resolved_list_free(&list);
        if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "out of memory");
        return false;
    }
    uint32_t max_tail = gb_ms_to_samples(GB_WAVE_TAIL_MS, sample_rate);
    uint32_t scratch_len = max_len + max_tail;
    gb_sample_t *scratch = scratch_len > 0 ? (gb_sample_t *)malloc((size_t)scratch_len * sizeof(gb_sample_t)) : NULL;
    if (scratch_len > 0 && !scratch) {
        free(mix);
        gb_resolved_list_free(&list);
        if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "out of memory");
        return false;
    }

    gb_channel_state_t state;
    gb_channel_state_init(&state);

    for (size_t i = 0; i < list.count; i++) {
        const gb_resolved_event_t *ev = &list.items[i];
        if (channel_mask != 0 && !(channel_mask & (1u << ev->channel))) {
            continue;
        }
        const bax_instrument_t *inst = &song->instruments[ev->inst_index];
        const char *fx_name = ev->fx_index >= 0 ? song->effects[ev->fx_index].name : "";
        uint32_t full = gb_render_note_full(scratch, ev->len, song, ev->midi, inst, fx_name,
                                            sample_rate, ev->vol_override, &state, ev->channel,
                                            ev->join, ev->hold, ev->start);
        for (uint32_t s = 0; s < full; s++) {
            mix[ev->start + s] += scratch[s];
        }
    }

    free(scratch);
    gb_resolved_list_free(&list);

    /* Find the complete mix's peak before any 16-bit conversion, then apply the
     * 0.95 peak-limit (PLAYBACK_PEAK_CEILING) and clamp on the final convert. */
    int64_t peak = 0;
    for (size_t s = 0; s < total; s++) {
        int32_t v = mix[s];
        int64_t a = v < 0 ? -(int64_t)v : (int64_t)v;
        if (a > peak) {
            peak = a;
        }
    }
    int32_t scale = 0;
    if (peak > 31130) {
        scale = (int32_t)(((uint64_t)31130u << 15) / (uint64_t)peak);
    }

    gb_sample_t *out = (gb_sample_t *)malloc((size_t)total * sizeof(gb_sample_t));
    if (!out) {
        free(mix);
        if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "out of memory");
        return false;
    }
    for (size_t s = 0; s < total; s++) {
        int32_t v = mix[s];
        if (scale != 0) {
            v = (int32_t)(((int64_t)v * scale) >> 15);
        }
        if (v > 32767) {
            v = 32767;
        } else if (v < -32768) {
            v = -32768;
        }
        out[s] = (gb_sample_t)v;
    }
    free(mix);

    *out_mono = out;
    *out_len = total;
    return true;
}

bool gb_song_render_mono(const bax_song_t *song, uint32_t sample_rate,
                         gb_sample_t **out_mono, size_t *out_len,
                         char *errbuf, size_t errbuf_len) {
    return gb_song_render_mono_mask(song, sample_rate, 0, out_mono, out_len, errbuf, errbuf_len);
}

int32_t gb_song_peak_scale_q15(const bax_song_t *song, uint32_t sample_rate) {
    gb_resolved_list_t list;
    uint32_t total = 0;
    char errbuf[256];
    if (!gb_song_resolve(song, sample_rate, &list, &total, errbuf, sizeof(errbuf))) {
        return 0;
    }

    uint32_t max_len = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].len > max_len) {
            max_len = list.items[i].len;
        }
    }

    int32_t *mix = (int32_t *)calloc(total, sizeof(int32_t));
    if (!mix) {
        gb_resolved_list_free(&list);
        return 0;
    }
    uint32_t max_tail = gb_ms_to_samples(GB_WAVE_TAIL_MS, sample_rate);
    uint32_t scratch_len = max_len + max_tail;
    gb_sample_t *scratch = scratch_len > 0 ? (gb_sample_t *)malloc((size_t)scratch_len * sizeof(gb_sample_t)) : NULL;
    if (scratch_len > 0 && !scratch) {
        free(mix);
        gb_resolved_list_free(&list);
        return 0;
    }

    gb_channel_state_t state;
    gb_channel_state_init(&state);
    for (size_t i = 0; i < list.count; i++) {
        const gb_resolved_event_t *ev = &list.items[i];
        const bax_instrument_t *inst = &song->instruments[ev->inst_index];
        const char *fx_name = ev->fx_index >= 0 ? song->effects[ev->fx_index].name : "";
        uint32_t full = gb_render_note_full(scratch, ev->len, song, ev->midi, inst, fx_name,
                                            sample_rate, ev->vol_override, &state, ev->channel,
                                            ev->join, ev->hold, ev->start);
        for (uint32_t s = 0; s < full; s++) {
            mix[ev->start + s] += scratch[s];
        }
    }
    free(scratch);
    gb_resolved_list_free(&list);

    int64_t peak = 0;
    for (size_t s = 0; s < total; s++) {
        int32_t v = mix[s];
        int64_t a = v < 0 ? -(int64_t)v : (int64_t)v;
        if (a > peak) {
            peak = a;
        }
    }
    free(mix);

    if (peak > 31130) {
        return (int32_t)(((uint64_t)31130u << 15) / (uint64_t)peak);
    }
    return 0;
}
