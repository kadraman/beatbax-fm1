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

#ifndef BAX_GB_NOTE_RENDER_H
#define BAX_GB_NOTE_RENDER_H

#include "gb_macro.h"
#include "gb_render.h"
#include "song_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Release tail kinds (see gb_note_begin_desc). */
#define GB_RELEASE_NONE 0
#define GB_RELEASE_LINEAR 1
#define GB_RELEASE_EXPONENTIAL 2

/* Vibrato LFO waveforms (lowered from the `vib:…,waveform` string). */
#define GB_VIB_SINE 0
#define GB_VIB_SQUARE 1
#define GB_VIB_SAW 2
#define GB_VIB_TRIANGLE 3

/* Per-channel persistent state for the full render (phase, vibrato LFO, portamento). */
typedef struct {
    gb_phase_t pulse_phase[BAX_MAX_CHANNELS];
    gb_phase_t vib_phase[BAX_MAX_CHANNELS];
    int32_t last_freq_q16[BAX_MAX_CHANNELS];
    int16_t env_last_q15[BAX_MAX_CHANNELS]; /* last pulse envelope level (legato sustain) */
} gb_channel_state_t;

void gb_channel_state_init(gb_channel_state_t *st);

/* log2(v) in Q16 (float-free) for v > 0. Used by the exponential portamento. */
int32_t gb_log2_q16(uint32_t v);

/* 2^(semi_q16 / 2^16 / 12) in Q16 (float-free). Inverse of gb_log2_q16 (×12). */
int32_t gb_pow2_semi_q16(int32_t semi_q16);

/*
 * Fully resolved, float-free description of one note's instrument + effect.
 *
 * This is the shared contract between the host (which lowers the 002 song model
 * into it, using float where the host already does) and the firmware (which reads
 * pre-lowered descriptors from the compiled song blob). Nothing in this struct or
 * in the sample-generation loop requires float/double.
 */
typedef struct {
    uint8_t voice;          /* BAX_VOICE_PULSE1 / PULSE2 / WAVE / NOISE */
    uint32_t duty_x10;      /* pulse: 125 / 250 / 500 / 750 */
    uint8_t env_initial;    /* pulse + noise volume envelope */
    uint8_t env_period;
    uint8_t env_dir;        /* GB_ENV_DOWN / UP / FLAT */
    uint8_t sweep_present;  /* pulse 1 only */
    uint8_t sweep_time;
    uint8_t sweep_dir;      /* BAX_SWEEP_DIR_UP / DOWN */
    uint8_t sweep_shift;
    uint8_t wave_table[32]; /* wave: 32 nibbles, tiled */
    int wave_sum;           /* wave: integer sum of the 32 nibbles */
    uint8_t wave_volume;    /* wave: 0 / 25 / 50 / 100 */
    uint8_t noise_width;    /* noise: 7 or 15 */
    int noise_note_index;   /* noise: uge note index 0..72 (clock base) */
    uint8_t noise_shift;    /* noise: NR43 shift */
    uint8_t noise_divisor;  /* noise: NR43 divisor */
    gb_tick_program_t prog; /* lowered vol/pitch/duty/arp macros */
    /* Named effect parameters (resolved). */
    int vib_depth;
    int vib_rate;
    uint8_t vib_waveform;   /* GB_VIB_* */
    int vib_duration_ms;    /* -1 = unset (whole note) */
    int vib_delay_ms;       /* -1 = unset (no onset delay) */
    int arp_semis[4];
    int arp_count;
    int port_speed;
    int32_t bend_semi_q16;  /* bend semitones in Q16 (fractional allowed) */
    int bend_delay_ms;      /* -1 = default (50% of note) */
    int bend_time_ms;       /* -1 = default (remaining) */
    char bend_curve[8];
} gb_note_desc_t;

/*
 * Stateful runtime context for a single sounding note. Produced by
 * `gb_note_begin_desc` and advanced sample-by-sample by `gb_note_step`, so the
 * same sample-generation loop serves both the offline host render and the device
 * streaming render without heap allocation or float arithmetic.
 */
typedef struct {
    uint8_t kind;           /* 0 pulse, 1 wave, 2 noise */
    uint32_t len;           /* total render length in samples (nominal + release tail) */
    uint32_t pos;           /* note-local index of the next sample to emit */
    uint32_t sample_rate;

    /* pulse */
    uint32_t current_reg;
    uint32_t duty_x10;
    uint32_t sweep_interval;
    uint8_t sweep_shift;
    uint8_t sweep_dir;
    bool has_sweep;
    gb_phase_t phase;       /* local pulse phase; saved to channel state at nominal end */
    gb_phase_t vib_phase;   /* local vibrato LFO phase (same rule) */

    /* wave */
    uint8_t wave_table[32];
    int wave_sum;
    int vol_div;
    int32_t base_freq;
    gb_phase_t wave_phase;  /* local wave phase accumulator (integrates portamento) */

    /* noise */
    int noise_base_index;
    uint8_t noise_shift;
    uint8_t noise_divisor;
    bool width7;
    uint64_t lfsr_inc;
    uint64_t nphase;
    uint16_t lfsr;

    /* common envelope (pulse + noise) */
    uint8_t env_initial;
    uint8_t env_period;
    uint8_t env_dir;

    /* macro tick program (owned by value) */
    gb_tick_program_t prog;
    bool use_prog_vol;
    int last_tick;
    int current_offset;
    int vol_scale;

    /* named effect (copied) */
    int vib_depth;
    int vib_rate;
    uint8_t vib_waveform;
    int arp_semis[4];
    int arp_count;
    int port_speed;
    int32_t bend_semi_q16;
    int bend_delay_ms;
    int bend_time_ms;
    char bend_curve[8];

    /* derived effect timing (sample counts) */
    int32_t port_dur_samples;
    int32_t bend_delay_samples;
    int32_t bend_time_samples;
    int32_t vib_duration_samples; /* -1 = unset (whole note) */
    int32_t vib_delay_samples;    /* 0 = none */

    int vol_override;
    int32_t last_freq;      /* captured at begin (previous note on this channel) */
    int32_t log2_last_q16;  /* log2(last_freq) in Q16 (exponential portamento) */

    /* release tail (spec-112 parity) */
    uint32_t nominal_len;   /* note length before the release tail */
    uint8_t release_kind;   /* GB_RELEASE_NONE / LINEAR / EXPONENTIAL */
    int16_t release_floor_q15; /* exponential floor (0.0001/baseGain), Q15 */
    int32_t release_exp_q30;   /* exponential accumulator (1.0 in Q30 at fade start) */

    /* pulse portamento voice-holding (spec-112 parity) */
    bool join;              /* join the previous note's held pulse voice */
    bool hold;              /* hold this pulse voice for the next note (no fade) */
    uint32_t chrome_default; /* leading samples played at 440 Hz (Chromium startup) */
    uint32_t start_off;      /* start_sample % 128 (wave portamento quantum) */

    gb_channel_state_t *chan;
    int channel;

    /* cached phase increment (pulse): recompute only when osc_freq changes */
    int32_t cached_osc_freq;
    uint32_t cached_inc;

    /* incremental envelope (pulse + noise); env_den == 0 means a constant level */
    uint32_t env_den;         /* period * sample_rate */
    uint32_t env_q;           /* 4194304 / env_den */
    uint32_t env_r;           /* 4194304 % env_den */
    uint32_t env_nsteps_q16;  /* clamped step count (Q16) */
    uint32_t env_step_q16;    /* current step position (Q16) */
    uint32_t env_rem;         /* bresenham remainder */

    /* incremental effect ramps (avoid per-sample 64-bit division) */
    int arp_frame;            /* arp 60 Hz frame counter */
    int arp_semi;             /* cached arp semitone offset */
    uint32_t arp_rem;         /* arp remainder (r = 60, den = sample_rate) */
    uint32_t port_q;          /* portamento: 65536 / port_dur_samples */
    uint32_t port_r;          /* 65536 % port_dur_samples */
    uint32_t port_p_q16;      /* current portamento position */
    uint32_t port_rem;        /* portamento remainder */
    uint32_t bend_q;          /* bend: 65536 / bend_time_samples */
    uint32_t bend_r;          /* 65536 % bend_time_samples */
    uint32_t bend_p_q16;      /* current bend position */
    uint32_t bend_rem;        /* bend remainder */
} gb_note_t;

/*
 * Begin a note from a resolved descriptor. Captures the channel's current phase /
 * vibrato / last-frequency state and derives effect timing. `midi` is the resolved
 * note; for noise it is ignored. `join`/`hold` carry the pulse portamento
 * voice-holding flags (spec-112 parity). Float-free.
 */
void gb_note_begin_desc(gb_note_t *n, const gb_note_desc_t *desc, int midi,
                        uint32_t len, uint32_t sample_rate, int vol_override,
                        gb_channel_state_t *state, int channel,
                        bool join, bool hold, uint32_t start_sample);

/* Render `count` more samples of `n` into `dst` (mono). Float-free, allocation-free. */
void gb_note_step(gb_note_t *n, gb_sample_t *dst, uint32_t count);

/*
 * Render one note (mono, overwrites `dst[0..n)`) with software macros, named
 * effects, and the release tail. Host path: resolves `effect_name` from `song`,
 * lowers `inst` into a descriptor, then begins + steps. `len` is the nominal note
 * length; returns the full rendered length (nominal + release tail) so the caller
 * mixes the tail too. Kept as the feature 003 API entry point.
 */
uint32_t gb_render_note_full(gb_sample_t *dst, uint32_t len,
                             const bax_song_t *song, int midi,
                             const bax_instrument_t *inst, const char *effect_name,
                             uint32_t sample_rate, int vol_override,
                             gb_channel_state_t *state, int channel,
                             bool join, bool hold, uint32_t start_sample);

#ifdef __cplusplus
}
#endif

#endif /* BAX_GB_NOTE_RENDER_H */
