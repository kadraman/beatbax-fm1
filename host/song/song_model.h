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

#ifndef BAX_SONG_MODEL_H
#define BAX_SONG_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BAX_MAX_NAME_LEN 64
#define BAX_MAX_STR_LEN 256
#define BAX_MAX_DESC_LEN 1024
#define BAX_MAX_MACRO_LEN 64
#define BAX_MAX_WAVE_SAMPLES 32
#define BAX_MAX_CHANNELS 4

typedef enum {
    BAX_CHIP_UNKNOWN = 0,
    BAX_CHIP_GAMEBOY
} bax_chip_t;

typedef enum {
    BAX_VOICE_UNKNOWN = 0,
    BAX_VOICE_PULSE1,
    BAX_VOICE_PULSE2,
    BAX_VOICE_WAVE,
    BAX_VOICE_NOISE
} bax_voice_type_t;

typedef enum {
    BAX_ENV_DIR_DOWN = 0,
    BAX_ENV_DIR_UP,
    BAX_ENV_DIR_FLAT
} bax_env_direction_t;

typedef enum {
    BAX_SWEEP_DIR_UP = 0,
    BAX_SWEEP_DIR_DOWN
} bax_sweep_direction_t;

typedef struct {
    bool present;
    uint8_t level;       /* 0..15 */
    bax_env_direction_t direction;
    uint8_t period;      /* 0..7 */
} bax_gb_env_t;

typedef struct {
    bool present;
    uint8_t time;        /* 0..7 */
    bax_sweep_direction_t direction;
    uint8_t shift;       /* 0..7 */
} bax_gb_sweep_t;

typedef struct {
    uint8_t samples[BAX_MAX_WAVE_SAMPLES];
    size_t count;        /* 16 or 32 */
} bax_gb_wave_t;

typedef struct {
    int8_t values[BAX_MAX_MACRO_LEN];
    size_t count;
    int loop_point;      /* -1 if no loop */
} bax_macro_env_t;

typedef struct {
    char name[BAX_MAX_NAME_LEN];
    bax_voice_type_t type;

    /* Pulse parameters */
    float duty;          /* 12.5, 25.0, 50.0, 75.0 */
    bax_gb_env_t env;
    bax_gb_sweep_t sweep;

    /* Wave parameters */
    uint8_t wave_volume; /* 0, 25, 50, 100 */
    bax_gb_wave_t wave;

    /* Noise parameters */
    uint8_t noise_width; /* 7 or 15 */
    char uge_note[16];   /* e.g. "C-6", "C-7" */
    char note[16];

    /* Common parameters */
    int length;          /* sound length counter; -1 if unset */
    int vol;             /* constant volume; -1 if unset */
    int gm;              /* general midi hint; -1 if unset */
    char subpat[BAX_MAX_NAME_LEN];

    bax_macro_env_t vol_env;
    bax_macro_env_t pitch_env;
    bax_macro_env_t duty_env;
    bax_macro_env_t arp_env;

    /* Pan (generic pan= or GB NR51 gb:pan=) */
    char pan[16];          /* "L", "R", "C", or numeric string; "" if unset */
    bool pan_gb;           /* true if authored as gb:pan= */

    /* Raw options preservation for formatting */
    bool has_env_object_syntax;
    bool has_sweep_object_syntax;
} bax_instrument_t;

typedef enum {
    BAX_EFFECT_NONE = 0,
    BAX_EFFECT_VIBRATO,
    BAX_EFFECT_ARPEGGIO,
    BAX_EFFECT_PORTAMENTO,
    BAX_EFFECT_BEND,
    BAX_EFFECT_OTHER
} bax_effect_type_t;

typedef struct {
    char name[BAX_MAX_NAME_LEN];
    bax_effect_type_t type;
    bool is_inline;      /* auto-registered inline `<vib:…>` expression (not a declaration) */
    char raw_expr[BAX_MAX_STR_LEN];
    union {
        struct {
            int speed;          /* first param = depth (0–15) */
            int depth;          /* second param = rate (Hz) */
            char waveform[16];  /* third param (sine/square/saw/triangle) */
            int duration_rows;  /* fourth param (0 = unset) */
            int delay_rows;     /* fifth param (0 = immediate) */
        } vib;
        struct {
            int semi1;
            int semi2;
            int semi3;
        } arp;
        struct {
            int speed;
        } port;
    } params;
} bax_effect_t;

typedef struct {
    char raw_line[BAX_MAX_STR_LEN];
} bax_subpat_step_t;

typedef struct {
    char name[BAX_MAX_NAME_LEN];
    bax_subpat_step_t *steps;
    size_t step_count;
    size_t step_capacity;
} bax_subpat_t;

typedef enum {
    BAX_TOKEN_REST = 0,
    BAX_TOKEN_HOLD,
    BAX_TOKEN_NOTE,
    BAX_TOKEN_INLINE_INST,
    BAX_TOKEN_INLINE_VOL
} bax_token_kind_t;

typedef struct {
    bax_token_kind_t kind;
    char text[BAX_MAX_NAME_LEN];      /* note name, inst name, or token text */
    int semitone;                     /* 0..11, or -1 if non-note */
    int octave;                       /* 0..8, or -1 if non-note */
    int duration;                     /* e.g. :4, :16 (default 1) */
    char effect_name[BAX_MAX_NAME_LEN];
    int effect_param;                 /* duration or parameter */
    int vol_level;
} bax_pat_token_t;

typedef struct {
    char name[BAX_MAX_NAME_LEN];
    bax_pat_token_t *tokens;
    size_t token_count;
    size_t token_capacity;
    char *rhs;                        /* raw pattern RHS (authoritative for round-trip); NULL if reconstructed from tokens */
} bax_pattern_t;

typedef struct {
    char pattern_name[BAX_MAX_NAME_LEN];
    char inst_override[BAX_MAX_NAME_LEN];
    int oct_offset;                   /* 0 if none */
    int vol_override;                 /* -1 if none */
    char effect[BAX_MAX_NAME_LEN];    /* `pattern:effect` fallback named effect ("" if none) */
    char raw[BAX_MAX_STR_LEN];        /* full item text (modifiers preserved verbatim) */
} bax_seq_item_t;

typedef struct {
    char name[BAX_MAX_NAME_LEN];
    bax_seq_item_t *items;
    size_t item_count;
    size_t item_capacity;
} bax_sequence_t;

typedef struct {
    int channel_num;                  /* 1..4 */
    char default_inst[BAX_MAX_NAME_LEN];
    char **ref_names;                 /* `seq <refs>` or `pat <refs>` (modifiers preserved verbatim) */
    size_t ref_count;
    size_t ref_capacity;
    bool ref_is_pat;                  /* true if bound with `pat`, false if `seq` */
    char role[BAX_MAX_NAME_LEN];      /* advanced role binding (e.g. "mel") */
    char form_ref[BAX_MAX_NAME_LEN];  /* form name for role binding (e.g. "main") */
    char lock_mode[BAX_MAX_NAME_LEN]; /* "" | "scale" | "root+fifth" | "chord" | "chord7" | "octaves" */
} bax_channel_t;

typedef struct {
    char part_name[BAX_MAX_NAME_LEN];   /* e.g. "mel", "harm", "bass", "perc" */
    char **patterns;
    size_t pattern_count;
    size_t pattern_capacity;
} bax_section_part_t;

typedef struct {
    char name[BAX_MAX_NAME_LEN];        /* e.g. "theme", "build", "turn", "reprise" */
    bax_section_part_t parts[BAX_MAX_CHANNELS];
    size_t part_count;
} bax_section_t;

typedef struct {
    char name[BAX_MAX_NAME_LEN];        /* e.g. "main" */
    char **section_names;
    size_t section_count;
    size_t section_capacity;
} bax_form_t;

typedef struct {
    /* Metadata */
    char name[BAX_MAX_STR_LEN];
    char artist[BAX_MAX_STR_LEN];
    char description[BAX_MAX_DESC_LEN];
    char tags[BAX_MAX_STR_LEN];

    /* Global settings */
    bax_chip_t chip;
    char chip_raw[BAX_MAX_NAME_LEN];
    int bpm;
    int steps_per_bar;                /* default 16 */
    char scale_root[16];
    char scale_mode[16];
    bool scale_warn;

    /* Instruments */
    bax_instrument_t *instruments;
    size_t instrument_count;
    size_t instrument_capacity;

    /* Effects */
    bax_effect_t *effects;
    size_t effect_count;
    size_t effect_capacity;

    /* Subpatterns */
    bax_subpat_t *subpatterns;
    size_t subpattern_count;
    size_t subpattern_capacity;

    /* Patterns */
    bax_pattern_t *patterns;
    size_t pattern_count;
    size_t pattern_capacity;

    /* Sequences */
    bax_sequence_t *sequences;
    size_t sequence_count;
    size_t sequence_capacity;

    /* Sections */
    bax_section_t *sections;
    size_t section_count;
    size_t section_capacity;

    /* Forms */
    bax_form_t *forms;
    size_t form_count;
    size_t form_capacity;

    /* Auto form routing */
    bool channel_auto_form;
    char auto_form_name[BAX_MAX_NAME_LEN];
    char auto_form_inst[BAX_MAX_STR_LEN];

    /* Channels (1..4) */
    bax_channel_t channels[BAX_MAX_CHANNELS];

    /* Transport */
    bool has_play;
    bool play_auto;
    bool play_repeat;
} bax_song_t;

/* Initialize an empty song structure */
void bax_song_init(bax_song_t *song);

/* Free all heap-allocated buffers in a song structure */
void bax_song_free(bax_song_t *song);

/* Helpers to add entities */
bax_instrument_t *bax_song_add_instrument(bax_song_t *song, const char *name, bax_voice_type_t type);
bax_effect_t *bax_song_add_effect(bax_song_t *song, const char *name, bax_effect_type_t type);
bax_subpat_t *bax_song_add_subpat(bax_song_t *song, const char *name);
bax_pattern_t *bax_song_add_pattern(bax_song_t *song, const char *name);
bax_sequence_t *bax_song_add_sequence(bax_song_t *song, const char *name);
bax_section_t *bax_song_add_section(bax_song_t *song, const char *name);
bax_form_t *bax_song_add_form(bax_song_t *song, const char *name);

/* Helpers to find entities */
const bax_instrument_t *bax_song_find_instrument(const bax_song_t *song, const char *name);
const bax_effect_t *bax_song_find_effect(const bax_song_t *song, const char *name);
const bax_subpat_t *bax_song_find_subpat(const bax_song_t *song, const char *name);
const bax_pattern_t *bax_song_find_pattern(const bax_song_t *song, const char *name);
const bax_sequence_t *bax_song_find_sequence(const bax_song_t *song, const char *name);
const bax_section_t *bax_song_find_section(const bax_song_t *song, const char *name);
const bax_form_t *bax_song_find_form(const bax_song_t *song, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* BAX_SONG_MODEL_H */
