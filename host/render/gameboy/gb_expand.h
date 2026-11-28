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

#ifndef BAX_GB_EXPAND_H
#define BAX_GB_EXPAND_H

#include "song_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A fully resolved, renderable event. Shared by the offline renderer and the
 * compiled-song embedder so both see identical schedules (bit-identity by
 * construction).
 */
typedef struct {
    uint32_t start;   /* sample index of note-on */
    uint32_t len;     /* final samples (noise hardware length applied, clamped) */
    int midi;         /* resolved MIDI; noise events are 0 */
    int inst_index;   /* index into song->instruments */
    int fx_index;     /* index into song->effects, or -1 */
    int vol_override; /* 0..15 or -1 */
    int channel;      /* 0..3 */
    bool join;        /* joins the previous note's held pulse portamento voice */
    bool hold;        /* holds this pulse portamento voice for the next note */
} gb_resolved_event_t;

typedef struct {
    gb_resolved_event_t *items;
    size_t count;
    size_t cap;
} gb_resolved_list_t;

/*
 * Expand a song into its flat event stream and resolve each event (instrument,
 * effect, MIDI, noise hardware length). Events are returned sorted by
 * (start, channel). `total` is the output length in samples. Returns false with
 * a named reason in `errbuf` on OOM or an unplayable song.
 */
bool gb_song_resolve(const bax_song_t *song, uint32_t sample_rate,
                     gb_resolved_list_t *list, uint32_t *total,
                     char *errbuf, size_t errbuf_len);

void gb_resolved_list_free(gb_resolved_list_t *list);

/* Resolve a simple note string ("C4", "B#4", "Cb4") to a MIDI number, or -1. */
int gb_note_str_to_midi(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_EXPAND_H */
