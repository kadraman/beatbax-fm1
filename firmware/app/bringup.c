/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 BeatBax FM-1 contributors
 *
 * Feature 001 platform bring-up diagnostic for the M-VAVE FM-1.
 * Links the pinned felucca-bsp HAL (see ../FELUCCA_PIN). Patterns only from
 * felucca-bsp examples/hwtest — no Felucca synth engines, no stock binary SDK.
 *
 * Controls: every matrix button (0..13), note key (0..26), encoder (0..6), and
 * MASTER illuminate on the diagnostic screen. PLAY (matrix id 12, Felucca
 * PANEL_DEFAULT) toggles a fixed-point square test tone in the audio callback.
 * USB: MIDI only; product string "BeatBax FM-1".
 */
#include <stdint.h>

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

extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _rt_start[], _rt_end[], _rt_load[];
extern void isr_timer5(void);
extern void isr_alnk0(void);

/* PLAY = matrix button 12 (Felucca PANEL_DEFAULT). */
enum { BB_BTN_PLAY = 12 };

#define FRAMES_PER_HALF 128u
#define SAMPLES_PER_HALF (FRAMES_PER_HALF * 2u) /* L,R int32 */
#define AUDIO_HALVES 2u

/* 440 Hz at 44117.6 Hz (24e6/544): phase_inc = freq * 544 * 2^32 / 24e6 */
#define TONE_PHASE_INC 42852255u
#define TONE_AMP_Q23 (1 << 20) /* modest level, left-justified Q23 */

static volatile uint32_t ms;
static volatile uint32_t tone_on;
static volatile uint32_t tone_phase;
static int32_t audio_buf[SAMPLES_PER_HALF * AUDIO_HALVES]
    __attribute__((aligned(8)));

static uint32_t boots_ok;
static uint32_t boot_stage;                 /* init stage reached this boot (for the fault report) */
static struct {
    uint32_t magic, pending, failed;
} guard __attribute__((section(".noinit")));
static uint32_t fault_stage __attribute__((section(".noinit"))); /* stage reached when the last crash fired */

/* ---- audio callback: SRAM fill only; no flash, draw, or USB wait ---- */

static RAMFN void tone_fill(int32_t *dst, uint32_t frames)
{
    uint32_t i, on = tone_on, ph = tone_phase, inc = TONE_PHASE_INC;
    int32_t amp = (int32_t)TONE_AMP_Q23;
    for (i = 0; i < frames; i++) {
        int32_t s = 0;
        if (on)
            s = (ph & 0x80000000u) ? amp : -amp;
        dst[i * 2u] = s;
        dst[i * 2u + 1u] = s;
        ph += inc;
    }
    tone_phase = ph;
}

void fm1_alnk0_irq(void)
{
    uint8_t p = fm1_audio_pending();
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half();
        FL_FAR(tone_fill)(audio_buf + half * SAMPLES_PER_HALF, FRAMES_PER_HALF);
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

/* Tiny 3x5 glyphs for the title strip (bits bottom bit = top row). */
static const uint8_t GLYPH[14][3] = {
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
    {0x1F, 0x15, 0x0A}, /* B */
    {0x1F, 0x15, 0x11}, /* E */
    {0x1E, 0x05, 0x1E}, /* A */
    {0x01, 0x1F, 0x01}, /* T */
};

static void lcd_glyph(uint32_t x, uint32_t y, uint32_t gi, uint16_t fg, uint16_t bg)
{
    uint32_t col, row;
    if (gi >= 14u)
        return;
    for (col = 0; col < 3u; col++) {
        uint8_t bits = GLYPH[gi][col];
        for (row = 0; row < 5u; row++)
            lcd_rect(x + col * 2u, y + row * 2u, 2, 2, (bits & (1u << row)) ? fg : bg);
    }
}

static void draw_title(void)
{
    /* "BEAT" "001" — BeatBax bring-up banner */
    static const uint8_t beat[] = {10, 11, 12, 13}; /* B E A T */
    static const uint8_t ver[] = {0, 0, 1};
    uint32_t i;
    lcd_rect(0, 0, 240, 18, BLU);
    for (i = 0; i < 4u; i++)
        lcd_glyph(8u + i * 10u, 4, beat[i], WHT, BLU);
    for (i = 0; i < 3u; i++)
        lcd_glyph(160u + i * 10u, 4, ver[i], YEL, BLU);
}

#define BTN_X(i) (4u + ((i) % 7u) * 33u)
#define BTN_Y(i) (100u + ((i) / 7u) * 26u)
#define KEY_X(i) (4u + ((i) % 9u) * 26u)
#define KEY_Y(i) (156u + ((i) / 9u) * 22u)
#define ENC_X(i) (4u + (i) * 33u)

static void draw_static(void)
{
    uint32_t i;
    lcd_rect(0, 0, 240, 240, BLK);
    draw_title();
    /* colour bars under the title */
    {
        static const uint16_t bars[6] = {RED, GRN, BLU, WHT, YEL, CYN};
        for (i = 0; i < 6; i++)
            lcd_rect(i * 40u, 20, 40, 8, bars[i]);
    }
    for (i = 0; i < 14; i++)
        lcd_rect(BTN_X(i), BTN_Y(i), 30, 22, i == BB_BTN_PLAY ? MAG : DIM);
    for (i = 0; i < 27; i++)
        lcd_rect(KEY_X(i), KEY_Y(i), 24, 20, DIM);
    for (i = 0; i < 7; i++)
        lcd_rect(ENC_X(i), 68, 30, 22, DIM);
}

static void draw_bar(uint32_t y, int32_t v, uint16_t col, uint32_t *shown)
{
    uint32_t w = v < 0 ? 0u : (uint32_t)v * 232u / 1023u;
    if (w == *shown)
        return;
    *shown = w;
    if (w)
        lcd_rect(4, y, w, 10, col);
    if (w < 232u)
        lcd_rect(4u + w, y, 232u - w, 10, DIM);
}

/* Boot-stage indicator (top-right corner): one filled segment per init stage
 * that completed, so a hardware test can attribute a "…then UBOOT" to one
 * stage. 0 screen, 1 flash probe, 2 USB, 3 guards+IRQs, 4 audio. */
static void mark_stage(uint32_t s)
{
    uint32_t i;
    boot_stage = s;
    for (i = 0; i < 5u; i++)
        lcd_rect(200u + i * 8u, 6, 6, 8, i == s ? YEL : i < s ? GRN : DIM);
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
    fault_stage = boot_stage;               /* survives the reset for the next boot's report */
    lcd_rect(0, 0, 240, 240, RED);
    if (fault_stage < 5u)   /* a white segment marks the stage reached before the fault */
        lcd_rect(200u + fault_stage * 8u, 6, 6, 8, WHT);
    for (;;)                /* park: keep the crash on screen for diagnosis */
        fm1_wdt_feed();
}

static int32_t enc_pos[FM1_NENC];

static void app_main(void)
{
    uint32_t i, last_bar = 0, shown_btn = ~0u, shown_keys = ~0u;
    int32_t knob = 0;
    uint32_t knob_w = ~0u, tone_shown = 0;

    for (i = 0; i < SAMPLES_PER_HALF * AUDIO_HALVES; i++)
        audio_buf[i] = 0;

    /* Boot order contract (see docs/hardware/status-001.md): paint the screen
     * first for visibility, then the deterministic rescue, then each risky
     * subsystem, marking each stage so a hardware test attributes a failure.
     * guard.pending stays armed until the main loop has proven USB + the tick
     * are alive (below), so a fault or hang here lands in recoverable ROM UBOOT
     * after two boots — never a USB-less silent crash loop. */
    lcd_init();
    draw_static();
    fm1_lcd_backlight(1);
    mark_stage(0);
    if (fm1_crash.magic == FM1_CRASH_MAGIC) {
        lcd_rect(200, 228, 24, 10, YEL);
        if (fault_stage < 5u)               /* the stage the last crash reached, in red */
            lcd_rect(200u + fault_stage * 8u, 6, 6, 8, RED);
    }

    /* Deterministic rescue: hold PLAY (matrix button 12) at power-on to enter
     * ROM download mode (WL82 UBOOT1.00), independent of USB/OTA and of the
     * two-boot counter. Synchronous scan, before any timer ISR exists. */
    fm1_input_init();
    for (i = 0; i < 4u; i++)
        fm1_input_scan();
    if (fm1_in.buttons & (1u << BB_BTN_PLAY))
        fm1_enter_uboot();

    /* Flash probe + interrupted-update cleanup. Guards are still off and the
     * boot-fail guard is still armed: if the newer UC25HQ80 part misbehaves
     * here, two boots -> UBOOT. */
    {
        uint32_t f = irq_save();
        uint32_t id = FL_FAR(fl_jedec_ram)();
        /* 0x856014 original FM-1 flash; 0xB36014 newer UC25HQ80 units */
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
    /* Attach ALNK0 BEFORE fm1_guard_lock_top(): fm1_irq_attach writes the
     * vector table at 0x01C7FE00, which lock_top then write-protects.
     * Attaching after it trips the write guard -> CPU exception -> UBOOT
     * (the boot failure the canary isolated). */
    fm1_audio_init(audio_buf, SAMPLES_PER_HALF, isr_alnk0, 3);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    mark_stage(3);
    mark_stage(4);

    for (;;) {
        uint32_t now = ms, pressed, released = 0;
        fm1_wdt_feed();
        usb_retry(now);
        /* Healthy point: once the loop has run ~2 s (USB polled, tick alive),
         * clear the boot-fail guard. Until then any fault or reset counts
         * toward the two-boot UBOOT fallback. */
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
            tone_shown = 0;
            shown_btn = ~0u;
            shown_keys = ~0u;
            knob_w = ~0u;
        }

        pressed = fm1_input_edges(&released);
        if (pressed & (1u << BB_BTN_PLAY))
            tone_on ^= 1u;

        if (now - last_bar >= 40u) {
            last_bar = now;
            knob = fm1_adc_read(FM1_ADC_MASTER);
            draw_bar(32, knob, WHT, &knob_w);
            /* tone indicator strip */
            if (tone_on != tone_shown) {
                tone_shown = tone_on;
                lcd_rect(4, 46, 80, 10, tone_on ? YEL : DIM);
            }
        }

        for (i = 0; i < FM1_NENC; i++) {
            int32_t s = fm1_enc_take(i);
            if (s) {
                enc_pos[i] += s;
                lcd_rect(ENC_X(i), 68, 30, 22, s > 0 ? GRN : RED);
                lcd_rect(ENC_X(i) + 2u, 86,
                         (uint32_t)(enc_pos[i] & 15) * 28u / 15u + 1u, 2, WHT);
            }
        }
        if (shown_btn != fm1_in.buttons) {
            uint32_t d = shown_btn ^ fm1_in.buttons;
            for (i = 0; i < 14; i++)
                if (d & (1u << i)) {
                    uint16_t on = (fm1_in.buttons >> i) & 1u ? WHT : (i == BB_BTN_PLAY ? MAG : DIM);
                    lcd_rect(BTN_X(i), BTN_Y(i), 30, 22, on);
                }
            shown_btn = fm1_in.buttons;
        }
        if (shown_keys != fm1_in.notes) {
            uint32_t d = shown_keys ^ fm1_in.notes;
            for (i = 0; i < 27; i++)
                if (d & (1u << i))
                    lcd_rect(KEY_X(i), KEY_Y(i), 24, 20,
                             (fm1_in.notes >> i) & 1u ? CYN : DIM);
            shown_keys = fm1_in.notes;
        }
        for (i = 0; i < 14; i++)
            fm1_led_key(i, (fm1_in.buttons >> i) & 1u);
        for (i = 0; i < 27; i++)
            fm1_led_key(14u + i, (fm1_in.notes >> i) & 1u);

        lcd_rect(232, 232, 8, 8,
                 usb.config ? ((now >> 9) & 1u ? GRN : DIM) : RED);
        (void)released;
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d;
    fm1_time_init();
    fm1_wdt_arm(0x0F); /* ~32 s during bring-up; fed in the main loop */
    /* "BB12" — bumped from "BB11" so a stale failed-boot counter left in SRAM by
     * an earlier build cannot force this build straight into UBOOT. Bump the
     * magic again whenever the firmware's boot path changes. */
    if (guard.magic != 0x42423132u) {
        guard.magic = 0x42423132u;
        guard.failed = 0;
        guard.pending = 0;
    }
    if (guard.pending)
        guard.failed++;
    guard.pending = 1;
    /* Two failed boots before the main loop's healthy point (~2 s in) → UBOOT,
     * so a broken build always lands in recoverable ROM download mode. */
    if (guard.failed >= 2u) {
        guard.failed = 0;
        guard.pending = 0;
        /* No auto-UBOOT: it fires falsely on this unit. Recovery stays
         * available via PLAY-hold at power-on or the Flipper USB key. */
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
