/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 BeatBax FM-1 contributors
 *
 * Feature 004 device player for the M-VAVE FM-1.
 * Links the pinned felucca-bsp HAL and the shared fixed-point Game Boy renderer
 * (host/render/gameboy), streams the compiled fixture from flash, and wires
 * PLAY transport + MASTER level + a minimal graphical status screen.
 *
 * Transport protocol (single-writer rule, see specs/global/audio.md):
 *   - `g_stream` (frame/notes/playing) is mutated ONLY by the audio callback.
 *   - The main loop sets `play_request` (volatile, 32-bit) on a PLAY edge; the
 *     callback applies the toggle at the next block boundary and clears it.
 *   - The main loop writes `master_q15` (volatile, 32-bit) from the MASTER ADC.
 *   - The main loop only READS `g_stream.playing` / `g_stream.frame` for the UI.
 */
#include <stdint.h>
#include <stdbool.h>

#define FELUCCA_CDC 0
#define FELUCCA_OTA 1
#define FELUCCA_ID "FM-1_1001"
#define FELUCCA_USB_MANUFACTURER "BeatBax"
#define FELUCCA_USB_PRODUCT "BeatBax FM-1"
/* Required by chip/usb.c MIDI ring producers (compiler barrier). */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")

#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_timer.h"
#include "fm1_input.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "fm1_audio.h"
#include "fm1_flash.h"
#include "libc.c"
#include "usb.c"
#include "ota.c"

#include "gb_stream.h"
#include "gb_song_embed.h" /* compiled grassland_dash.bax blob */

extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _rt_start[], _rt_end[], _rt_load[];
extern void isr_timer5(void);
extern void isr_alnk0(void);

/* PLAY = matrix button 12 (Felucca PANEL_DEFAULT). */
enum { BB_BTN_PLAY = 12 };

#define FRAMES_PER_HALF 128u
#define SAMPLES_PER_HALF (FRAMES_PER_HALF * 2u) /* L,R int32 */
#define AUDIO_HALVES 2u

static volatile uint32_t ms;
static volatile uint32_t play_request; /* main loop -> callback: 1 = toggle transport */
static volatile int32_t master_q15 = 32767; /* 1.0 */

/* MASTER knob: audio (log) taper. The ADC reads 0..1023; index = knob>>3 gives
 * 128 steps. index 0 = mute, then a -40 dB -> 0 dB linear-in-dB curve so the
 * quiet end of the knob has usable resolution. A linear taper packed all the
 * attenuation into the last few % of rotation (full knob = deafening, half
 * knob = only -6 dB), which is why the device felt "too loud".
 *   idx 1   = -40 dB (gain 0.01)
 *   idx 32  = -30 dB (gain 0.032)
 *   idx 64  = -20 dB (gain 0.10)
 *   idx 95  = -10 dB (gain 0.32)
 *   idx 127 =   0 dB (gain 1.0) */
static const uint16_t master_taper[128] = {
        0,   328,   340,   353,   366,   379,   393,   408,   423,   439,   455,   472,
      490,   508,   527,   547,   567,   588,   610,   633,   656,   681,   706,   732,
      759,   788,   817,   847,   879,   912,   946,   981,  1017,  1055,  1095,  1135,
     1178,  1221,  1267,  1314,  1363,  1414,  1466,  1521,  1578,  1636,  1697,  1760,
     1826,  1894,  1964,  2037,  2113,  2192,  2274,  2358,  2446,  2537,  2631,  2729,
     2831,  2936,  3046,  3159,  3277,  3399,  3525,  3656,  3793,  3934,  4080,  4232,
     4390,  4553,  4722,  4898,  5081,  5270,  5466,  5669,  5880,  6099,  6326,  6562,
     6806,  7059,  7322,  7595,  7878,  8171,  8475,  8790,  9118,  9457,  9809, 10174,
    10553, 10946, 11353, 11776, 12214, 12669, 13140, 13630, 14137, 14663, 15209, 15775,
    16362, 16972, 17603, 18259, 18938, 19643, 20375, 21133, 21920, 22736, 23582, 24460,
    25370, 26315, 27294, 28310, 29364, 30457, 31591, 32767,
};

static int32_t audio_buf[SAMPLES_PER_HALF * AUDIO_HALVES] __attribute__((aligned(8)));

#ifdef GB_DEBUG_CAPTURE
/* Worst-case `player_fill` cost, in ticks (FM1_TICKS_PER_US = 24). Written by the
 * audio callback (single writer), read by the main loop for the on-screen readout. */
static volatile uint32_t dbg_max_ticks;
#endif

static gb_stream_t g_stream; /* owned by the audio callback */

static uint32_t boots_ok;
static uint32_t boot_stage;
static struct {
    uint32_t magic, pending, failed;
} guard __attribute__((section(".noinit")));
static uint32_t fault_stage __attribute__((section(".noinit")));

/* ---- audio callback: SRAM fill only; no flash, draw, or USB wait ----
 *
 * The renderer call graph (gb_stream_render_block -> gb_note_step -> voice/
 * period helpers) must also be placed in .ram_text so these calls stay within
 * SRAM; see build.sh and docs/hardware/checklist-004.md (T021). The IRQ enters
 * this leaf via FL_FAR, matching the 001 tone path.
 */
static RAMFN void player_fill(int32_t *dst, uint32_t frames)
{
    gb_sample_t mono[GB_STREAM_MAX_BLOCK];
#ifdef GB_DEBUG_CAPTURE
    uint32_t t0 = fm1_ticks();
#endif

    if (play_request) {
        play_request = 0;
        if (g_stream.playing)
            gb_stream_stop(&g_stream);
        else
            gb_stream_play(&g_stream);
    }

    gb_stream_render_block(&g_stream, mono, frames);

    int32_t gain = master_q15;
    for (uint32_t i = 0; i < frames; i++) {
        int32_t s = (int32_t)mono[i] << 8; /* Q15 -> Q23 */
        s = (int32_t)(((int64_t)s * gain) >> 15);
        dst[i * 2u] = s;
        dst[i * 2u + 1u] = s;
    }

#ifdef GB_DEBUG_CAPTURE
    {
        uint32_t dt = (uint32_t)(fm1_ticks() - t0);
        if (dt > dbg_max_ticks)
            dbg_max_ticks = dt;
    }
#endif
}

void fm1_alnk0_irq(void)
{
    uint8_t p = fm1_audio_pending();
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half();
        FL_FAR(player_fill)(audio_buf + half * SAMPLES_PER_HALF, FRAMES_PER_HALF);
        fm1_audio_ack_half();
    }
}

void fm1_timer5_irq(void)
{
    static uint32_t sub, last, acc;
    uint32_t t = fm1_ticks();
    fm1_timer5_ack();
    fm1_input_tick();
    acc += t - last;
    last = t;
    while (acc >= 1000u * FM1_TICKS_PER_US) {
        acc -= 1000u * FM1_TICKS_PER_US;
        ms++;
    }
    if (++sub == 5u) {
        sub = 0;
        usb_poll();
    }
}

/* ---- LCD (ST7789 240x240 RGB565, bytes swapped on the wire) ---- */

#define C(x) ((uint16_t)((((x) >> 8) | ((x) << 8)) & 0xFFFFu))
enum {
    RED = 0xF800,
    GRN = 0x07E0,
    BLU = 0x001F,
    WHT = 0xFFFF,
    BLK = 0,
    DIM = 0x2104,
    CYN = 0x07FF,
    YEL = 0xFFE0,
    MAG = 0xF81F
};
static uint16_t row_px[240];

static void lcd_cmd(uint8_t c, const uint8_t *d, uint32_t n)
{
    static uint8_t buf[4];
    uint32_t i;
    fm1_lcd_send_cmd(c);
    fm1_lcd_wait();
    fm1_lcd_deselect();
    if (!n)
        return;
    for (i = 0; i < n; i++)
        buf[i] = d[i];
    fm1_lcd_send_data(buf, n);
    fm1_lcd_wait();
    fm1_lcd_deselect();
}

static void delay_ms_fed(uint32_t ms)
{
    while (ms--) {
        fm1_wdt_feed();
        fm1_delay_ms(1);
    }
}

static void lcd_init(void)
{
    static const uint8_t colmod = 0x55, madctl = 0x00;
    fm1_lcd_hw_init();
    delay_ms_fed(50);
    lcd_cmd(0x01, 0, 0);
    delay_ms_fed(150);
    lcd_cmd(0x11, 0, 0);
    delay_ms_fed(120);
    lcd_cmd(0x3A, &colmod, 1);
    lcd_cmd(0x36, &madctl, 1);
    lcd_cmd(0x21, 0, 0);
    lcd_cmd(0x13, 0, 0);
    lcd_cmd(0x29, 0, 0);
    fm1_lcd_baud(4);
}

static void lcd_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t rgb)
{
    uint8_t a[4];
    uint32_t i;
    for (i = 0; i < w; i++)
        row_px[i] = C(rgb);
    a[0] = 0;
    a[1] = (uint8_t)x;
    a[2] = 0;
    a[3] = (uint8_t)(x + w - 1u);
    lcd_cmd(0x2A, a, 4);
    a[1] = (uint8_t)y;
    a[3] = (uint8_t)(y + h - 1u);
    lcd_cmd(0x2B, a, 4);
    lcd_cmd(0x2C, 0, 0);
    for (i = 0; i < h; i++) {
        fm1_lcd_send_data(row_px, w * 2u);
        fm1_lcd_wait();
        fm1_lcd_deselect();
    }
}

/* Tiny 3x5 glyphs (bit 0 = top row). 0-9 digits, then A-Z, then space. */
static const uint8_t GLYPH[][3] = {
    {0x1F, 0x11, 0x1F}, /* 0 */
    {0x00, 0x1F, 0x00}, /* 1 */
    {0x1D, 0x15, 0x17}, /* 2 */
    {0x15, 0x15, 0x1F}, /* 3 */
    {0x07, 0x04, 0x1F}, /* 4 */
    {0x17, 0x15, 0x1D}, /* 5 */
    {0x1F, 0x15, 0x1D}, /* 6 */
    {0x01, 0x01, 0x1F}, /* 7 */
    {0x1F, 0x15, 0x1F}, /* 8 */
    {0x17, 0x15, 0x1F}, /* 9 */
    {0x1E, 0x05, 0x1E}, /* A */
    {0x1F, 0x15, 0x0A}, /* B */
    {0x0E, 0x11, 0x11}, /* C */
    {0x1F, 0x11, 0x0E}, /* D */
    {0x1F, 0x15, 0x11}, /* E */
    {0x1F, 0x05, 0x01}, /* F */
    {0x0E, 0x11, 0x1D}, /* G */
    {0x1F, 0x04, 0x1F}, /* H */
    {0x11, 0x1F, 0x11}, /* I */
    {0x18, 0x10, 0x0F}, /* J */
    {0x1F, 0x04, 0x1B}, /* K */
    {0x1F, 0x10, 0x10}, /* L */
    {0x1F, 0x02, 0x1F}, /* M */
    {0x1F, 0x06, 0x1F}, /* N */
    {0x0E, 0x11, 0x0E}, /* O */
    {0x1F, 0x05, 0x02}, /* P */
    {0x1E, 0x11, 0x0F}, /* Q */
    {0x1F, 0x05, 0x1A}, /* R */
    {0x12, 0x15, 0x09}, /* S */
    {0x01, 0x1F, 0x01}, /* T */
    {0x1F, 0x10, 0x1F}, /* U */
    {0x0F, 0x10, 0x0F}, /* V */
    {0x1F, 0x08, 0x1F}, /* W */
    {0x1B, 0x04, 0x1B}, /* X */
    {0x03, 0x1C, 0x03}, /* Y */
    {0x19, 0x15, 0x13}, /* Z */
    {0x00, 0x00, 0x00}, /* (space) */
};

#define GLYPH_COUNT 37u

/* Map a char to a glyph index: digits, A-Z/a-z; anything else -> space. */
static uint32_t glyph_of(char c)
{
    if (c >= '0' && c <= '9')
        return (uint32_t)(c - '0');
    if (c >= 'A' && c <= 'Z')
        return 10u + (uint32_t)(c - 'A');
    if (c >= 'a' && c <= 'z')
        return 10u + (uint32_t)(c - 'a');
    return 36u;
}

static void lcd_glyph(uint32_t x, uint32_t y, uint32_t gi, uint16_t fg, uint16_t bg)
{
    uint32_t col, row;
    if (gi >= GLYPH_COUNT)
        return;
    for (col = 0; col < 3u; col++) {
        uint8_t bits = GLYPH[gi][col];
        for (row = 0; row < 5u; row++)
            lcd_rect(x + col * 2u, y + row * 2u, 2, 2, (bits & (1u << row)) ? fg : bg);
    }
}

/* Render a NUL-terminated string at a 10 px pitch (6 px glyph + 4 px gap). */
static void lcd_text(uint32_t x, uint32_t y, const char *s, uint16_t fg, uint16_t bg)
{
    uint32_t i = 0;
    while (s[i]) {
        lcd_glyph(x + i * 10u, y, glyph_of(s[i]), fg, bg);
        i++;
    }
}

#ifdef GB_DEBUG_CAPTURE
/* Render a decimal number left-to-right using the 3x5 glyphs (10 px pitch). */
static void lcd_number(uint32_t x, uint32_t y, uint32_t v, uint16_t fg, uint16_t bg)
{
    uint8_t d[8];
    uint32_t n = 0, i = 0;
    if (!v)
        d[n++] = 0;
    while (v && n < 8u) {
        d[n++] = (uint8_t)(v % 10u);
        v /= 10u;
    }
    while (n)
        lcd_glyph(x + i++ * 10u, y, d[--n], fg, bg);
}
#endif

static void draw_title(void)
{
    lcd_rect(0, 0, 240, 18, BLU);
    lcd_text(8, 4, "BEAT", WHT, BLU);
    lcd_text(160, 4, "004", YEL, BLU);
}

static void draw_static(void)
{
    char name[24];
    uint32_t n = 0, len;
    const char *s = gb_song_blob.name;

    lcd_rect(0, 0, 240, 240, BLK);
    draw_title();

    /* Song-name strip (FR-006): render the embedded fixture's name, uppercased
     * and centered, on the dim strip. */
    lcd_rect(4, 26, 232, 12, DIM);
    while (s && s[n] && n < 23u) {
        char c = s[n];
        name[n] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        n++;
    }
    name[n] = 0;
    len = n * 10u;
    lcd_text(4u + (232u - len) / 2u, 27, name, WHT, DIM);
}

/* ---- OTA hooks (installer can replace this image over USB-MIDI) ---- */

static uint8_t flash_ok;
static uint32_t ota_now_ms(void) { return ms; }
static void ota_idle(void) { fm1_wdt_feed(); }
static int ota_in_area(uint32_t off, uint32_t n)
{
    return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN);
}
static int ota_erase(uint32_t off)
{
    uint32_t took;
    if (!ota_in_area(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    return fl_erase4k(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!ota_in_area(off, n))
        return -8;
    return fl_write(off, p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n)
{
    uint8_t *d = p;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static void ota_show(uint32_t step, int32_t code)
{
    lcd_rect(0, 220, 240, 12, BLK);
    if (step < 9u)
        lcd_rect(0, 220, step * 26u, 12, CYN);
    else
        lcd_rect(0, 220, 240, 12, code == 1 ? GRN : RED);
}
static void ota_commit(const uint8_t *parm)
{
    guard.pending = 0;
    guard.failed = 0;
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);
}
static void fm1_fault(const fm1_crash_t *c)
{
    (void)c;
    fault_stage = boot_stage;
    lcd_rect(0, 0, 240, 240, RED);
    if (fault_stage < 5u)
        lcd_rect(200u + fault_stage * 8u, 6, 6, 8, WHT);
    for (;;)
        fm1_wdt_feed();
}

static void mark_stage(uint32_t s)
{
    uint32_t i;
    boot_stage = s;
    for (i = 0; i < 5u; i++)
        lcd_rect(200u + i * 8u, 6, 6, 8, i == s ? YEL : i < s ? GRN : DIM);
}

static void app_main(void)
{
    uint32_t i, last_bar = 0;
    int32_t knob = 0;
    uint32_t knob_w = ~0u, shown_play = ~0u, shown_playhead = ~0u;

    for (i = 0; i < SAMPLES_PER_HALF * AUDIO_HALVES; i++)
        audio_buf[i] = 0;

    FL_FAR(gb_stream_init)(&g_stream, &gb_song_blob);
    /* Boot order contract (see docs/hardware/status-001.md). */
    lcd_init();
    draw_static();
    fm1_lcd_backlight(1);
    mark_stage(0);
    if (fm1_crash.magic == FM1_CRASH_MAGIC) {
        lcd_rect(200, 228, 24, 10, YEL);
        if (fault_stage < 5u)
            lcd_rect(200u + fault_stage * 8u, 6, 6, 8, RED);
    }

    /* Deterministic rescue: hold PLAY at power-on -> ROM download mode. */
    fm1_input_init();
    for (i = 0; i < 4u; i++)
        fm1_input_scan();
    if (fm1_in.buttons & (1u << BB_BTN_PLAY))
        fm1_enter_uboot();

    /* Flash probe + interrupted-update cleanup. */
    {
        uint32_t f = irq_save();
        uint32_t id = FL_FAR(fl_jedec_ram)();
        flash_ok = (id == 0x856014u || id == 0xB36014u);
        irq_restore(f);
        if (flash_ok)
            ota_boot_cleanup();
    }
    mark_stage(1);

    fm1_adc_init();
    usb_start();
    mark_stage(2);
    fm1_timer5_start(isr_timer5, 4);
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    fm1_audio_init(audio_buf, SAMPLES_PER_HALF, isr_alnk0, 3);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    mark_stage(3);
    mark_stage(4);

    for (;;) {
        uint32_t now = ms, pressed, released = 0;
        fm1_wdt_feed();
        usb_retry(now);
        if (!boots_ok && now > 2000u) {
            boots_ok = 1;
            guard.pending = 0;
            guard.failed = 0;
        }
        if (usb.uboot_req) {
            usb_detach();
            fm1_enter_uboot();
        }
        ota_service();
        if (usb.ota_req) {
            usb.ota_req = 0;
            if (flash_ok)
                ota_session();
            draw_static();
            shown_play = ~0u;
            shown_playhead = ~0u;
            knob_w = ~0u;
        }

        pressed = fm1_input_edges(&released);
        if (pressed & (1u << BB_BTN_PLAY))
            play_request = 1u; /* callback toggles transport at the next block */

        if (now - last_bar >= 40u) {
            last_bar = now;
            knob = fm1_adc_read(FM1_ADC_MASTER);
            master_q15 = (int32_t)master_taper[(uint32_t)knob >> 3];
            if (knob != (int32_t)knob_w) {
                knob_w = (uint32_t)knob;
                lcd_rect(4, 42, 232, 10, BLK);
                uint32_t w = (uint32_t)knob * 232u / 1023u;
                if (w)
                    lcd_rect(4, 42, w, 10, WHT);
            }
        }

        /* Status strip: green playing, dim stopped. */
        if (shown_play != (uint32_t)g_stream.playing) {
            shown_play = (uint32_t)g_stream.playing;
            lcd_rect(4, 58, 232, 14, shown_play ? GRN : DIM);
        }

        /* Playhead bar. */
        {
            uint32_t total = gb_song_blob.total_frames;
            uint32_t frame = (uint32_t)(g_stream.frame < total ? g_stream.frame : total);
            uint32_t w = total ? frame * 232u / total : 0;
            if (w != shown_playhead) {
                shown_playhead = w;
                if (w)
                    lcd_rect(4, 78, w, 8, CYN);
                if (w < 232u)
                    lcd_rect(4u + w, 78, 232u - w, 8, DIM);
            }
        }

#ifdef GB_DEBUG_CAPTURE
        /* Callback-budget readout (checklist-004 row 11): worst-case `player_fill`
         * cost in microseconds (FM1_TICKS_PER_US = 24; deadline 2901 us). */
        {
            static uint32_t shown_max = ~0u;
            uint32_t max_us = dbg_max_ticks / FM1_TICKS_PER_US;
            if (max_us != shown_max) {
                shown_max = max_us;
                lcd_rect(4, 92, 232, 12, BLK);
                lcd_text(4, 93, "MAX", CYN, BLK);
                lcd_number(44, 93, max_us, WHT, BLK);
                lcd_text(84, 93, " US", CYN, BLK);
            }
        }
#endif

        for (i = 0; i < 14; i++)
            fm1_led_key(i, (fm1_in.buttons >> i) & 1u);

        lcd_rect(232, 232, 8, 8,
                 usb.config ? ((now >> 9) & 1u ? GRN : DIM) : RED);
        (void)released;
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d;
    fm1_time_init();
    fm1_wdt_arm(0x0F);
    if (guard.magic != 0x42423132u) {
        guard.magic = 0x42423132u;
        guard.failed = 0;
        guard.pending = 0;
    }
    if (guard.pending)
        guard.failed++;
    guard.pending = 1;
    if (guard.failed >= 2u) {
        guard.failed = 0;
        guard.pending = 0;
    }
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;
    app_main();
    for (;;)
        ;
}
