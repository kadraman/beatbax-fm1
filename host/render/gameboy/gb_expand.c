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
 * Song expansion + event resolution (host). Matches the BeatBax PCM renderer
 * grid: one step = 1/4 beat = 1/16 note; note-on at the exact step boundary.
 * `tick_seconds = (60 / bpm) / 4` -> `start_sample = floor(step * 15 * sr / bpm)`.
 *
 * Both the offline renderer (gb_song_render.c) and the compiled-song embedder
 * (gb_embed.c) consume this, so their event schedules are identical.
 */

#include "gb_expand.h"

#include "gb_voice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    /* BeatBax rounds note starts to the nearest sample frame (pcmRenderer:
     * startSample = Math.round(time * sampleRate)). Use floor(x + 0.5) exactly so
     * odd BPM grids round identically to Math.round. */
    return (uint32_t)((step * 30ull * (uint64_t)c->sample_rate + (uint64_t)c->bpm) /
                      (2ull * (uint64_t)c->bpm));
}

static uint32_t dur_to_samples(const gb_expand_ctx_t *c, uint64_t base_step, uint64_t steps) {
    /* BeatBax: durationSamples = round((time+dur)*sr) - round(time*sr). */
    return step_to_sample(c, base_step + steps) - step_to_sample(c, base_step);
}

static bool list_push(gb_ev_list_t *list, const gb_ev_t *ev) {
    if (list->count >= list->cap) {
        size_t ncap = list->cap ? list->cap * 2 : 64;
        gb_ev_t *n = (gb_ev_t *)realloc(list->items, ncap * sizeof(gb_ev_t));
        if (!n) {
            return false;
        }
        list->items = n;
        list->cap = ncap;
    }
    list->items[list->count++] = *ev;
    return true;
}

/* Parse a simple note string ("C6", "C2", "D#4") into a MIDI number, or -1.
 * The signed semitone is added to the octave base directly so accidentals that
 * cross the octave boundary (B# -> next C, Cb -> previous B) resolve correctly. */
int gb_note_str_to_midi(const char *s) {
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
    return (octave + 1) * 12 + semi;
}

/*
 * Expand one pattern's tokens into events. `inst_name` is the channel's default
 * instrument. Tracks inline `inst` changes, temporary `inst(name,dur)`, inline
 * `vol:N`, and sustains the preceding note across hold (`-`) steps.
 * Returns false on allocation failure.
 */
static bool expand_pattern(gb_expand_ctx_t *ctx, gb_ev_list_t *list,
                           const bax_pattern_t *pat, const char *inst_name,
                           int oct_offset, int vol_override, const char *default_effect,
                           uint64_t *step) {
    char cur_inst[BAX_MAX_NAME_LEN];
    snprintf(cur_inst, sizeof(cur_inst), "%s", inst_name ? inst_name : "");
    char temp_inst[BAX_MAX_NAME_LEN];
    temp_inst[0] = '\0';
    int temp_remaining = 0;
    int cur_vol = -1; /* inline vol:N */

    bool last_note = false;
    uint64_t last_start_step = 0;
    uint64_t last_end_step = 0;

    for (size_t i = 0; i < pat->token_count; i++) {
        const bax_pat_token_t *t = &pat->tokens[i];
        switch (t->kind) {
            case BAX_TOKEN_REST:
                last_note = false;
                *step += 1;
                break;

            case BAX_TOKEN_HOLD:
                if (last_note && list->count > 0) {
                    last_end_step += 1;
                    list->items[list->count - 1].len_samples =
                        step_to_sample(ctx, last_end_step) - step_to_sample(ctx, last_start_step);
                }
                *step += 1;
                break;

            case BAX_TOKEN_INLINE_VOL:
                cur_vol = t->vol_level;
                *step += 1;
                break;

            case BAX_TOKEN_INLINE_INST: {
                if (strncmp(t->text, "inst ", 5) == 0) {
                    snprintf(cur_inst, sizeof(cur_inst), "%s", t->text + 5);
                    temp_remaining = 0;
                } else if (strncmp(t->text, "inst(", 5) == 0) {
                    char name[BAX_MAX_NAME_LEN];
                    const char *open = t->text + 5;
                    const char *close = strchr(open, ')');
                    size_t n = close ? (size_t)(close - open) : strlen(open);
                    if (n >= sizeof(name)) {
                        n = sizeof(name) - 1;
                    }
                    memcpy(name, open, n);
                    name[n] = '\0';
                    char *comma = strchr(name, ',');
                    int dur = 1;
                    if (comma) {
                        *comma = '\0';
                        dur = atoi(comma + 1);
                        if (dur < 1) {
                            dur = 1;
                        }
                    }
                    snprintf(temp_inst, sizeof(temp_inst), "%s", name);
                    temp_remaining = dur;
                } else {
                    /* Named instrument hit. */
                    gb_ev_t ev;
                    memset(&ev, 0, sizeof(ev));
                    ev.kind = GB_EV_NAMED;
                    ev.midi = -1;
                    snprintf(ev.inst, sizeof(ev.inst), "%s", t->text);
                    ev.start_sample = step_to_sample(ctx, *step);
                    ev.len_samples = dur_to_samples(ctx, *step, 1);
                    ev.vol_override = (cur_vol >= 0) ? cur_vol : vol_override;
                    if (!list_push(list, &ev)) {
                        return false;
                    }
                    last_note = true;
                    last_start_step = *step;
                    last_end_step = *step + 1;
                    *step += 1;
                }
                break;
            }

            case BAX_TOKEN_NOTE: {
                const char *inst = (temp_remaining > 0) ? temp_inst : cur_inst;
                if (temp_remaining > 0) {
                    temp_remaining--;
                }
                int midi = (t->octave + 1) * 12 + t->semitone + oct_offset * 12;
                int dur = t->duration > 0 ? t->duration : 1;
                gb_ev_t ev;
                memset(&ev, 0, sizeof(ev));
                ev.kind = GB_EV_NOTE;
                ev.midi = midi;
                snprintf(ev.inst, sizeof(ev.inst), "%s", inst);
                snprintf(ev.effect, sizeof(ev.effect), "%s",
                         t->effect_name[0] ? t->effect_name : (default_effect ? default_effect : ""));
                ev.start_sample = step_to_sample(ctx, *step);
                ev.len_samples = dur_to_samples(ctx, *step, (uint64_t)dur);
                ev.vol_override = (cur_vol >= 0) ? cur_vol : vol_override;
                if (!list_push(list, &ev)) {
                    return false;
                }
                last_note = true;
                last_start_step = *step;
                last_end_step = *step + (uint64_t)dur;
                *step += (uint64_t)dur;
                break;
            }
        }
    }
    return true;
}

static bool expand_seq_item(gb_expand_ctx_t *ctx, gb_ev_list_t *list,
                            const bax_seq_item_t *item, const char *default_inst,
                            int extra_oct, uint64_t *step) {
    const bax_pattern_t *pat = bax_song_find_pattern(ctx->song, item->pattern_name);
    if (!pat) {
        return true;
    }
    const char *inst = item->inst_override[0] ? item->inst_override : default_inst;
    return expand_pattern(ctx, list, pat, inst, item->oct_offset + extra_oct, item->vol_override,
                          item->effect, step);
}

/* Parse a channel ref like "seq", "seq*3", "seq:oct(-2)", "seq:oct(-2)*3" into a
 * base name, a repeat count, and an octave offset. */
static void parse_channel_ref(const char *ref, char *name, size_t name_sz, int *repeat, int *oct) {
    *repeat = 1;
    *oct = 0;
    const char *star = strchr(ref, '*');
    const char *end = star ? star : ref + strlen(ref);
    if (star) {
        *repeat = atoi(star + 1);
        if (*repeat < 1) *repeat = 1;
    }
    const char *c = ref;
    while (c < end && *c != ':') c++;
    size_t n = (size_t)(c - ref);
    if (n >= name_sz) n = name_sz - 1;
    memcpy(name, ref, n);
    name[n] = '\0';
    const char *p = c;
    while (p < end && *p == ':') {
        p++;
        if (strncmp(p, "oct(", 4) == 0) {
            *oct = atoi(p + 4);
        }
        while (p < end && *p != ':') p++;
    }
}

static bool expand_channel(gb_expand_ctx_t *ctx, gb_ev_list_t *list,
                           const bax_channel_t *ch) {
    uint64_t step = 0;
    for (size_t r = 0; r < ch->ref_count; r++) {
        const char *ref = ch->ref_names[r];
        char name[BAX_MAX_NAME_LEN];
        int repeat, oct;
        parse_channel_ref(ref, name, sizeof(name), &repeat, &oct);
        const bax_sequence_t *seq = bax_song_find_sequence(ctx->song, name);
        if (seq) {
            for (int rep = 0; rep < repeat; rep++) {
                for (size_t i = 0; i < seq->item_count; i++) {
                    if (!expand_seq_item(ctx, list, &seq->items[i], ch->default_inst, oct, &step)) {
                        return false;
                    }
                }
            }
        } else {
            const bax_pattern_t *pat = bax_song_find_pattern(ctx->song, name);
            if (pat) {
                for (int rep = 0; rep < repeat; rep++) {
                    if (!expand_pattern(ctx, list, pat, ch->default_inst, oct, -1, "", &step)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

/* Split "a|b|c|d" into four instrument names (Beginner Mode channel auto). */
static void split_auto_inst(const char *s, char insts[4][BAX_MAX_NAME_LEN]) {
    for (int i = 0; i < 4; i++) {
        insts[i][0] = '\0';
    }
    int idx = 0;
    const char *p = s;
    while (*p && idx < 4) {
        const char *pipe = strchr(p, '|');
        size_t n = pipe ? (size_t)(pipe - p) : strlen(p);
        if (n >= BAX_MAX_NAME_LEN) {
            n = BAX_MAX_NAME_LEN - 1;
        }
        memcpy(insts[idx], p, n);
        insts[idx][n] = '\0';
        idx++;
        if (!pipe) {
            break;
        }
        p = pipe + 1;
    }
}

static bool expand_channel_auto(gb_expand_ctx_t *ctx, gb_ev_list_t lists[BAX_MAX_CHANNELS],
                                const bax_song_t *song) {
    static const char *const roles[BAX_MAX_CHANNELS] = { "mel", "harm", "bass", "perc" };
    char insts[4][BAX_MAX_NAME_LEN];
    split_auto_inst(song->auto_form_inst, insts);

    const bax_form_t *form = bax_song_find_form(song, song->auto_form_name);
    if (!form) {
        return true;
    }

    uint64_t step[BAX_MAX_CHANNELS] = { 0, 0, 0, 0 };
    for (size_t si = 0; si < form->section_count; si++) {
        const bax_section_t *sec = bax_song_find_section(song, form->section_names[si]);
        if (!sec) {
            continue;
        }
        for (int r = 0; r < BAX_MAX_CHANNELS; r++) {
            for (size_t pi = 0; pi < sec->part_count; pi++) {
                const bax_section_part_t *part = &sec->parts[pi];
                if (strcmp(part->part_name, roles[r]) != 0) {
                    continue;
                }
                for (size_t pn = 0; pn < part->pattern_count; pn++) {
                    const bax_pattern_t *pat = bax_song_find_pattern(song, part->patterns[pn]);
                    if (pat) {
                        if (!expand_pattern(ctx, &lists[r], pat, insts[r], 0, -1, "", &step[r])) {
                            return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

static int compare_resolved(const void *a, const void *b) {
    const gb_resolved_event_t *ea = (const gb_resolved_event_t *)a;
    const gb_resolved_event_t *eb = (const gb_resolved_event_t *)b;
    if (ea->start != eb->start) {
        return ea->start < eb->start ? -1 : 1;
    }
    return ea->channel - eb->channel;
}

/* True if this event is a pulse note carrying a portamento effect (a candidate
 * for the shared pulse portamento voice, spec 112 / pulsePortamento.ts). */
static bool event_is_joinable_port(const gb_resolved_event_t *ev, const bax_song_t *song) {
    if (ev->fx_index < 0) {
        return false;
    }
    if (song->effects[ev->fx_index].type != BAX_EFFECT_PORTAMENTO) {
        return false;
    }
    bax_voice_type_t t = song->instruments[ev->inst_index].type;
    return t == BAX_VOICE_PULSE1 || t == BAX_VOICE_PULSE2;
}

/*
 * Tag `join`/`hold` on contiguous pulse `port` runs. The opening note holds its
 * voice (no release fade) for the next port note; later notes join that voice
 * (continue phase, flat envelope). Mirrors planPulsePortamentoLink. Note starts
 * are floor-rounded, so contiguity is judged within ~1e-4 s.
 */
static void tag_portamento_links(gb_resolved_list_t *list, const bax_song_t *song,
                                 uint32_t sample_rate) {
    uint32_t eps = sample_rate / 10000u + 1u; /* ~1e-4 s (PORTAMENTO_CONTIGUOUS_EPSILON_SEC) */
    uint32_t hold_until[BAX_MAX_CHANNELS];
    for (int c = 0; c < BAX_MAX_CHANNELS; c++) {
        hold_until[c] = UINT32_MAX;
    }

    for (size_t i = 0; i < list->count; i++) {
        gb_resolved_event_t *ev = &list->items[i];
        ev->join = false;
        ev->hold = false;
        int c = ev->channel;
        if (!event_is_joinable_port(ev, song)) {
            hold_until[c] = UINT32_MAX;
            continue;
        }
        /* Walk the contiguous chain of joinable port notes on this channel. */
        uint32_t note_end = ev->start + ev->len;
        uint32_t expected = note_end;
        uint32_t chain_end = note_end;
        for (size_t j = i + 1; j < list->count; j++) {
            const gb_resolved_event_t *next = &list->items[j];
            if (next->channel != c) {
                continue;
            }
            uint32_t d = next->start > expected ? next->start - expected : expected - next->start;
            if (d > eps) {
                break;
            }
            if (!event_is_joinable_port(next, song)) {
                break;
            }
            expected = next->start + next->len;
            chain_end = expected;
        }
        bool holds = chain_end > note_end;
        ev->hold = holds;
        if (hold_until[c] != UINT32_MAX) {
            uint32_t d = hold_until[c] > ev->start ? hold_until[c] - ev->start : ev->start - hold_until[c];
            ev->join = d <= eps;
        }
        hold_until[c] = holds ? note_end : UINT32_MAX;
    }
}

bool gb_song_resolve(const bax_song_t *song, uint32_t sample_rate,
                     gb_resolved_list_t *list, uint32_t *total,
                     char *errbuf, size_t errbuf_len) {
    memset(list, 0, sizeof(*list));
    *total = 0;
    if (!song || sample_rate == 0) {
        if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "null argument");
        return false;
    }
    if (song->chip != BAX_CHIP_GAMEBOY) {
        if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "unsupported chip (renderer is Game Boy only)");
        return false;
    }

    uint32_t bpm = song->bpm > 0 ? (uint32_t)song->bpm : 128;
    gb_expand_ctx_t ctx;
    ctx.song = song;
    ctx.sample_rate = sample_rate;
    ctx.bpm = bpm;

    gb_ev_list_t lists[BAX_MAX_CHANNELS];
    memset(lists, 0, sizeof(lists));

    bool ok = true;
    if (song->channel_auto_form) {
        ok = expand_channel_auto(&ctx, lists, song);
    } else {
        for (int c = 0; c < BAX_MAX_CHANNELS && ok; c++) {
            const bax_channel_t *ch = &song->channels[c];
            if (ch->ref_count == 0) {
                continue;
            }
            ok = expand_channel(&ctx, &lists[c], ch);
        }
    }
    if (!ok) {
        for (int c = 0; c < BAX_MAX_CHANNELS; c++) free(lists[c].items);
        if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "out of memory");
        return false;
    }

    uint64_t max_step = 0;
    for (int c = 0; c < BAX_MAX_CHANNELS; c++) {
        for (size_t i = 0; i < lists[c].count; i++) {
            uint64_t end_sample = (uint64_t)lists[c].items[i].start_sample +
                                  lists[c].items[i].len_samples;
            if (end_sample > max_step) {
                max_step = end_sample;
            }
        }
    }
    if (max_step == 0) {
        for (int c = 0; c < BAX_MAX_CHANNELS; c++) free(lists[c].items);
        if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "song has no playable events");
        return false;
    }

    uint32_t tail_margin = gb_ms_to_samples(GB_WAVE_TAIL_MS, sample_rate);
    uint32_t total_frames = (uint32_t)max_step + step_to_sample(&ctx, 1) + tail_margin;

    /* Resolve each raw event into a renderable event. */
    for (int c = 0; c < BAX_MAX_CHANNELS; c++) {
        for (size_t i = 0; i < lists[c].count; i++) {
            const gb_ev_t *ev = &lists[c].items[i];
            const bax_instrument_t *inst = bax_song_find_instrument(song, ev->inst);
            if (!inst) {
                continue;
            }
            int midi = ev->midi;
            if (ev->kind == GB_EV_NAMED) {
                midi = gb_note_str_to_midi(inst->note);
                if (midi < 0 && inst->type != BAX_VOICE_NOISE) {
                    continue; /* melodic named hit with no default note */
                }
                if (midi < 0) {
                    midi = 0; /* noise ignores pitch; uge_note drives its clock */
                }
            }

            uint32_t len = ev->len_samples;
            if (ev->start_sample + len > total_frames) {
                len = total_frames - ev->start_sample;
            }
            if (len == 0) {
                continue;
            }
            if (inst->type == BAX_VOICE_NOISE && inst->length > 0) {
                int l = inst->length > 63 ? 63 : inst->length;
                uint32_t hw = (uint32_t)((uint64_t)(64 - l) * sample_rate / 256u);
                if (hw > len) {
                    len = hw;
                }
                if (ev->start_sample + len > total_frames) {
                    len = total_frames - ev->start_sample;
                }
                if (len == 0) {
                    continue;
                }
            }

            gb_resolved_event_t r;
            r.start = ev->start_sample;
            r.len = len;
            r.midi = midi;
            r.inst_index = (int)(inst - song->instruments);
            r.fx_index = -1;
            if (ev->effect[0]) {
                const bax_effect_t *fx = bax_song_find_effect(song, ev->effect);
                if (fx) {
                    if (fx->type == BAX_EFFECT_OTHER) {
                        /* Out-of-scope effect token (trem / volSlide / cut / retrig /
                         * echo / panning …). Refuse with a named reason rather than
                         * silently dropping it (constitution IV). The parser keeps
                         * BAX_EFFECT_OTHER for round-trip fidelity; only rendering
                         * refuses. */
                        char tok[24];
                        const char *colon = strchr(fx->raw_expr, ':');
                        size_t tl = colon ? (size_t)(colon - fx->raw_expr) : strlen(fx->raw_expr);
                        if (tl >= sizeof(tok)) tl = sizeof(tok) - 1;
                        memcpy(tok, fx->raw_expr, tl);
                        tok[tl] = '\0';
                        if (errbuf && errbuf_len) {
                            snprintf(errbuf, errbuf_len, "unsupported effect: %s", tok);
                        }
                        for (int cc = 0; cc < BAX_MAX_CHANNELS; cc++) free(lists[cc].items);
                        gb_resolved_list_free(list);
                        return false;
                    }
                    r.fx_index = (int)(fx - song->effects);
                }
            }
            r.vol_override = ev->vol_override;
            r.channel = c;

            if (list->count >= list->cap) {
                size_t ncap = list->cap ? list->cap * 2 : 64;
                gb_resolved_event_t *n = (gb_resolved_event_t *)realloc(list->items, ncap * sizeof(gb_resolved_event_t));
                if (!n) {
                    for (int cc = 0; cc < BAX_MAX_CHANNELS; cc++) free(lists[cc].items);
                    gb_resolved_list_free(list);
                    if (errbuf && errbuf_len) snprintf(errbuf, errbuf_len, "out of memory");
                    return false;
                }
                list->items = n;
                list->cap = ncap;
            }
            list->items[list->count++] = r;
        }
    }

    for (int c = 0; c < BAX_MAX_CHANNELS; c++) free(lists[c].items);

    qsort(list->items, list->count, sizeof(gb_resolved_event_t), compare_resolved);

    tag_portamento_links(list, song, sample_rate);

    *total = total_frames;
    return true;
}

void gb_resolved_list_free(gb_resolved_list_t *list) {
    if (!list) {
        return;
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}
