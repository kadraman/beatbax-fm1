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

#include "song_parse.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *bax_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

void bax_error_init(bax_error_t *err) {
    if (!err) return;
    err->code = BAX_OK;
    err->line = 0;
    err->column = 0;
    err->construct[0] = '\0';
    err->message[0] = '\0';
}

void bax_error_set(bax_error_t *err, bax_error_code_t code, int line, int col,
                  const char *construct, const char *fmt, ...) {
    if (!err) return;
    err->code = code;
    err->line = line;
    err->column = col;
    if (construct) {
        strncpy(err->construct, construct, sizeof(err->construct) - 1);
        err->construct[sizeof(err->construct) - 1] = '\0';
    } else {
        err->construct[0] = '\0';
    }
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(err->message, sizeof(err->message), fmt, args);
        va_end(args);
    } else {
        err->message[0] = '\0';
    }
}

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    int line;
    int col;
} parser_ctx_t;

static char peek_char(parser_ctx_t *ctx) {
    if (ctx->pos >= ctx->len) return '\0';
    return ctx->src[ctx->pos];
}

static char next_char(parser_ctx_t *ctx) {
    if (ctx->pos >= ctx->len) return '\0';
    char c = ctx->src[ctx->pos++];
    if (c == '\n') {
        ctx->line++;
        ctx->col = 1;
    } else {
        ctx->col++;
    }
    return c;
}

static void skip_whitespace_inline(parser_ctx_t *ctx) {
    while (ctx->pos < ctx->len) {
        char c = peek_char(ctx);
        if (c == ' ' || c == '\t' || c == '\r') {
            next_char(ctx);
        } else {
            break;
        }
    }
}

static void skip_comment(parser_ctx_t *ctx) {
    if (peek_char(ctx) == '#') {
        while (ctx->pos < ctx->len && peek_char(ctx) != '\n') {
            next_char(ctx);
        }
    }
}

static void skip_whitespace_and_comments(parser_ctx_t *ctx) {
    while (ctx->pos < ctx->len) {
        char c = peek_char(ctx);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            next_char(ctx);
        } else if (c == '#') {
            skip_comment(ctx);
        } else {
            break;
        }
    }
}

static bool read_ident(parser_ctx_t *ctx, char *buf, size_t buf_size) {
    skip_whitespace_inline(ctx);
    size_t idx = 0;
    while (ctx->pos < ctx->len) {
        char c = peek_char(ctx);
        if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.' || c == ':') {
            if (idx + 1 < buf_size) {
                buf[idx++] = c;
            }
            next_char(ctx);
        } else {
            break;
        }
    }
    buf[idx] = '\0';
    return idx > 0;
}

static bool read_string_literal(parser_ctx_t *ctx, char *buf, size_t buf_size) {
    skip_whitespace_inline(ctx);
    if (ctx->pos + 2 < ctx->len &&
        ctx->src[ctx->pos] == '"' &&
        ctx->src[ctx->pos + 1] == '"' &&
        ctx->src[ctx->pos + 2] == '"') {
        /* Triple-quoted string */
        next_char(ctx);
        next_char(ctx);
        next_char(ctx);
        size_t idx = 0;
        while (ctx->pos + 2 < ctx->len) {
            if (ctx->src[ctx->pos] == '"' &&
                ctx->src[ctx->pos + 1] == '"' &&
                ctx->src[ctx->pos + 2] == '"') {
                next_char(ctx);
                next_char(ctx);
                next_char(ctx);
                buf[idx] = '\0';
                return true;
            }
            char c = next_char(ctx);
            if (idx + 1 < buf_size) {
                buf[idx++] = c;
            }
        }
        buf[idx] = '\0';
        return false;
    } else if (peek_char(ctx) == '"') {
        next_char(ctx);
        size_t idx = 0;
        while (ctx->pos < ctx->len) {
            char c = next_char(ctx);
            if (c == '"') {
                buf[idx] = '\0';
                return true;
            }
            if (c == '\\' && ctx->pos < ctx->len) {
                char esc = next_char(ctx);
                if (esc == 'n') c = '\n';
                else if (esc == 't') c = '\t';
                else if (esc == 'r') c = '\r';
                else c = esc;
            }
            if (idx + 1 < buf_size) {
                buf[idx++] = c;
            }
        }
        buf[idx] = '\0';
        return false;
    }
    return false;
}

static bool parse_macro_array(const char *str, bax_macro_env_t *macro) {
    macro->count = 0;
    macro->loop_point = -1;
    const char *p = strchr(str, '[');
    if (!p) return false;
    p++;
    while (*p && *p != ']') {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == ']') break;
        if (*p == '|') {
            p++;
            macro->loop_point = (int)strtol(p, (char **)&p, 10);
            continue;
        }
        char *end;
        long val = strtol(p, &end, 10);
        if (end == p) break;
        if (macro->count < BAX_MAX_MACRO_LEN) {
            macro->values[macro->count++] = (int8_t)val;
        }
        p = end;
    }
    return true;
}

static bool parse_wave_array(const char *str, bax_gb_wave_t *wave) {
    wave->count = 0;
    const char *p = strchr(str, '[');
    if (!p) return false;
    p++;
    while (*p && *p != ']') {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == ']') break;
        char *end;
        long val = strtol(p, &end, 10);
        if (end == p) break;
        if (wave->count < BAX_MAX_WAVE_SAMPLES) {
            wave->samples[wave->count++] = (uint8_t)val;
        }
        p = end;
    }
    return true;
}

static void parse_json_object(const char *str, const char *key, char *out_val, size_t out_size) {
    out_val[0] = '\0';
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(str, search);
    if (!p) return;
    p += strlen(search);
    while (*p == ' ' || *p == ':') p++;
    if (*p == '"') {
        p++;
        size_t idx = 0;
        while (*p && *p != '"' && idx + 1 < out_size) {
            out_val[idx++] = *p++;
        }
        out_val[idx] = '\0';
    } else {
        size_t idx = 0;
        while (*p && *p != ',' && *p != '}' && *p != ' ' && idx + 1 < out_size) {
            out_val[idx++] = *p++;
        }
        out_val[idx] = '\0';
    }
}

static bool parse_instrument_option(bax_instrument_t *inst, const char *opt) {
    if (strncmp(opt, "type=", 5) == 0) {
        const char *val = opt + 5;
        if (strcmp(val, "pulse1") == 0) inst->type = BAX_VOICE_PULSE1;
        else if (strcmp(val, "pulse2") == 0) inst->type = BAX_VOICE_PULSE2;
        else if (strcmp(val, "wave") == 0) inst->type = BAX_VOICE_WAVE;
        else if (strcmp(val, "noise") == 0) inst->type = BAX_VOICE_NOISE;
        else return false;
    } else if (strncmp(opt, "duty=", 5) == 0) {
        inst->duty = (float)atof(opt + 5);
    } else if (strncmp(opt, "env=", 4) == 0) {
        const char *val = opt + 4;
        inst->env.present = true;
        if (*val == '{') {
            inst->has_env_object_syntax = true;
            char buf[32];
            parse_json_object(val, "level", buf, sizeof(buf));
            inst->env.level = (uint8_t)atoi(buf);
            parse_json_object(val, "direction", buf, sizeof(buf));
            inst->env.direction = (strcmp(buf, "up") == 0) ? BAX_ENV_DIR_UP : BAX_ENV_DIR_DOWN;
            parse_json_object(val, "period", buf, sizeof(buf));
            inst->env.period = (uint8_t)atoi(buf);
        } else if (strncmp(val, "gb:", 3) == 0) {
            val += 3;
            inst->env.level = (uint8_t)atoi(val);
            const char *c1 = strchr(val, ',');
            if (c1) {
                c1++;
                if (strncmp(c1, "up", 2) == 0) inst->env.direction = BAX_ENV_DIR_UP;
                else inst->env.direction = BAX_ENV_DIR_DOWN;
                const char *c2 = strchr(c1, ',');
                if (c2) inst->env.period = (uint8_t)atoi(c2 + 1);
            }
        }
    } else if (strncmp(opt, "sweep=", 6) == 0) {
        const char *val = opt + 6;
        inst->sweep.present = true;
        if (*val == '{') {
            inst->has_sweep_object_syntax = true;
            char buf[32];
            parse_json_object(val, "time", buf, sizeof(buf));
            inst->sweep.time = (uint8_t)atoi(buf);
            parse_json_object(val, "direction", buf, sizeof(buf));
            inst->sweep.direction = (strcmp(buf, "down") == 0) ? BAX_SWEEP_DIR_DOWN : BAX_SWEEP_DIR_UP;
            parse_json_object(val, "shift", buf, sizeof(buf));
            inst->sweep.shift = (uint8_t)atoi(buf);
        }
    } else if (strncmp(opt, "volume=", 7) == 0) {
        inst->wave_volume = (uint8_t)atoi(opt + 7);
    } else if (strncmp(opt, "wave=", 5) == 0) {
        parse_wave_array(opt + 5, &inst->wave);
    } else if (strncmp(opt, "gb:width=", 9) == 0) {
        inst->noise_width = (uint8_t)atoi(opt + 9);
    } else if (strncmp(opt, "uge_note=", 9) == 0) {
        strncpy(inst->uge_note, opt + 9, sizeof(inst->uge_note) - 1);
    } else if (strncmp(opt, "note=", 5) == 0) {
        strncpy(inst->note, opt + 5, sizeof(inst->note) - 1);
    } else if (strncmp(opt, "length=", 7) == 0) {
        inst->length = atoi(opt + 7);
    } else if (strncmp(opt, "vol=", 4) == 0) {
        inst->vol = atoi(opt + 4);
    } else if (strncmp(opt, "gm=", 3) == 0) {
        inst->gm = atoi(opt + 3);
    } else if (strncmp(opt, "subpat=", 7) == 0) {
        strncpy(inst->subpat, opt + 7, sizeof(inst->subpat) - 1);
    } else if (strncmp(opt, "vol_env=", 8) == 0) {
        parse_macro_array(opt + 8, &inst->vol_env);
    } else if (strncmp(opt, "pitch_env=", 10) == 0) {
        parse_macro_array(opt + 10, &inst->pitch_env);
    }
    return true;
}

static bool parse_pattern_token(const char *tok, bax_pat_token_t *ptok) {
    memset(ptok, 0, sizeof(bax_pat_token_t));
    ptok->duration = 1;
    ptok->semitone = -1;
    ptok->octave = -1;

    if (strcmp(tok, ".") == 0) {
        ptok->kind = BAX_TOKEN_REST;
        strncpy(ptok->text, ".", sizeof(ptok->text) - 1);
        return true;
    }
    if (strcmp(tok, "-") == 0) {
        ptok->kind = BAX_TOKEN_HOLD;
        strncpy(ptok->text, "-", sizeof(ptok->text) - 1);
        return true;
    }
    if (strncmp(tok, "vol:", 4) == 0) {
        ptok->kind = BAX_TOKEN_INLINE_VOL;
        ptok->vol_level = atoi(tok + 4);
        strncpy(ptok->text, tok, sizeof(ptok->text) - 1);
        return true;
    }

    /* Check for note or token with modifiers: NOTE<effect>:dur or NOTE:dur */
    char base[BAX_MAX_NAME_LEN];
    base[0] = '\0';
    const char *p = tok;
    size_t bi = 0;
    while (*p && *p != '<' && *p != ':') {
        if (bi + 1 < sizeof(base)) base[bi++] = *p;
        p++;
    }
    base[bi] = '\0';

    if (*p == '<') {
        p++;
        size_t ei = 0;
        while (*p && *p != '>') {
            if (ei + 1 < sizeof(ptok->effect_name)) ptok->effect_name[ei++] = *p;
            p++;
        }
        ptok->effect_name[ei] = '\0';
        if (*p == '>') p++;
    }

    if (*p == ':') {
        p++;
        ptok->duration = atoi(p);
    }

    strncpy(ptok->text, base, sizeof(ptok->text) - 1);

    /* Determine if base is a musical note (e.g. C3, D#4, Eb5, etc.) */
    char note_letter = (char)toupper((unsigned char)base[0]);
    if (note_letter >= 'A' && note_letter <= 'G') {
        ptok->kind = BAX_TOKEN_NOTE;
        /* Calculate semitone and octave if present */
        int semi = 0;
        switch (note_letter) {
            case 'C': semi = 0; break;
            case 'D': semi = 2; break;
            case 'E': semi = 4; break;
            case 'F': semi = 5; break;
            case 'G': semi = 7; break;
            case 'A': semi = 9; break;
            case 'B': semi = 11; break;
        }
        const char *np = base + 1;
        if (*np == '#') { semi++; np++; }
        else if (*np == 'b') { semi--; np++; }
        else if (*np == '-') { np++; }

        if (isdigit((unsigned char)*np)) {
            ptok->octave = *np - '0';
            ptok->semitone = (semi + 12) % 12;
        }
    } else {
        ptok->kind = BAX_TOKEN_INLINE_INST;
    }

    return true;
}

static bool parse_seq_item(const char *tok, bax_seq_item_t *item) {
    memset(item, 0, sizeof(bax_seq_item_t));
    item->vol_override = -1;

    char base[BAX_MAX_NAME_LEN];
    base[0] = '\0';
    const char *p = tok;
    size_t bi = 0;
    while (*p && *p != ':') {
        if (bi + 1 < sizeof(base)) base[bi++] = *p;
        p++;
    }
    base[bi] = '\0';
    strncpy(item->pattern_name, base, sizeof(item->pattern_name) - 1);

    while (*p == ':') {
        p++;
        if (strncmp(p, "inst(", 5) == 0) {
            p += 5;
            size_t ii = 0;
            while (*p && *p != ')') {
                if (ii + 1 < sizeof(item->inst_override)) item->inst_override[ii++] = *p;
                p++;
            }
            item->inst_override[ii] = '\0';
            if (*p == ')') p++;
        } else if (strncmp(p, "oct(", 4) == 0) {
            p += 4;
            item->oct_offset = atoi(p);
            while (*p && *p != ')') p++;
            if (*p == ')') p++;
        } else if (strncmp(p, "vol(", 4) == 0) {
            p += 4;
            item->vol_override = atoi(p);
            while (*p && *p != ')') p++;
            if (*p == ')') p++;
        } else {
            while (*p && *p != ':') p++;
        }
    }
    return true;
}

bool bax_song_parse(const char *text, size_t len, bax_song_t *out_song, bax_error_t *out_err) {
    if (!text || len == 0) {
        bax_error_set(out_err, BAX_ERR_EMPTY_FILE, 1, 1, "song", "empty song file");
        return false;
    }

    parser_ctx_t ctx = { text, len, 0, 1, 1 };
    bax_song_init(out_song);
    bax_error_init(out_err);

    bool has_any_content = false;

    while (ctx.pos < ctx.len) {
        skip_whitespace_and_comments(&ctx);
        if (ctx.pos >= ctx.len) break;

        has_any_content = true;
        int line_start = ctx.line;
        int col_start = ctx.col;

        char keyword[BAX_MAX_NAME_LEN];
        if (!read_ident(&ctx, keyword, sizeof(keyword))) {
            bax_error_set(out_err, BAX_ERR_SYNTAX, ctx.line, ctx.col, "token", "unexpected character");
            bax_song_free(out_song);
            return false;
        }

        if (strcmp(keyword, "song") == 0) {
            char subkey[BAX_MAX_NAME_LEN];
            if (!read_ident(&ctx, subkey, sizeof(subkey))) {
                bax_error_set(out_err, BAX_ERR_SYNTAX, ctx.line, ctx.col, "song", "missing song metadata key");
                bax_song_free(out_song);
                return false;
            }
            char str_val[BAX_MAX_DESC_LEN];
            if (!read_string_literal(&ctx, str_val, sizeof(str_val))) {
                bax_error_set(out_err, BAX_ERR_SYNTAX, ctx.line, ctx.col, subkey, "missing or malformed string literal");
                bax_song_free(out_song);
                return false;
            }
            if (strcmp(subkey, "name") == 0) {
                strncpy(out_song->name, str_val, sizeof(out_song->name) - 1);
            } else if (strcmp(subkey, "artist") == 0) {
                strncpy(out_song->artist, str_val, sizeof(out_song->artist) - 1);
            } else if (strcmp(subkey, "description") == 0) {
                strncpy(out_song->description, str_val, sizeof(out_song->description) - 1);
            } else if (strcmp(subkey, "tags") == 0) {
                strncpy(out_song->tags, str_val, sizeof(out_song->tags) - 1);
            }
        } else if (strcmp(keyword, "chip") == 0) {
            char chip_name[BAX_MAX_NAME_LEN];
            read_ident(&ctx, chip_name, sizeof(chip_name));
            strncpy(out_song->chip_raw, chip_name, sizeof(out_song->chip_raw) - 1);

            if (strcmp(chip_name, "gameboy") == 0 ||
                strcmp(chip_name, "gb") == 0 ||
                strcmp(chip_name, "dmg") == 0) {
                out_song->chip = BAX_CHIP_GAMEBOY;
            } else {
                bax_error_set(out_err, BAX_ERR_UNSUPPORTED_CHIP, line_start, col_start,
                              "chip", "unsupported chip: %s", chip_name);
                bax_song_free(out_song);
                return false;
            }
        } else if (strcmp(keyword, "bpm") == 0) {
            char bpm_str[32];
            read_ident(&ctx, bpm_str, sizeof(bpm_str));
            out_song->bpm = atoi(bpm_str);
        } else if (strcmp(keyword, "stepsPerBar") == 0) {
            char spb_str[32];
            read_ident(&ctx, spb_str, sizeof(spb_str));
            out_song->steps_per_bar = atoi(spb_str);
        } else if (strcmp(keyword, "scale") == 0) {
            read_ident(&ctx, out_song->scale_root, sizeof(out_song->scale_root));
            read_ident(&ctx, out_song->scale_mode, sizeof(out_song->scale_mode));
            skip_whitespace_inline(&ctx);
            if (ctx.pos < ctx.len && peek_char(&ctx) != '\n' && peek_char(&ctx) != '#') {
                char warn_token[16];
                read_ident(&ctx, warn_token, sizeof(warn_token));
                if (strcmp(warn_token, "warn") == 0) {
                    out_song->scale_warn = true;
                }
            }
        } else if (strcmp(keyword, "import") == 0) {
            char import_path[BAX_MAX_STR_LEN];
            read_string_literal(&ctx, import_path, sizeof(import_path));
            bax_error_set(out_err, BAX_ERR_UNSUPPORTED_CONSTRUCT, line_start, col_start,
                          "import", "unsupported directive: import (device songs must be self-contained)");
            bax_song_free(out_song);
            return false;
        } else if (strcmp(keyword, "inst") == 0) {
            char inst_name[BAX_MAX_NAME_LEN];
            read_ident(&ctx, inst_name, sizeof(inst_name));
            bax_instrument_t *inst = bax_song_add_instrument(out_song, inst_name, BAX_VOICE_UNKNOWN);

            /* Parse key=val options on the line (or continuation lines) */
            while (ctx.pos < ctx.len) {
                skip_whitespace_inline(&ctx);
                if (peek_char(&ctx) == '\n' || peek_char(&ctx) == '\0') {
                    next_char(&ctx);
                    skip_whitespace_inline(&ctx);
                    /* If next line starts with '#' or is not an option continuation, break */
                    if (peek_char(&ctx) == '#' || isalpha((unsigned char)peek_char(&ctx))) {
                        /* Check if next token is a keyword */
                        size_t save_pos = ctx.pos;
                        int save_line = ctx.line;
                        int save_col = ctx.col;
                        char next_word[32];
                        read_ident(&ctx, next_word, sizeof(next_word));
                        ctx.pos = save_pos;
                        ctx.line = save_line;
                        ctx.col = save_col;
                        if (strcmp(next_word, "inst") == 0 ||
                            strcmp(next_word, "pat") == 0 ||
                            strcmp(next_word, "seq") == 0 ||
                            strcmp(next_word, "channel") == 0 ||
                            strcmp(next_word, "play") == 0 ||
                            strcmp(next_word, "effect") == 0 ||
                            strcmp(next_word, "subpat") == 0 ||
                            strcmp(next_word, "song") == 0 ||
                            strcmp(next_word, "chip") == 0 ||
                            strcmp(next_word, "bpm") == 0) {
                            break;
                        }
                    } else {
                        break;
                    }
                }
                if (peek_char(&ctx) == '#') {
                    skip_comment(&ctx);
                    continue;
                }

                /* Read key=value token */
                char opt_buf[BAX_MAX_STR_LEN];
                size_t oi = 0;
                bool in_quote = false;
                bool in_bracket = false;
                bool in_brace = false;

                while (ctx.pos < ctx.len) {
                    char c = peek_char(&ctx);
                    if (c == '"') in_quote = !in_quote;
                    else if (!in_quote) {
                        if (c == '[') in_bracket = true;
                        else if (c == ']') in_bracket = false;
                        else if (c == '{') in_brace = true;
                        else if (c == '}') in_brace = false;
                        else if ((c == ' ' || c == '\t' || c == '\r' || c == '\n') && !in_bracket && !in_brace) {
                            break;
                        }
                    }
                    if (oi + 1 < sizeof(opt_buf)) {
                        opt_buf[oi++] = c;
                    }
                    next_char(&ctx);
                }
                opt_buf[oi] = '\0';
                if (oi > 0) {
                    parse_instrument_option(inst, opt_buf);
                }
            }
        } else if (strcmp(keyword, "effect") == 0) {
            char eff_name[BAX_MAX_NAME_LEN];
            read_ident(&ctx, eff_name, sizeof(eff_name));
            skip_whitespace_inline(&ctx);
            if (peek_char(&ctx) == '=') next_char(&ctx);
            skip_whitespace_inline(&ctx);

            char eff_expr[BAX_MAX_STR_LEN];
            size_t ei = 0;
            while (ctx.pos < ctx.len && peek_char(&ctx) != '\n' && peek_char(&ctx) != '#') {
                char c = peek_char(&ctx);
                if (c == ' ' || c == '\t' || c == '\r') break;
                if (ei + 1 < sizeof(eff_expr)) eff_expr[ei++] = c;
                next_char(&ctx);
            }
            eff_expr[ei] = '\0';

            bax_effect_type_t etype = BAX_EFFECT_NONE;
            if (strncmp(eff_expr, "vib:", 4) == 0) etype = BAX_EFFECT_VIBRATO;
            else if (strncmp(eff_expr, "arp:", 4) == 0) etype = BAX_EFFECT_ARPEGGIO;
            else if (strncmp(eff_expr, "port:", 5) == 0) etype = BAX_EFFECT_PORTAMENTO;

            bax_effect_t *eff = bax_song_add_effect(out_song, eff_name, etype);
            if (etype == BAX_EFFECT_VIBRATO) {
                const char *p = eff_expr + 4;
                eff->params.vib.speed = atoi(p);
                const char *c1 = strchr(p, ',');
                if (c1) {
                    eff->params.vib.depth = atoi(c1 + 1);
                    const char *c2 = strchr(c1 + 1, ',');
                    if (c2) {
                        strncpy(eff->params.vib.waveform, c2 + 1, sizeof(eff->params.vib.waveform) - 1);
                    }
                }
            } else if (etype == BAX_EFFECT_ARPEGGIO) {
                const char *p = eff_expr + 4;
                eff->params.arp.semi1 = atoi(p);
                const char *c1 = strchr(p, ',');
                if (c1) {
                    eff->params.arp.semi2 = atoi(c1 + 1);
                }
            } else if (etype == BAX_EFFECT_PORTAMENTO) {
                eff->params.port.speed = atoi(eff_expr + 5);
            }
        } else if (strcmp(keyword, "subpat") == 0) {
            char sub_name[BAX_MAX_NAME_LEN];
            read_ident(&ctx, sub_name, sizeof(sub_name));
            skip_whitespace_inline(&ctx);
            if (peek_char(&ctx) == '=') next_char(&ctx);

            bax_subpat_t *sub = bax_song_add_subpat(out_song, sub_name);

            /* Read lines until next top-level keyword */
            while (ctx.pos < ctx.len) {
                skip_whitespace_inline(&ctx);
                if (peek_char(&ctx) == '\n') {
                    next_char(&ctx);
                    continue;
                }
                if (peek_char(&ctx) == '#') {
                    skip_comment(&ctx);
                    continue;
                }

                /* Check if next token is top-level definition */
                size_t save_pos = ctx.pos;
                int save_line = ctx.line;
                int save_col = ctx.col;
                char next_tok[32];
                read_ident(&ctx, next_tok, sizeof(next_tok));
                ctx.pos = save_pos;
                ctx.line = save_line;
                ctx.col = save_col;

                if (strcmp(next_tok, "subpat") == 0 ||
                    strcmp(next_tok, "pat") == 0 ||
                    strcmp(next_tok, "seq") == 0 ||
                    strcmp(next_tok, "channel") == 0 ||
                    strcmp(next_tok, "play") == 0 ||
                    strcmp(next_tok, "inst") == 0 ||
                    strcmp(next_tok, "effect") == 0) {
                    break;
                }

                /* Read full line as step */
                char step_line[BAX_MAX_STR_LEN];
                size_t si = 0;
                while (ctx.pos < ctx.len && peek_char(&ctx) != '\n') {
                    if (si + 1 < sizeof(step_line)) {
                        step_line[si++] = next_char(&ctx);
                    } else {
                        next_char(&ctx);
                    }
                }
                step_line[si] = '\0';
                if (peek_char(&ctx) == '\n') next_char(&ctx);

                if (sub->step_count >= sub->step_capacity) {
                    size_t new_cap = sub->step_capacity == 0 ? 8 : sub->step_capacity * 2;
                    bax_subpat_step_t *new_steps = (bax_subpat_step_t *)realloc(sub->steps, new_cap * sizeof(bax_subpat_step_t));
                    if (!new_steps) break;
                    sub->steps = new_steps;
                    sub->step_capacity = new_cap;
                }
                strncpy(sub->steps[sub->step_count++].raw_line, step_line, sizeof(sub->steps[0].raw_line) - 1);
            }
        } else if (strcmp(keyword, "pat") == 0) {
            char pat_name[BAX_MAX_NAME_LEN];
            read_ident(&ctx, pat_name, sizeof(pat_name));
            skip_whitespace_inline(&ctx);
            if (peek_char(&ctx) == '=') next_char(&ctx);

            bax_pattern_t *pat = bax_song_add_pattern(out_song, pat_name);

            /* Read tokens until newline */
            while (ctx.pos < ctx.len && peek_char(&ctx) != '\n') {
                skip_whitespace_inline(&ctx);
                if (peek_char(&ctx) == '\n' || peek_char(&ctx) == '\0') break;
                if (peek_char(&ctx) == '#') {
                    skip_comment(&ctx);
                    break;
                }

                char tok[BAX_MAX_NAME_LEN];
                size_t ti = 0;
                while (ctx.pos < ctx.len) {
                    char c = peek_char(&ctx);
                    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '#') break;
                    if (ti + 1 < sizeof(tok)) tok[ti++] = c;
                    next_char(&ctx);
                }
                tok[ti] = '\0';
                if (ti == 0) continue;

                if (strcmp(tok, "inst") == 0) {
                    /* Read the instrument name following 'inst' */
                    skip_whitespace_inline(&ctx);
                    char inst_tok[BAX_MAX_NAME_LEN];
                    read_ident(&ctx, inst_tok, sizeof(inst_tok));
                    if (pat->token_count >= pat->token_capacity) {
                        size_t new_cap = pat->token_capacity == 0 ? 16 : pat->token_capacity * 2;
                        bax_pat_token_t *narr = (bax_pat_token_t *)realloc(pat->tokens, new_cap * sizeof(bax_pat_token_t));
                        if (!narr) break;
                        pat->tokens = narr;
                        pat->token_capacity = new_cap;
                    }
                    bax_pat_token_t *ptok = &pat->tokens[pat->token_count++];
                    memset(ptok, 0, sizeof(bax_pat_token_t));
                    ptok->kind = BAX_TOKEN_INLINE_INST;
                    strncpy(ptok->text, inst_tok, sizeof(ptok->text) - 1);
                    continue;
                }

                if (pat->token_count >= pat->token_capacity) {
                    size_t new_cap = pat->token_capacity == 0 ? 16 : pat->token_capacity * 2;
                    bax_pat_token_t *narr = (bax_pat_token_t *)realloc(pat->tokens, new_cap * sizeof(bax_pat_token_t));
                    if (!narr) break;
                    pat->tokens = narr;
                    pat->token_capacity = new_cap;
                }

                bax_pat_token_t *ptok = &pat->tokens[pat->token_count++];
                parse_pattern_token(tok, ptok);
            }
        } else if (strcmp(keyword, "seq") == 0) {
            char seq_name[BAX_MAX_NAME_LEN];
            read_ident(&ctx, seq_name, sizeof(seq_name));
            skip_whitespace_inline(&ctx);
            if (peek_char(&ctx) == '=') next_char(&ctx);

            bax_sequence_t *seq = bax_song_add_sequence(out_song, seq_name);

            /* Read pattern references until newline */
            while (ctx.pos < ctx.len && peek_char(&ctx) != '\n') {
                skip_whitespace_inline(&ctx);
                if (peek_char(&ctx) == '\n' || peek_char(&ctx) == '\0') break;
                if (peek_char(&ctx) == '#') {
                    skip_comment(&ctx);
                    break;
                }

                char item_tok[BAX_MAX_NAME_LEN];
                size_t ii = 0;
                while (ctx.pos < ctx.len) {
                    char c = peek_char(&ctx);
                    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '#') break;
                    if (ii + 1 < sizeof(item_tok)) item_tok[ii++] = c;
                    next_char(&ctx);
                }
                item_tok[ii] = '\0';
                if (ii == 0) continue;

                if (seq->item_count >= seq->item_capacity) {
                    size_t new_cap = seq->item_capacity == 0 ? 8 : seq->item_capacity * 2;
                    bax_seq_item_t *narr = (bax_seq_item_t *)realloc(seq->items, new_cap * sizeof(bax_seq_item_t));
                    if (!narr) break;
                    seq->items = narr;
                    seq->item_capacity = new_cap;
                }
                bax_seq_item_t *item = &seq->items[seq->item_count++];
                parse_seq_item(item_tok, item);
            }
        } else if (strcmp(keyword, "channel") == 0) {
            char ch_num_str[16];
            read_ident(&ctx, ch_num_str, sizeof(ch_num_str));
            int ch_num = atoi(ch_num_str);
            if (ch_num < 1 || ch_num > BAX_MAX_CHANNELS) {
                bax_error_set(out_err, BAX_ERR_INVALID_CHANNEL, line_start, col_start,
                              "channel", "channel number %d out of range (1..%d)", ch_num, BAX_MAX_CHANNELS);
                bax_song_free(out_song);
                return false;
            }
            bax_channel_t *ch = &out_song->channels[ch_num - 1];

            skip_whitespace_inline(&ctx);
            if (ctx.pos + 1 < ctx.len && ctx.src[ctx.pos] == '=' && ctx.src[ctx.pos + 1] == '>') {
                next_char(&ctx);
                next_char(&ctx);
            }

            while (ctx.pos < ctx.len && peek_char(&ctx) != '\n') {
                skip_whitespace_inline(&ctx);
                if (peek_char(&ctx) == '\n' || peek_char(&ctx) == '\0') break;
                if (peek_char(&ctx) == '#') {
                    skip_comment(&ctx);
                    break;
                }

                char word[BAX_MAX_NAME_LEN];
                read_ident(&ctx, word, sizeof(word));
                if (strcmp(word, "inst") == 0) {
                    read_ident(&ctx, ch->default_inst, sizeof(ch->default_inst));
                } else if (strcmp(word, "seq") == 0) {
                    /* Next words are sequence names */
                    while (ctx.pos < ctx.len && peek_char(&ctx) != '\n') {
                        skip_whitespace_inline(&ctx);
                        if (peek_char(&ctx) == '\n' || peek_char(&ctx) == '\0' || peek_char(&ctx) == '#') break;
                        char sname[BAX_MAX_NAME_LEN];
                        read_ident(&ctx, sname, sizeof(sname));
                        if (strncmp(sname, "lock=", 5) == 0) {
                            ch->lock_scale = true;
                            continue;
                        }
                        if (sname[0] == '\0') break;

                        if (ch->seq_count >= ch->seq_capacity) {
                            size_t new_cap = ch->seq_capacity == 0 ? 4 : ch->seq_capacity * 2;
                            char **narr = (char **)realloc(ch->seq_names, new_cap * sizeof(char *));
                            if (!narr) break;
                            ch->seq_names = narr;
                            ch->seq_capacity = new_cap;
                        }
                        ch->seq_names[ch->seq_count++] = bax_strdup(sname);
                    }
                } else if (strncmp(word, "lock=", 5) == 0) {
                    ch->lock_scale = true;
                }
            }
        } else if (strcmp(keyword, "play") == 0) {
            out_song->has_play = true;
            skip_whitespace_inline(&ctx);
            if (ctx.pos < ctx.len && peek_char(&ctx) != '\n' && peek_char(&ctx) != '#') {
                char mode[32];
                read_ident(&ctx, mode, sizeof(mode));
                if (strcmp(mode, "auto") == 0) {
                    out_song->play_auto = true;
                }
            }
        } else {
            /* Unknown top-level directive */
            bax_error_set(out_err, BAX_ERR_UNSUPPORTED_CONSTRUCT, line_start, col_start,
                          keyword, "unsupported top-level directive: %s", keyword);
            bax_song_free(out_song);
            return false;
        }
    }

    if (!has_any_content) {
        bax_error_set(out_err, BAX_ERR_EMPTY_FILE, 1, 1, "song", "empty song file");
        bax_song_free(out_song);
        return false;
    }

    return true;
}

bool bax_song_parse_file(const char *path, bax_song_t *out_song, bax_error_t *out_err) {
    if (!path || !out_song) {
        bax_error_set(out_err, BAX_ERR_IO, 0, 0, "file", "invalid parameters");
        return false;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        bax_error_set(out_err, BAX_ERR_IO, 0, 0, "file", "failed to open file '%s'", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) {
        fclose(f);
        bax_error_set(out_err, BAX_ERR_IO, 0, 0, "file", "failed to determine file size '%s'", path);
        return false;
    }
    if (sz == 0) {
        fclose(f);
        bax_error_set(out_err, BAX_ERR_EMPTY_FILE, 1, 1, "song", "empty file '%s'", path);
        return false;
    }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        bax_error_set(out_err, BAX_ERR_OUT_OF_MEMORY, 0, 0, "alloc", "out of memory reading '%s'", path);
        return false;
    }

    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    buf[rd] = '\0';

    bool ok = bax_song_parse(buf, rd, out_song, out_err);
    free(buf);
    return ok;
}
