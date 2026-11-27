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

#include "song_model.h"
#include <stdlib.h>
#include <string.h>

void bax_song_init(bax_song_t *song) {
    if (!song) return;
    memset(song, 0, sizeof(bax_song_t));
    song->chip = BAX_CHIP_GAMEBOY;
    strncpy(song->chip_raw, "gameboy", sizeof(song->chip_raw) - 1);
    song->bpm = 120;
    song->steps_per_bar = 16;

    for (int i = 0; i < BAX_MAX_CHANNELS; i++) {
        song->channels[i].channel_num = i + 1;
    }
}

void bax_song_free(bax_song_t *song) {
    if (!song) return;

    if (song->instruments) {
        free(song->instruments);
        song->instruments = NULL;
    }
    song->instrument_count = 0;
    song->instrument_capacity = 0;

    if (song->effects) {
        free(song->effects);
        song->effects = NULL;
    }
    song->effect_count = 0;
    song->effect_capacity = 0;

    if (song->subpatterns) {
        for (size_t i = 0; i < song->subpattern_count; i++) {
            if (song->subpatterns[i].steps) {
                free(song->subpatterns[i].steps);
            }
        }
        free(song->subpatterns);
        song->subpatterns = NULL;
    }
    song->subpattern_count = 0;
    song->subpattern_capacity = 0;

    if (song->patterns) {
        for (size_t i = 0; i < song->pattern_count; i++) {
            if (song->patterns[i].tokens) {
                free(song->patterns[i].tokens);
            }
            if (song->patterns[i].rhs) {
                free(song->patterns[i].rhs);
            }
        }
        free(song->patterns);
        song->patterns = NULL;
    }
    song->pattern_count = 0;
    song->pattern_capacity = 0;

    if (song->sequences) {
        for (size_t i = 0; i < song->sequence_count; i++) {
            if (song->sequences[i].items) {
                free(song->sequences[i].items);
            }
        }
        free(song->sequences);
        song->sequences = NULL;
    }
    song->sequence_count = 0;
    song->sequence_capacity = 0;

    if (song->sections) {
        for (size_t i = 0; i < song->section_count; i++) {
            for (size_t p = 0; p < song->sections[i].part_count; p++) {
                if (song->sections[i].parts[p].patterns) {
                    for (size_t k = 0; k < song->sections[i].parts[p].pattern_count; k++) {
                        free(song->sections[i].parts[p].patterns[k]);
                    }
                    free(song->sections[i].parts[p].patterns);
                }
            }
        }
        free(song->sections);
        song->sections = NULL;
    }
    song->section_count = 0;
    song->section_capacity = 0;

    if (song->forms) {
        for (size_t i = 0; i < song->form_count; i++) {
            if (song->forms[i].section_names) {
                for (size_t k = 0; k < song->forms[i].section_count; k++) {
                    free(song->forms[i].section_names[k]);
                }
                free(song->forms[i].section_names);
            }
        }
        free(song->forms);
        song->forms = NULL;
    }
    song->form_count = 0;
    song->form_capacity = 0;

    for (int i = 0; i < BAX_MAX_CHANNELS; i++) {
        if (song->channels[i].ref_names) {
            for (size_t j = 0; j < song->channels[i].ref_count; j++) {
                if (song->channels[i].ref_names[j]) {
                    free(song->channels[i].ref_names[j]);
                }
            }
            free(song->channels[i].ref_names);
            song->channels[i].ref_names = NULL;
        }
        song->channels[i].ref_count = 0;
        song->channels[i].ref_capacity = 0;
    }
}

bax_instrument_t *bax_song_add_instrument(bax_song_t *song, const char *name, bax_voice_type_t type) {
    if (!song || !name) return NULL;
    if (strlen(name) >= BAX_MAX_NAME_LEN) return NULL;

    if (song->instrument_count >= song->instrument_capacity) {
        size_t new_cap = song->instrument_capacity == 0 ? 8 : song->instrument_capacity * 2;
        bax_instrument_t *new_arr = (bax_instrument_t *)realloc(song->instruments, new_cap * sizeof(bax_instrument_t));
        if (!new_arr) return NULL;
        song->instruments = new_arr;
        song->instrument_capacity = new_cap;
    }

    bax_instrument_t *inst = &song->instruments[song->instrument_count++];
    memset(inst, 0, sizeof(bax_instrument_t));
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->type = type;
    inst->length = -1;
    inst->vol = -1;
    inst->gm = -1;
    inst->wave_volume = 100; /* BeatBax defaults wave volume to 100% (NR32 full) */
    inst->vol_env.loop_point = -1;
    inst->pitch_env.loop_point = -1;
    inst->duty_env.loop_point = -1;
    inst->arp_env.loop_point = -1;
    return inst;
}

bax_effect_t *bax_song_add_effect(bax_song_t *song, const char *name, bax_effect_type_t type) {
    if (!song || !name) return NULL;
    if (strlen(name) >= BAX_MAX_NAME_LEN) return NULL;

    if (song->effect_count >= song->effect_capacity) {
        size_t new_cap = song->effect_capacity == 0 ? 8 : song->effect_capacity * 2;
        bax_effect_t *new_arr = (bax_effect_t *)realloc(song->effects, new_cap * sizeof(bax_effect_t));
        if (!new_arr) return NULL;
        song->effects = new_arr;
        song->effect_capacity = new_cap;
    }

    bax_effect_t *eff = &song->effects[song->effect_count++];
    memset(eff, 0, sizeof(bax_effect_t));
    strncpy(eff->name, name, sizeof(eff->name) - 1);
    eff->type = type;
    return eff;
}

bax_subpat_t *bax_song_add_subpat(bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    if (strlen(name) >= BAX_MAX_NAME_LEN) return NULL;

    if (song->subpattern_count >= song->subpattern_capacity) {
        size_t new_cap = song->subpattern_capacity == 0 ? 8 : song->subpattern_capacity * 2;
        bax_subpat_t *new_arr = (bax_subpat_t *)realloc(song->subpatterns, new_cap * sizeof(bax_subpat_t));
        if (!new_arr) return NULL;
        song->subpatterns = new_arr;
        song->subpattern_capacity = new_cap;
    }

    bax_subpat_t *sub = &song->subpatterns[song->subpattern_count++];
    memset(sub, 0, sizeof(bax_subpat_t));
    strncpy(sub->name, name, sizeof(sub->name) - 1);
    return sub;
}

bax_pattern_t *bax_song_add_pattern(bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    if (strlen(name) >= BAX_MAX_NAME_LEN) return NULL;

    if (song->pattern_count >= song->pattern_capacity) {
        size_t new_cap = song->pattern_capacity == 0 ? 16 : song->pattern_capacity * 2;
        bax_pattern_t *new_arr = (bax_pattern_t *)realloc(song->patterns, new_cap * sizeof(bax_pattern_t));
        if (!new_arr) return NULL;
        song->patterns = new_arr;
        song->pattern_capacity = new_cap;
    }

    bax_pattern_t *pat = &song->patterns[song->pattern_count++];
    memset(pat, 0, sizeof(bax_pattern_t));
    strncpy(pat->name, name, sizeof(pat->name) - 1);
    return pat;
}

bax_sequence_t *bax_song_add_sequence(bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    if (strlen(name) >= BAX_MAX_NAME_LEN) return NULL;

    if (song->sequence_count >= song->sequence_capacity) {
        size_t new_cap = song->sequence_capacity == 0 ? 8 : song->sequence_capacity * 2;
        bax_sequence_t *new_arr = (bax_sequence_t *)realloc(song->sequences, new_cap * sizeof(bax_sequence_t));
        if (!new_arr) return NULL;
        song->sequences = new_arr;
        song->sequence_capacity = new_cap;
    }

    bax_sequence_t *seq = &song->sequences[song->sequence_count++];
    memset(seq, 0, sizeof(bax_sequence_t));
    strncpy(seq->name, name, sizeof(seq->name) - 1);
    return seq;
}

const bax_instrument_t *bax_song_find_instrument(const bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    for (size_t i = 0; i < song->instrument_count; i++) {
        if (strcmp(song->instruments[i].name, name) == 0) {
            return &song->instruments[i];
        }
    }
    return NULL;
}

const bax_effect_t *bax_song_find_effect(const bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    for (size_t i = 0; i < song->effect_count; i++) {
        if (strcmp(song->effects[i].name, name) == 0) {
            return &song->effects[i];
        }
    }
    return NULL;
}

const bax_subpat_t *bax_song_find_subpat(const bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    for (size_t i = 0; i < song->subpattern_count; i++) {
        if (strcmp(song->subpatterns[i].name, name) == 0) {
            return &song->subpatterns[i];
        }
    }
    return NULL;
}

const bax_pattern_t *bax_song_find_pattern(const bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    for (size_t i = 0; i < song->pattern_count; i++) {
        if (strcmp(song->patterns[i].name, name) == 0) {
            return &song->patterns[i];
        }
    }
    return NULL;
}

const bax_sequence_t *bax_song_find_sequence(const bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    for (size_t i = 0; i < song->sequence_count; i++) {
        if (strcmp(song->sequences[i].name, name) == 0) {
            return &song->sequences[i];
        }
    }
    return NULL;
}

bax_section_t *bax_song_add_section(bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    if (strlen(name) >= BAX_MAX_NAME_LEN) return NULL;

    if (song->section_count >= song->section_capacity) {
        size_t new_cap = song->section_capacity == 0 ? 8 : song->section_capacity * 2;
        bax_section_t *new_arr = (bax_section_t *)realloc(song->sections, new_cap * sizeof(bax_section_t));
        if (!new_arr) return NULL;
        song->sections = new_arr;
        song->section_capacity = new_cap;
    }

    bax_section_t *sec = &song->sections[song->section_count++];
    memset(sec, 0, sizeof(bax_section_t));
    strncpy(sec->name, name, sizeof(sec->name) - 1);
    return sec;
}

bax_form_t *bax_song_add_form(bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    if (strlen(name) >= BAX_MAX_NAME_LEN) return NULL;

    if (song->form_count >= song->form_capacity) {
        size_t new_cap = song->form_capacity == 0 ? 4 : song->form_capacity * 2;
        bax_form_t *new_arr = (bax_form_t *)realloc(song->forms, new_cap * sizeof(bax_form_t));
        if (!new_arr) return NULL;
        song->forms = new_arr;
        song->form_capacity = new_cap;
    }

    bax_form_t *frm = &song->forms[song->form_count++];
    memset(frm, 0, sizeof(bax_form_t));
    strncpy(frm->name, name, sizeof(frm->name) - 1);
    return frm;
}

const bax_section_t *bax_song_find_section(const bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    for (size_t i = 0; i < song->section_count; i++) {
        if (strcmp(song->sections[i].name, name) == 0) {
            return &song->sections[i];
        }
    }
    return NULL;
}

const bax_form_t *bax_song_find_form(const bax_song_t *song, const char *name) {
    if (!song || !name) return NULL;
    for (size_t i = 0; i < song->form_count; i++) {
        if (strcmp(song->forms[i].name, name) == 0) {
            return &song->forms[i];
        }
    }
    return NULL;
}
