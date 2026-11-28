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
 * Host-only compiled-song builder (feature 004). Lowers a parsed song into the
 * fixed-point blob the firmware streams, and bakes the peak-limit scale so the
 * device output is sample-identical to the offline host render.
 */

#include "gb_embed.h"

#include "gb_expand.h"
#include "gb_note_desc.h"
#include "gb_song_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void inst_from_desc(gb_song_inst_t *o, const gb_note_desc_t *d) {
    memset(o, 0, sizeof(*o));
    o->voice = d->voice;
    o->duty_x10 = d->duty_x10;
    o->env_initial = d->env_initial;
    o->env_period = d->env_period;
    o->env_dir = d->env_dir;
    o->sweep_present = d->sweep_present;
    o->sweep_time = d->sweep_time;
    o->sweep_dir = d->sweep_dir;
    o->sweep_shift = d->sweep_shift;
    memcpy(o->wave_table, d->wave_table, sizeof(o->wave_table));
    o->wave_sum = d->wave_sum;
    o->wave_volume = d->wave_volume;
    o->noise_width = d->noise_width;
    o->noise_note_index = d->noise_note_index;
    o->noise_shift = d->noise_shift;
    o->noise_divisor = d->noise_divisor;
    o->prog = d->prog;
}

static void fx_from_song(gb_song_fx_t *o, const bax_song_t *song, const char *effect_name) {
    memset(o, 0, sizeof(*o));
    if (!effect_name || !effect_name[0] || song->instrument_count == 0) {
        return;
    }
    /* Effect resolution is independent of the instrument; use a valid dummy. */
    gb_note_desc_t d;
    gb_note_desc_from_song(&d, song, &song->instruments[0], effect_name);
    o->vib_depth = d.vib_depth;
    o->vib_rate = d.vib_rate;
    o->vib_waveform = d.vib_waveform;
    o->vib_duration_ms = d.vib_duration_ms;
    o->vib_delay_ms = d.vib_delay_ms;
    for (int i = 0; i < 4; i++) o->arp_semis[i] = d.arp_semis[i];
    o->arp_count = d.arp_count;
    o->port_speed = d.port_speed;
    o->bend_semi_q16 = d.bend_semi_q16;
    o->bend_delay_ms = d.bend_delay_ms;
    o->bend_time_ms = d.bend_time_ms;
    memcpy(o->bend_curve, d.bend_curve, sizeof(o->bend_curve));
}

bool gb_blob_build(const bax_song_t *song, uint32_t sample_rate,
                   gb_song_blob_t *out, char *errbuf, size_t errbuf_len) {
    memset(out, 0, sizeof(*out));

    gb_resolved_list_t list;
    uint32_t total = 0;
    if (!gb_song_resolve(song, sample_rate, &list, &total, errbuf, errbuf_len)) {
        return false;
    }

    size_t n_inst = song->instrument_count;
    size_t n_fx = song->effect_count;
    size_t n_ev = list.count;

    gb_song_inst_t *insts = n_inst ? (gb_song_inst_t *)calloc(n_inst, sizeof(gb_song_inst_t)) : NULL;
    gb_song_fx_t *fxs = n_fx ? (gb_song_fx_t *)calloc(n_fx, sizeof(gb_song_fx_t)) : NULL;
    gb_song_event_t *events = n_ev ? (gb_song_event_t *)malloc(n_ev * sizeof(gb_song_event_t)) : NULL;

    if ((n_inst && !insts) || (n_fx && !fxs) || (n_ev && !events)) {
        free(insts);
        free(fxs);
        free(events);
        gb_resolved_list_free(&list);
        if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "out of memory");
        return false;
    }

    for (size_t i = 0; i < n_inst; i++) {
        gb_note_desc_t d;
        gb_note_desc_from_song(&d, song, &song->instruments[i], "");
        inst_from_desc(&insts[i], &d);
    }
    for (size_t i = 0; i < n_fx; i++) {
        fx_from_song(&fxs[i], song, song->effects[i].name);
    }
    for (size_t i = 0; i < n_ev; i++) {
        const gb_resolved_event_t *re = &list.items[i];
        events[i].start = re->start;
        events[i].len = re->len;
        events[i].midi = (int16_t)re->midi;
        events[i].inst = (uint8_t)re->inst_index;
        events[i].fx = re->fx_index >= 0 ? (uint8_t)re->fx_index : GB_BLOB_FX_NONE;
        events[i].channel = (uint8_t)re->channel;
        events[i].join = re->join ? 1u : 0u;
        events[i].hold = re->hold ? 1u : 0u;
        events[i].vol_override = (int8_t)re->vol_override;
    }

    const char *nm = song->name[0] ? song->name : "song";
    char *name = (char *)malloc(strlen(nm) + 1);
    if (!name) {
        free(insts);
        free(fxs);
        free(events);
        gb_resolved_list_free(&list);
        if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "out of memory");
        return false;
    }
    strcpy(name, nm);

    gb_resolved_list_free(&list);

    out->magic = GB_BLOB_MAGIC;
    out->version = GB_BLOB_VERSION;
    out->sample_rate = sample_rate;
    out->total_frames = total;
    out->scale = (uint32_t)gb_song_peak_scale_q15(song, sample_rate);
    out->bpm = song->bpm > 0 ? (uint32_t)song->bpm : 128;
    out->name = name;
    out->insts = insts;
    out->inst_count = (uint16_t)n_inst;
    out->fxs = fxs;
    out->fx_count = (uint16_t)n_fx;
    out->events = events;
    out->event_count = (uint32_t)n_ev;

    /* Refuse a schedule that would exhaust the streaming pool: the scheduler
     * silently drops the extra notes, diverging from the offline renderer. */
    if (gb_stream_validate(out) != 0) {
        gb_blob_free(out);
        if (errbuf && errbuf_len) {
            snprintf(errbuf, errbuf_len, "schedule exceeds active-note pool");
        }
        return false;
    }

    return true;
}

void gb_blob_free(gb_song_blob_t *blob) {
    if (!blob) {
        return;
    }
    free((void *)blob->name);
    free((void *)blob->insts);
    free((void *)blob->fxs);
    free((void *)blob->events);
    memset(blob, 0, sizeof(*blob));
}
