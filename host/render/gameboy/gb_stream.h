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

#ifndef BAX_GB_STREAM_H
#define BAX_GB_STREAM_H

#include "gb_note_render.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compiled-song streaming renderer (feature 004).
 *
 * A host tool lowers a `.bax` song into a `gb_song_blob_t` (instrument and
 * effect descriptors + a flat, start-sorted event schedule + a baked peak
 * scale). The firmware streams that blob block-by-block with no parser, no
 * heap, and no float. Output is sample-identical to the offline host renderer
 * at the blob's `sample_rate`.
 */

#define GB_BLOB_MAGIC 0x42415834u /* "BAX4" */
#define GB_BLOB_VERSION 4u
#define GB_BLOB_FX_NONE 0xFFu
#define GB_STREAM_MAX_BLOCK 256u
/* Up to two notes per channel can overlap (a release tail ringing into the next
 * note-on). Notes are never shorter than one step, which exceeds the 20 ms tail
 * at any realistic BPM, so two slots per channel is sufficient. */
#define GB_STREAM_MAX_ACTIVE (BAX_MAX_CHANNELS * 2)

/* Lowered instrument (voice + parameters + macro program). */
typedef struct {
    uint8_t voice;          /* BAX_VOICE_PULSE1 / PULSE2 / WAVE / NOISE */
    uint32_t duty_x10;      /* pulse: 125 / 250 / 500 / 750 */
    uint8_t env_initial;    /* pulse + noise */
    uint8_t env_period;
    uint8_t env_dir;        /* GB_ENV_* */
    uint8_t sweep_present;  /* pulse 1 */
    uint8_t sweep_time;
    uint8_t sweep_dir;      /* BAX_SWEEP_DIR_* */
    uint8_t sweep_shift;
    uint8_t wave_table[32]; /* wave */
    int wave_sum;
    uint8_t wave_volume;    /* wave: 0/25/50/100 */
    uint8_t noise_width;    /* noise: 7 or 15 */
    int noise_note_index;   /* noise clock base */
    uint8_t noise_shift;
    uint8_t noise_divisor;
    gb_tick_program_t prog; /* lowered macros */
} gb_song_inst_t;

/* Lowered named effect. */
typedef struct {
    int vib_depth;
    int vib_rate;
    uint8_t vib_waveform;
    int vib_duration_ms;
    int vib_delay_ms;
    int arp_semis[4];
    int arp_count;
    int port_speed;
    int32_t bend_semi_q16;
    int bend_delay_ms;
    int bend_time_ms;
    char bend_curve[8];
} gb_song_fx_t;

/* One note event (start-sorted). */
typedef struct {
    uint32_t start;   /* sample index */
    uint32_t len;     /* samples */
    int16_t midi;     /* resolved MIDI */
    uint8_t inst;     /* index into insts[] */
    uint8_t fx;       /* index into fxs[], or GB_BLOB_FX_NONE */
    uint8_t channel;  /* 0..3 */
    uint8_t join;     /* joins the previous note's held pulse portamento voice */
    uint8_t hold;     /* holds this pulse portamento voice for the next note */
    int8_t vol_override; /* -1 = none */
} gb_song_event_t;

/* The compiled song. Pointers must outlive the stream state. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t sample_rate;
    uint32_t total_frames;
    uint32_t scale;    /* Q15 peak scale (0 = none) */
    uint32_t bpm;
    const char *name;
    const gb_song_inst_t *insts;
    uint16_t inst_count;
    const gb_song_fx_t *fxs;
    uint16_t fx_count;
    const gb_song_event_t *events;
    uint32_t event_count;
} gb_song_blob_t;

/* Streaming scheduler state (owned by the caller; keep it in RAM). */
typedef struct {
    const gb_song_blob_t *blob;
    uint64_t frame;      /* monotonic playhead (output frames rendered) */
    uint32_t next_event; /* next event index to begin */
    uint8_t playing;
    gb_note_t active[GB_STREAM_MAX_ACTIVE];
    uint8_t active_valid[GB_STREAM_MAX_ACTIVE];
    uint32_t active_start[GB_STREAM_MAX_ACTIVE];
    gb_channel_state_t chan;
} gb_stream_t;

/* Validate a blob (magic/version/counts, and that no schedule point exceeds the
 * active-note pool). Returns 0 on success, else an error code:
 *   1 null blob, 2 bad magic, 3 bad version, 4 no instruments, 5 no events,
 *   6 schedule exceeds the active-note pool, 7 event instrument index out of
 *   range. */
int gb_stream_validate(const gb_song_blob_t *blob);

/* Bind a stream to a blob and reset transport (stopped, at the start). */
void gb_stream_init(gb_stream_t *st, const gb_song_blob_t *blob);

/* Start playing (restarts from the beginning if already at the end). */
void gb_stream_play(gb_stream_t *st);

/* Stop and rewind to the beginning. */
void gb_stream_stop(gb_stream_t *st);

/*
 * Render up to `nframes` (<= GB_STREAM_MAX_BLOCK) mono samples into `out`.
 * Outputs silence when stopped or past the end; the baked peak scale + clamp
 * are applied exactly as in the offline renderer.
 */
void gb_stream_render_block(gb_stream_t *st, gb_sample_t *out, uint32_t nframes);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_STREAM_H */
