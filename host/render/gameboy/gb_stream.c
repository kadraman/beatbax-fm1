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
 * Streaming scheduler (feature 004). Float-free, allocation-free; the hot path
 * is meant to live in SRAM on the device. The per-sample voice math is shared
 * with the offline renderer via `gb_note_begin_desc` / `gb_note_step`.
 */

#include "gb_stream.h"

#include <string.h>

/* Assemble a resolved note descriptor from a compiled instrument + effect. */
static GB_RAMFN void gb_desc_from_compiled(gb_note_desc_t *desc, const gb_song_inst_t *inst,
                                  const gb_song_fx_t *fx) {
    memset(desc, 0, sizeof(*desc));
    desc->voice = inst->voice;
    desc->duty_x10 = inst->duty_x10;
    desc->env_initial = inst->env_initial;
    desc->env_period = inst->env_period;
    desc->env_dir = inst->env_dir;
    desc->sweep_present = inst->sweep_present;
    desc->sweep_time = inst->sweep_time;
    desc->sweep_dir = inst->sweep_dir;
    desc->sweep_shift = inst->sweep_shift;
    memcpy(desc->wave_table, inst->wave_table, sizeof(desc->wave_table));
    desc->wave_sum = inst->wave_sum;
    desc->wave_volume = inst->wave_volume;
    desc->noise_width = inst->noise_width;
    desc->noise_note_index = inst->noise_note_index;
    desc->noise_shift = inst->noise_shift;
    desc->noise_divisor = inst->noise_divisor;
    desc->prog = inst->prog;
    if (fx) {
        desc->vib_depth = fx->vib_depth;
        desc->vib_rate = fx->vib_rate;
        desc->vib_waveform = fx->vib_waveform;
        desc->vib_duration_ms = fx->vib_duration_ms;
        desc->vib_delay_ms = fx->vib_delay_ms;
        for (int i = 0; i < 4; i++) desc->arp_semis[i] = fx->arp_semis[i];
        desc->arp_count = fx->arp_count;
        desc->port_speed = fx->port_speed;
        desc->bend_semi_q16 = fx->bend_semi_q16;
        desc->bend_delay_ms = fx->bend_delay_ms;
        desc->bend_time_ms = fx->bend_time_ms;
        memcpy(desc->bend_curve, fx->bend_curve, sizeof(desc->bend_curve));
    }
}

/* True if the tick program writes volume anywhere (mirrors the file-local
 * prog_has_volume in the renderer). */
static bool prog_has_volume(const gb_tick_program_t *p) {
    for (int i = 0; i < p->count; i++) {
        if (p->rows[i].vol >= 0) {
            return true;
        }
    }
    return false;
}

/* Release-tail length for a compiled event, matching gb_note_begin_desc. */
static uint32_t event_tail(const gb_song_blob_t *blob, uint32_t ev_idx) {
    const gb_song_event_t *ev = &blob->events[ev_idx];
    const gb_song_inst_t *inst = &blob->insts[ev->inst];
    uint32_t sr = blob->sample_rate;
    switch (inst->voice) {
        case BAX_VOICE_PULSE1:
        case BAX_VOICE_PULSE2:
            return ev->hold ? 0u : gb_ms_to_samples(GB_NOTE_RELEASE_MS, sr);
        case BAX_VOICE_NOISE:
            return gb_ms_to_samples(GB_NOTE_RELEASE_MS, sr);
        case BAX_VOICE_WAVE:
            return (inst->env_period > 0 || (inst->prog.enabled && prog_has_volume(&inst->prog)))
                       ? gb_ms_to_samples(GB_NOTE_RELEASE_MS, sr)
                       : gb_ms_to_samples(GB_WAVE_TAIL_MS, sr);
        default:
            return 0;
    }
}

int gb_stream_validate(const gb_song_blob_t *blob) {
    if (!blob) {
        return 1;
    }
    if (blob->magic != GB_BLOB_MAGIC) {
        return 2;
    }
    if (blob->version != GB_BLOB_VERSION) {
        return 3;
    }
    if (!blob->insts || blob->inst_count == 0) {
        return 4;
    }
    if (!blob->events || blob->event_count == 0) {
        return 5;
    }

    /* Reject schedules whose overlapping notes (including release tails) would
     * exhaust the active-note pool: the scheduler would silently drop the extra
     * notes and diverge from the offline renderer. */
    uint32_t max_active = 0;
    for (uint32_t i = 0; i < blob->event_count; i++) {
        uint32_t t = blob->events[i].start;
        uint32_t active = 0;
        for (uint32_t j = 0; j < blob->event_count; j++) {
            uint32_t js = blob->events[j].start;
            uint32_t je = js + blob->events[j].len + event_tail(blob, j);
            if (js <= t && t < je) {
                active++;
            }
        }
        if (active > max_active) {
            max_active = active;
        }
    }
    if (max_active > GB_STREAM_MAX_ACTIVE) {
        return 6;
    }

    return 0;
}

GB_RAMFN void gb_stream_init(gb_stream_t *st, const gb_song_blob_t *blob) {
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
    st->blob = blob;
    gb_channel_state_init(&st->chan);
}

GB_RAMFN void gb_stream_play(gb_stream_t *st) {
    if (!st || !st->blob) {
        return;
    }
    if (st->frame >= st->blob->total_frames) {
        gb_stream_stop(st); /* restart from the beginning */
    }
    st->playing = 1;
}

GB_RAMFN void gb_stream_stop(gb_stream_t *st) {
    if (!st) {
        return;
    }
    st->playing = 0;
    st->frame = 0;
    st->next_event = 0;
    memset(st->active_valid, 0, sizeof(st->active_valid));
    gb_channel_state_init(&st->chan);
}

GB_RAMFN void gb_stream_render_block(gb_stream_t *st, gb_sample_t *out, uint32_t nframes) {
    if (!st || !out || nframes == 0) {
        return;
    }
    if (nframes > GB_STREAM_MAX_BLOCK) {
        nframes = GB_STREAM_MAX_BLOCK;
    }

    const gb_song_blob_t *blob = st->blob;
    if (!blob || !st->playing || st->frame >= blob->total_frames) {
        memset(out, 0, (size_t)nframes * sizeof(gb_sample_t));
        if (blob && st->playing && st->frame >= blob->total_frames) {
            st->playing = 0;
        }
        return;
    }

    uint32_t start = (uint32_t)st->frame;
    uint32_t end = start + nframes;
    if (end > blob->total_frames) {
        end = blob->total_frames;
    }

    int32_t mix[GB_STREAM_MAX_BLOCK];
    gb_sample_t tmp[GB_STREAM_MAX_BLOCK];
    memset(mix, 0, (size_t)nframes * sizeof(int32_t));

    /*
     * Pass 1: render the continuation of notes that were already active at the
     * start of this block (their note-on is strictly before `start`).
     */
    for (int s = 0; s < GB_STREAM_MAX_ACTIVE; s++) {
        if (!st->active_valid[s]) {
            continue;
        }
        gb_note_t *n = &st->active[s];
        uint32_t n_end = st->active_start[s] + n->len;
        if (n_end <= start) {
            st->active_valid[s] = 0;
            continue;
        }
        uint32_t s1 = (end < n_end) ? end : n_end;
        uint32_t count = s1 - start;
        gb_note_step(n, tmp, count);
        for (uint32_t i = 0; i < count; i++) {
            mix[i] += tmp[i];
        }
        if (n_end <= end) {
            st->active_valid[s] = 0;
        }
    }

    /*
     * Pass 2: begin events whose note-on falls in [start, end) and render their
     * overlap immediately. A note's release tail may still be sounding from a
     * previous slot on the same channel, so allocate a free slot (up to two per
     * channel) instead of assuming the channel is free.
     */
    while (st->next_event < blob->event_count && blob->events[st->next_event].start < end) {
        const gb_song_event_t *ev = &blob->events[st->next_event];
        int c = ev->channel;
        const gb_song_inst_t *inst = &blob->insts[ev->inst];
        const gb_song_fx_t *fx = (ev->fx != GB_BLOB_FX_NONE) ? &blob->fxs[ev->fx] : NULL;

        int slot = -1;
        for (int s = 0; s < GB_STREAM_MAX_ACTIVE; s++) {
            if (!st->active_valid[s]) {
                slot = s;
                break;
            }
        }
        if (slot < 0) {
            /* Pool exhausted: a pathological song overlaps more notes than the
             * pool allows (notes shorter than the 20 ms tail). Skip rather than
             * corrupt state; the bit-identity test will flag any real case. */
            st->next_event++;
            continue;
        }

        gb_note_desc_t desc;
        gb_desc_from_compiled(&desc, inst, fx);
        gb_note_begin_desc(&st->active[slot], &desc, ev->midi, ev->len,
                           blob->sample_rate, ev->vol_override, &st->chan, c,
                           ev->join != 0, ev->hold != 0, ev->start);
        st->active_start[slot] = ev->start;
        st->active_valid[slot] = 1;
        st->next_event++;

        uint32_t n_end = ev->start + st->active[slot].len;
        uint32_t s0 = ev->start; /* >= start */
        uint32_t s1 = (end < n_end) ? end : n_end;
        uint32_t count = s1 - s0;
        gb_note_step(&st->active[slot], tmp, count);
        for (uint32_t i = 0; i < count; i++) {
            mix[(s0 - start) + i] += tmp[i];
        }
        if (n_end <= end) {
            st->active_valid[slot] = 0;
        }
    }

    /* 3. Finalize: apply the baked peak scale + clamp (identical to offline). */
    int32_t scale = (int32_t)blob->scale;
    for (uint32_t i = 0; i < nframes; i++) {
        int32_t v = mix[i];
        if (scale != 0) {
            v = (int32_t)(((int64_t)v * scale) >> 15);
        }
        if (v > 32767) {
            v = 32767;
        } else if (v < -32768) {
            v = -32768;
        }
        out[i] = (gb_sample_t)v;
    }

    st->frame = end;
    if (st->frame >= blob->total_frames) {
        st->playing = 0;
    }
}
