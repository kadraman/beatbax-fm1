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

#include "song_write.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *buf;
    size_t capacity;
    size_t length;
    bool overflow;
} write_ctx_t;

static void w_printf(write_ctx_t *ctx, const char *fmt, ...) {
    if (ctx->overflow) return;
    va_list args;
    va_start(args, fmt);
    int written = 0;
    if (ctx->buf && ctx->length < ctx->capacity) {
        written = vsnprintf(ctx->buf + ctx->length, ctx->capacity - ctx->length, fmt, args);
    } else {
        /* Dry run to calculate size */
        char dummy[1];
        written = vsnprintf(dummy, sizeof(dummy), fmt, args);
    }
    va_end(args);

    if (written < 0) {
        ctx->overflow = true;
        return;
    }

    ctx->length += (size_t)written;
    if (ctx->buf && ctx->length >= ctx->capacity) {
        ctx->overflow = true;
    }
}

static const char *voice_type_str(bax_voice_type_t type) {
    switch (type) {
        case BAX_VOICE_PULSE1: return "pulse1";
        case BAX_VOICE_PULSE2: return "pulse2";
        case BAX_VOICE_WAVE: return "wave";
        case BAX_VOICE_NOISE: return "noise";
        default: return "unknown";
    }
}

static const char *env_direction_str(bax_env_direction_t dir) {
    switch (dir) {
        case BAX_ENV_DIR_UP: return "up";
        case BAX_ENV_DIR_FLAT: return "flat";
        default: return "down";
    }
}

static void write_macro_array(write_ctx_t *ctx, const char *key, const bax_macro_env_t *m) {
    if (m->count == 0) return;
    w_printf(ctx, " %s=[", key);
    for (size_t i = 0; i < m->count; i++) {
        if (i > 0) w_printf(ctx, ",");
        w_printf(ctx, "%d", m->values[i]);
    }
    if (m->loop_point >= 0) {
        w_printf(ctx, "|%d", m->loop_point);
    }
    w_printf(ctx, "]");
}

static void write_wave_array(write_ctx_t *ctx, const bax_gb_wave_t *w) {
    w_printf(ctx, " wave=[");
    for (size_t i = 0; i < w->count; i++) {
        if (i > 0) w_printf(ctx, ",");
        w_printf(ctx, "%d", w->samples[i]);
    }
    w_printf(ctx, "]");
}

static void write_instrument(write_ctx_t *ctx, const bax_instrument_t *inst) {
    w_printf(ctx, "inst %s type=%s", inst->name, voice_type_str(inst->type));

    if (inst->type == BAX_VOICE_PULSE1 || inst->type == BAX_VOICE_PULSE2) {
        if (inst->duty > 0.0f) {
            if (inst->duty == (float)((int)inst->duty)) {
                w_printf(ctx, " duty=%d", (int)inst->duty);
            } else {
                w_printf(ctx, " duty=%.1f", inst->duty);
            }
        }
        if (inst->env.present) {
            const char *dir = env_direction_str(inst->env.direction);
            if (inst->has_env_object_syntax) {
                w_printf(ctx, " env={\"level\":%d,\"direction\":\"%s\",\"period\":%d}",
                         inst->env.level, dir, inst->env.period);
            } else {
                w_printf(ctx, " env=gb:%d,%s,%d", inst->env.level, dir, inst->env.period);
            }
        }
        if (inst->sweep.present) {
            const char *dir = (inst->sweep.direction == BAX_SWEEP_DIR_DOWN) ? "down" : "up";
            w_printf(ctx, " sweep={\"time\":%d,\"direction\":\"%s\",\"shift\":%d}",
                     inst->sweep.time, dir, inst->sweep.shift);
        }
    } else if (inst->type == BAX_VOICE_WAVE) {
        if (inst->wave_volume > 0) {
            w_printf(ctx, " volume=%d", inst->wave_volume);
        }
        if (inst->wave.count > 0) {
            write_wave_array(ctx, &inst->wave);
        }
    } else if (inst->type == BAX_VOICE_NOISE) {
        if (inst->noise_width > 0) {
            w_printf(ctx, " gb:width=%d", inst->noise_width);
        }
        if (inst->env.present) {
            const char *dir = env_direction_str(inst->env.direction);
            if (inst->has_env_object_syntax) {
                w_printf(ctx, " env={\"level\":%d,\"direction\":\"%s\",\"period\":%d}",
                         inst->env.level, dir, inst->env.period);
            } else {
                w_printf(ctx, " env=gb:%d,%s,%d", inst->env.level, dir, inst->env.period);
            }
        }
    }

    if (inst->uge_note[0] != '\0') {
        w_printf(ctx, " uge_note=%s", inst->uge_note);
    }
    if (inst->note[0] != '\0') {
        w_printf(ctx, " note=%s", inst->note);
    }

    if (inst->length >= 0) {
        w_printf(ctx, " length=%d", inst->length);
    }
    if (inst->vol >= 0) {
        w_printf(ctx, " vol=%d", inst->vol);
    }
    if (inst->gm >= 0) {
        w_printf(ctx, " gm=%d", inst->gm);
    }
    if (inst->subpat[0] != '\0') {
        w_printf(ctx, " subpat=%s", inst->subpat);
    }
    if (inst->pan[0] != '\0') {
        w_printf(ctx, " %span=%s", inst->pan_gb ? "gb:" : "", inst->pan);
    }

    write_macro_array(ctx, "vol_env", &inst->vol_env);
    write_macro_array(ctx, "pitch_env", &inst->pitch_env);
    write_macro_array(ctx, "duty_env", &inst->duty_env);
    write_macro_array(ctx, "arp_env", &inst->arp_env);

    w_printf(ctx, "\n");
}

static void write_effect(write_ctx_t *ctx, const bax_effect_t *eff) {
    /* The effect RHS is opaque text in BeatBax (parsed downstream at render
     * time), so emit the preserved raw expression verbatim. This keeps arp:4,7,10,
     * vib:3,5,sine,3 and bend:... byte-stable without inventing a sub-grammar. */
    if (eff->raw_expr[0] != '\0') {
        w_printf(ctx, "effect %s = %s\n", eff->name, eff->raw_expr);
        return;
    }

    /* Fallback for programmatically-built songs without a raw expression. */
    w_printf(ctx, "effect %s = ", eff->name);
    if (eff->type == BAX_EFFECT_VIBRATO) {
        if (eff->params.vib.waveform[0] != '\0') {
            w_printf(ctx, "vib:%d,%d,%s\n", eff->params.vib.speed, eff->params.vib.depth, eff->params.vib.waveform);
        } else {
            w_printf(ctx, "vib:%d,%d\n", eff->params.vib.speed, eff->params.vib.depth);
        }
    } else if (eff->type == BAX_EFFECT_ARPEGGIO) {
        if (eff->params.arp.semi3 != 0) {
            w_printf(ctx, "arp:%d,%d,%d\n", eff->params.arp.semi1, eff->params.arp.semi2, eff->params.arp.semi3);
        } else {
            w_printf(ctx, "arp:%d,%d\n", eff->params.arp.semi1, eff->params.arp.semi2);
        }
    } else if (eff->type == BAX_EFFECT_PORTAMENTO) {
        w_printf(ctx, "port:%d\n", eff->params.port.speed);
    } else {
        w_printf(ctx, "\n");
    }
}

static void write_subpattern(write_ctx_t *ctx, const bax_subpat_t *sub) {
    w_printf(ctx, "subpat %s =\n", sub->name);
    for (size_t i = 0; i < sub->step_count; i++) {
        w_printf(ctx, "  %s\n", sub->steps[i].raw_line);
    }
}

static void write_pattern(write_ctx_t *ctx, const bax_pattern_t *pat) {
    w_printf(ctx, "pat %s =", pat->name);

    /* Preserve the raw RHS verbatim when present (authoritative for group
     * repeats and other constructs the flat token list can't represent). */
    if (pat->rhs && pat->rhs[0] != '\0') {
        w_printf(ctx, " %s\n", pat->rhs);
        return;
    }

    for (size_t i = 0; i < pat->token_count; i++) {
        const bax_pat_token_t *tok = &pat->tokens[i];
        w_printf(ctx, " ");
        if (tok->kind == BAX_TOKEN_REST) {
            w_printf(ctx, ".");
        } else if (tok->kind == BAX_TOKEN_HOLD) {
            w_printf(ctx, "-");
        } else if (tok->kind == BAX_TOKEN_INLINE_VOL) {
            w_printf(ctx, "%s", tok->text);
        } else if (tok->kind == BAX_TOKEN_INLINE_INST) {
            w_printf(ctx, "%s", tok->text);
        } else {
            /* Note */
            w_printf(ctx, "%s", tok->text);
            if (tok->effect_name[0] != '\0') {
                w_printf(ctx, "<%s>", tok->effect_name);
            }
            if (tok->duration > 1) {
                w_printf(ctx, ":%d", tok->duration);
            }
        }
    }
    w_printf(ctx, "\n");
}

static void write_sequence(write_ctx_t *ctx, const bax_sequence_t *seq) {
    w_printf(ctx, "seq %s =", seq->name);
    for (size_t i = 0; i < seq->item_count; i++) {
        const bax_seq_item_t *item = &seq->items[i];
        if (item->raw[0] != '\0') {
            w_printf(ctx, " %s", item->raw);
            continue;
        }
        w_printf(ctx, " %s", item->pattern_name);
        if (item->inst_override[0] != '\0') {
            w_printf(ctx, ":inst(%s)", item->inst_override);
        }
        if (item->oct_offset != 0) {
            w_printf(ctx, ":oct(%d)", item->oct_offset);
        }
        if (item->vol_override >= 0) {
            w_printf(ctx, ":vol(%d)", item->vol_override);
        }
    }
    w_printf(ctx, "\n");
}

static void write_channel(write_ctx_t *ctx, const bax_channel_t *ch) {
    bool has_inst = ch->default_inst[0] != '\0';
    bool has_refs = ch->ref_count > 0;
    bool has_role = ch->role[0] != '\0';
    if (!has_inst && !has_refs && !has_role) {
        return;
    }
    w_printf(ctx, "channel %d =>", ch->channel_num);
    if (has_inst) {
        w_printf(ctx, " inst %s", ch->default_inst);
    }
    if (has_role) {
        w_printf(ctx, " role %s", ch->role);
        if (ch->form_ref[0] != '\0') {
            w_printf(ctx, " form %s", ch->form_ref);
        }
    }
    if (has_refs) {
        w_printf(ctx, " %s", ch->ref_is_pat ? "pat" : "seq");
        for (size_t i = 0; i < ch->ref_count; i++) {
            w_printf(ctx, " %s", ch->ref_names[i]);
        }
    }
    if (ch->lock_mode[0] != '\0') {
        w_printf(ctx, " lock=%s", ch->lock_mode);
    }
    w_printf(ctx, "\n");
}

static void write_section(write_ctx_t *ctx, const bax_section_t *sec) {
    w_printf(ctx, "section %s {\n", sec->name);
    for (size_t p = 0; p < sec->part_count; p++) {
        const bax_section_part_t *part = &sec->parts[p];
        w_printf(ctx, "  %s =", part->part_name);
        for (size_t k = 0; k < part->pattern_count; k++) {
            w_printf(ctx, " %s", part->patterns[k]);
        }
        w_printf(ctx, "\n");
    }
    w_printf(ctx, "}\n");
}

static void write_form(write_ctx_t *ctx, const bax_form_t *frm) {
    w_printf(ctx, "form %s =", frm->name);
    for (size_t i = 0; i < frm->section_count; i++) {
        if (i > 0) w_printf(ctx, ",");
        w_printf(ctx, " %s", frm->section_names[i]);
    }
    w_printf(ctx, "\n");
}

static void write_channel_auto(write_ctx_t *ctx, const bax_song_t *song) {
    w_printf(ctx, "channel auto form %s", song->auto_form_name);
    if (song->auto_form_inst[0] != '\0') {
        w_printf(ctx, " inst=%s", song->auto_form_inst);
    }
    w_printf(ctx, "\n");
}

/* Serialize a metadata string value into a BeatBax string literal, escaping
 * quotes, backslashes, and control characters (including newlines) so any
 * value — even one containing a newline and three consecutive double quotes —
 * survives a round-trip. The triple-quoted form is deliberately avoided here
 * because it cannot represent an embedded `"""` delimiter. */
static void write_metadata_string(write_ctx_t *ctx, const char *s) {
    w_printf(ctx, "\"");
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') {
            w_printf(ctx, "\\%c", *p);
        } else if (*p == '\n') {
            w_printf(ctx, "\\n");
        } else if (*p == '\t') {
            w_printf(ctx, "\\t");
        } else if (*p == '\r') {
            w_printf(ctx, "\\r");
        } else {
            w_printf(ctx, "%c", *p);
        }
    }
    w_printf(ctx, "\"");
}

static bool write_internal(const bax_song_t *song, write_ctx_t *ctx) {
    /* Metadata */
    if (song->name[0] != '\0') {
        w_printf(ctx, "song name ");
        write_metadata_string(ctx, song->name);
        w_printf(ctx, "\n");
    }
    if (song->artist[0] != '\0') {
        w_printf(ctx, "song artist ");
        write_metadata_string(ctx, song->artist);
        w_printf(ctx, "\n");
    }
    if (song->description[0] != '\0') {
        w_printf(ctx, "song description ");
        write_metadata_string(ctx, song->description);
        w_printf(ctx, "\n");
    }
    if (song->tags[0] != '\0') {
        w_printf(ctx, "song tags ");
        write_metadata_string(ctx, song->tags);
        w_printf(ctx, "\n");
    }
    if (song->name[0] != '\0' || song->artist[0] != '\0' ||
        song->description[0] != '\0' || song->tags[0] != '\0') {
        w_printf(ctx, "\n");
    }

    /* Chip & timing */
    w_printf(ctx, "chip %s\n", song->chip_raw[0] != '\0' ? song->chip_raw : "gameboy");
    w_printf(ctx, "bpm %d\n", song->bpm > 0 ? song->bpm : 120);
    if (song->steps_per_bar > 0 && song->steps_per_bar != 16) {
        w_printf(ctx, "stepsPerBar %d\n", song->steps_per_bar);
    }
    if (song->scale_root[0] != '\0') {
        w_printf(ctx, "scale %s %s%s\n", song->scale_root, song->scale_mode,
                 song->scale_warn ? " warn" : "");
    }
    w_printf(ctx, "\n");

    /* Instruments */
    if (song->instrument_count > 0) {
        for (size_t i = 0; i < song->instrument_count; i++) {
            write_instrument(ctx, &song->instruments[i]);
        }
        w_printf(ctx, "\n");
    }

    /* Effects */
    if (song->effect_count > 0) {
        for (size_t i = 0; i < song->effect_count; i++) {
            write_effect(ctx, &song->effects[i]);
        }
        w_printf(ctx, "\n");
    }

    /* Subpatterns */
    if (song->subpattern_count > 0) {
        for (size_t i = 0; i < song->subpattern_count; i++) {
            write_subpattern(ctx, &song->subpatterns[i]);
            w_printf(ctx, "\n");
        }
    }

    /* Patterns */
    if (song->pattern_count > 0) {
        for (size_t i = 0; i < song->pattern_count; i++) {
            write_pattern(ctx, &song->patterns[i]);
        }
        w_printf(ctx, "\n");
    }

    /* Sequences */
    if (song->sequence_count > 0) {
        for (size_t i = 0; i < song->sequence_count; i++) {
            write_sequence(ctx, &song->sequences[i]);
        }
        w_printf(ctx, "\n");
    }

    /* Sections */
    if (song->section_count > 0) {
        for (size_t i = 0; i < song->section_count; i++) {
            write_section(ctx, &song->sections[i]);
            w_printf(ctx, "\n");
        }
    }

    /* Forms */
    if (song->form_count > 0) {
        for (size_t i = 0; i < song->form_count; i++) {
            write_form(ctx, &song->forms[i]);
        }
        w_printf(ctx, "\n");
    }

    /* Channels */
    for (int i = 0; i < BAX_MAX_CHANNELS; i++) {
        write_channel(ctx, &song->channels[i]);
    }

    /* Channel auto form routing */
    if (song->channel_auto_form) {
        write_channel_auto(ctx, song);
    }
    w_printf(ctx, "\n");

    /* Play statement */
    if (song->has_play) {
        if (song->play_auto || song->play_repeat) {
            w_printf(ctx, "play");
            if (song->play_auto) w_printf(ctx, " auto");
            if (song->play_repeat) w_printf(ctx, " repeat");
            w_printf(ctx, "\n");
        } else {
            w_printf(ctx, "play\n");
        }
    }

    return !ctx->overflow;
}

bool bax_song_write(const bax_song_t *song, char *buf, size_t buf_size, size_t *out_len) {
    if (!song || !buf || buf_size == 0) return false;
    write_ctx_t ctx = { buf, buf_size, 0, false };
    bool ok = write_internal(song, &ctx);
    if (ok && ctx.length < buf_size) {
        buf[ctx.length] = '\0';
        if (out_len) *out_len = ctx.length;
        return true;
    }
    return false;
}

bool bax_song_write_alloc(const bax_song_t *song, char **out_str, size_t *out_len) {
    if (!song || !out_str) return false;

    /* Measure needed length */
    write_ctx_t measure_ctx = { NULL, 0, 0, false };
    write_internal(song, &measure_ctx);

    size_t needed = measure_ctx.length + 1;
    char *buf = (char *)malloc(needed);
    if (!buf) return false;

    write_ctx_t write_ctx = { buf, needed, 0, false };
    bool ok = write_internal(song, &write_ctx);
    if (ok) {
        buf[write_ctx.length] = '\0';
        *out_str = buf;
        if (out_len) *out_len = write_ctx.length;
        return true;
    }

    free(buf);
    return false;
}

bool bax_song_write_file(const bax_song_t *song, const char *path) {
    if (!song || !path) return false;
    char *str = NULL;
    size_t len = 0;
    if (!bax_song_write_alloc(song, &str, &len)) return false;

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(str);
        return false;
    }
    size_t written = fwrite(str, 1, len, f);
    int close_rc = fclose(f);
    free(str);
    return (written == len && close_rc == 0);
}
