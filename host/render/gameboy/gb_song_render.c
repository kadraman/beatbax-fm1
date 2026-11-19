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

#include "gb_note_render.h"
#include "gb_song_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Scheduler/expansion for feature 003 (host). Matches the BeatBax PCM renderer
 * grid: one step = 1/4 beat = 1/16 note; note-on at the exact step boundary.
 * `tick_seconds = (60 / bpm) / 4` -> `start_sample = floor(step * 15 * sr / bpm)`.
 */

typedef enum {
    GB_EV_NOTE = 0,
    GB_EV_NAMED = 1
} gb_ev_kind_t;

typedef struct {
    gb_ev_kind_t kind;
    uint32_t start_sample;
    uint32_t len_samples;
    int midi; /* NOTE: resolved MIDI; NAMED: -1 (resolved at render) */
    char inst[BAX_MAX_NAME_LEN];
    char effect[BAX_MAX_NAME_LEN];
    int vol_override; /* -1 = none */
} gb_ev_t;

typedef struct {
    gb_ev_t *items;
    size_t count;
    size_t cap;
} gb_ev_list_t;

typedef struct {
    const bax_song_t *song;
    uint32_t sample_rate;
    uint32_t bpm;
} gb_expand_ctx_t;

static uint32_t step_to_sample(const gb_expand_ctx_t *c, uint64_t step) {
    return (uint32_t)((step * 15ull * (uint64_t)c->sample_rate) / (uint64_t)c->bpm);
}

static void list_push(gb_ev_list_t *list, const gb_ev_t *ev) {
    if (list->count >= list->cap) {
        size_t ncap = list->cap ? list->cap * 2 : 64;
        gb_ev_t *n = (gb_ev_t *)realloc(list->items, ncap * sizeof(gb_ev_t));
        if (!n) {
            return;
        }
        list->items = n;
        list->cap = ncap;
    }
    list->items[list->count++] = *ev;
}

/* Parse a simple note string ("C6", "C2", "D#4") into a MIDI number, or -1. */
static int note_str_to_midi(const char *s) {
    if (!s || !s[0]) {
        return -1;
    }
    char c = s[0];
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    int semi;
    switch (c) {
        case 'C': semi = 0; break;
        case 'D': semi = 2; break;
        case 'E': semi = 4; break;
        case 'F': semi = 5; break;
        case 'G': semi = 7; break;
        case 'A': semi = 9; break;
        case 'B': semi = 11; break;
        default: return -1;
    }
    const char *p = s + 1;
    if (*p == '#') { semi++; p++; }
    else if (*p == 'b') { semi--; p++; }
    else if (*p == '-') { p++; }
    if (*p < '0' || *p > '9') {
        return -1;
    }
    int octave = *p - '0';
    return (octave + 1) * 12 + ((semi % 12) + 12) % 12;
}

/*
 * Expand one pattern's tokens into events. `cur_inst` is a mutable scratch
 * holding the active instrument name (inline `inst X` changes it).
 */
static void expand_pattern(gb_expand_ctx_t *ctx, gb_ev_list_t *list,
                           const bax_pattern_t *pat, const char *inst_name,
                           int oct_offset, int vol_override, uint64_t *step) {
    char cur_inst[BAX_MAX_NAME_LEN];
    snprintf(cur_inst, sizeof(cur_inst), "%s", inst_name ? inst_name : "");

    for (size_t i = 0; i < pat->token_count; i++) {
        const bax_pat_token_t *t = &pat->tokens[i];
        switch (t->kind) {
            case BAX_TOKEN_REST:
            case BAX_TOKEN_HOLD:
            case BAX_TOKEN_INLINE_VOL:
                *step += 1;
                break;

            case BAX_TOKEN_INLINE_INST: {
                if (strncmp(t->text, "inst ", 5) == 0) {
                    snprintf(cur_inst, sizeof(cur_inst), "%s", t->text + 5);
                } else if (strncmp(t->text, "inst(", 5) == 0) {
                    /* Temporary instrument inst(name[,dur]). Use the name; the
                     * temporary-duration counter is a later increment. */
                    const char *open = t->text + 5;
                    const char *close = strchr(open, ')');
                    size_t n = close ? (size_t)(close - open) : strlen(open);
                    if (n >= sizeof(cur_inst)) {
                        n = sizeof(cur_inst) - 1;
                    }
                    memcpy(cur_inst, open, n);
                    cur_inst[n] = '\0';
                    char *comma = strchr(cur_inst, ',');
                    if (comma) {
                        *comma = '\0';
                    }
                } else {
                    /* Named instrument hit. */
                    gb_ev_t ev;
                    memset(&ev, 0, sizeof(ev));
                    ev.kind = GB_EV_NAMED;
                    ev.midi = -1;
                    snprintf(ev.inst, sizeof(ev.inst), "%s", t->text);
                    ev.start_sample = step_to_sample(ctx, *step);
                    ev.len_samples = step_to_sample(ctx, 1);
                    ev.vol_override = vol_override;
                    list_push(list, &ev);
                    *step += 1;
                }
                break;
            }

            case BAX_TOKEN_NOTE: {
                int midi = (t->octave + 1) * 12 + t->semitone + oct_offset * 12;
                int dur = t->duration > 0 ? t->duration : 1;
                gb_ev_t ev;
                memset(&ev, 0, sizeof(ev));
                ev.kind = GB_EV_NOTE;
                ev.midi = midi;
                snprintf(ev.inst, sizeof(ev.inst), "%s", cur_inst);
                snprintf(ev.effect, sizeof(ev.effect), "%s", t->effect_name);
                ev.start_sample = step_to_sample(ctx, *step);
                ev.len_samples = step_to_sample(ctx, *step + (uint64_t)dur) - ev.start_sample;
                ev.vol_override = vol_override;
                list_push(list, &ev);
                *step += (uint64_t)dur;
                break;
            }
        }
    }
}

/* Append a pattern reference's events to `list`, using `default_inst` when the
 * sequence item has no `:inst(...)` override. */
static void expand_seq_item(gb_expand_ctx_t *ctx, gb_ev_list_t *list,
                            const bax_seq_item_t *item, const char *default_inst,
                            uint64_t *step) {
    const bax_pattern_t *pat = bax_song_find_pattern(ctx->song, item->pattern_name);
    if (!pat) {
        return;
    }
    const char *inst = item->inst_override[0] ? item->inst_override : default_inst;
    expand_pattern(ctx, list, pat, inst, item->oct_offset, item->vol_override, step);
}

/* Expand a channel's ref list (sequence names or pattern names) into events. */
static void expand_channel(gb_expand_ctx_t *ctx, gb_ev_list_t *list,
                           const bax_channel_t *ch) {
    uint64_t step = 0;
    for (size_t r = 0; r < ch->ref_count; r++) {
        const char *ref = ch->ref_names[r];
        const bax_sequence_t *seq = bax_song_find_sequence(ctx->song, ref);
        if (seq) {
            for (size_t i = 0; i < seq->item_count; i++) {
                expand_seq_item(ctx, list, &seq->items[i], ch->default_inst, &step);
            }
        } else {
            const bax_pattern_t *pat = bax_song_find_pattern(ctx->song, ref);
            if (pat) {
                expand_pattern(ctx, list, pat, ch->default_inst, 0, -1, &step);
            }
        }
    }
}

bool gb_song_render_mono(const bax_song_t *song, uint32_t sample_rate,
                         gb_sample_t **out_mono, size_t *out_len,
                         char *errbuf, size_t errbuf_len) {
    if (!song || !out_mono || !out_len) {
        if (errbuf && errbuf_len) {
            snprintf(errbuf, errbuf_len, "null argument");
        }
        return false;
    }
    if (song->chip != BAX_CHIP_GAMEBOY) {
        if (errbuf && errbuf_len) {
            snprintf(errbuf, errbuf_len, "unsupported chip (renderer is Game Boy only)");
        }
        return false;
    }

    uint32_t bpm = song->bpm > 0 ? (uint32_t)song->bpm : 128;
    gb_expand_ctx_t ctx;
    ctx.song = song;
    ctx.sample_rate = sample_rate;
    ctx.bpm = bpm;

    gb_ev_list_t lists[BAX_MAX_CHANNELS];
    memset(lists, 0, sizeof(lists));

    uint64_t max_step = 0;
    for (int c = 0; c < BAX_MAX_CHANNELS; c++) {
        const bax_channel_t *ch = &song->channels[c];
        if (ch->ref_count == 0) {
            continue;
        }
        expand_channel(&ctx, &lists[c], ch);
        for (size_t i = 0; i < lists[c].count; i++) {
            uint64_t end_sample = (uint64_t)lists[c].items[i].start_sample +
                                  lists[c].items[i].len_samples;
            if (end_sample > max_step) {
                max_step = end_sample;
            }
        }
    }

    if (max_step == 0) {
        for (int c = 0; c < BAX_MAX_CHANNELS; c++) {
            free(lists[c].items);
        }
        if (errbuf && errbuf_len) {
            snprintf(errbuf, errbuf_len, "song has no playable events");
        }
        return false;
    }

    size_t total = (size_t)max_step + step_to_sample(&ctx, 1);
    int32_t *mix = (int32_t *)calloc(total, sizeof(int32_t));
    if (!mix) {
        for (int c = 0; c < BAX_MAX_CHANNELS; c++) {
            free(lists[c].items);
        }
        if (errbuf && errbuf_len) {
            snprintf(errbuf, errbuf_len, "out of memory");
        }
        return false;
    }

    uint32_t max_len = 0;
    for (int c = 0; c < BAX_MAX_CHANNELS; c++) {
        for (size_t i = 0; i < lists[c].count; i++) {
            uint32_t l = lists[c].items[i].len_samples;
            const bax_instrument_t *ni = bax_song_find_instrument(song, lists[c].items[i].inst);
            if (ni && ni->type == BAX_VOICE_NOISE && ni->length > 0) {
                int ll = ni->length > 63 ? 63 : ni->length;
                uint32_t hw = (uint32_t)((uint64_t)(64 - ll) * sample_rate / 256u);
                if (hw > l) {
                    l = hw;
                }
            }
            if (l > max_len) {
                max_len = l;
            }
        }
    }
    gb_sample_t *scratch = (gb_sample_t *)malloc((size_t)max_len * sizeof(gb_sample_t));
    if (max_len > 0 && !scratch) {
        free(mix);
        for (int c = 0; c < BAX_MAX_CHANNELS; c++) {
            free(lists[c].items);
        }
        if (errbuf && errbuf_len) {
            snprintf(errbuf, errbuf_len, "out of memory");
        }
        return false;
    }

    gb_channel_state_t state;
    gb_channel_state_init(&state);

    for (int c = 0; c < BAX_MAX_CHANNELS; c++) {
        for (size_t i = 0; i < lists[c].count; i++) {
            const gb_ev_t *ev = &lists[c].items[i];
            const bax_instrument_t *inst = bax_song_find_instrument(song, ev->inst);
            if (!inst) {
                continue;
            }
            uint32_t len = ev->len_samples;
            if (ev->start_sample + len > total) {
                len = (uint32_t)(total - ev->start_sample);
            }
            if (len == 0) {
                continue;
            }

            /* Noise hardware length extends the render window (NR41). */
            if (inst->type == BAX_VOICE_NOISE && inst->length > 0) {
                int l = inst->length > 63 ? 63 : inst->length;
                uint32_t hw = (uint32_t)((uint64_t)(64 - l) * sample_rate / 256u);
                if (hw > len) {
                    len = hw;
                    if (ev->start_sample + len > total) {
                        len = (uint32_t)(total - ev->start_sample);
                    }
                }
            }

            int midi = ev->midi;
            if (ev->kind == GB_EV_NAMED) {
                midi = note_str_to_midi(inst->note);
                if (midi < 0 && inst->type != BAX_VOICE_NOISE) {
                    /* Melodic named hit with no default note -> nothing to play. */
                    continue;
                }
                if (midi < 0) {
                    midi = 0; /* noise ignores pitch; uge_note drives its clock */
                }
            }

            gb_render_note_full(scratch, len, song, midi, inst, ev->effect,
                                sample_rate, &state, c);

            for (uint32_t s = 0; s < len; s++) {
                int32_t v = mix[ev->start_sample + s] + scratch[s];
                if (v > 32767) {
                    v = 32767;
                } else if (v < -32768) {
                    v = -32768;
                }
                mix[ev->start_sample + s] = v;
            }
        }
    }

    free(scratch);
    for (int c = 0; c < BAX_MAX_CHANNELS; c++) {
        free(lists[c].items);
    }

    gb_sample_t *out = (gb_sample_t *)malloc(total * sizeof(gb_sample_t));
    if (!out) {
        free(mix);
        if (errbuf && errbuf_len) {
            snprintf(errbuf, errbuf_len, "out of memory");
        }
        return false;
    }
    for (size_t s = 0; s < total; s++) {
        out[s] = (gb_sample_t)mix[s];
    }
    free(mix);

    /* Peak-limit to 0.95 (PLAYBACK_PEAK_CEILING), matching BeatBax's
     * normalizeBuffer(buffer, false): scale down only if clipping. */
    int peak = 0;
    for (size_t s = 0; s < total; s++) {
        int a = out[s] < 0 ? -out[s] : out[s];
        if (a > peak) {
            peak = a;
        }
    }
    if (peak > 31130) {
        int32_t scale = (int32_t)(((uint64_t)31130u << 15) / (uint64_t)peak);
        for (size_t s = 0; s < total; s++) {
            out[s] = (gb_sample_t)(((int32_t)out[s] * scale) >> 15);
        }
    }

    *out_mono = out;
    *out_len = total;
    return true;
}
