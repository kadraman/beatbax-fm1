/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 BeatBax FM-1 contributors
 *
 * Feature 001 boot canary (stage 2): mirrors bringup.c's boot order exactly,
 * so one flash isolates the bringup's second fault. Distinct full-screen colour
 * per stage, 2 s each; a fault paints RED and parks (no UBOOT, no reboot).
 *
 * Differences from the stage-1 canary, all now exercised:
 *   - fm1_irq_init() runs BEFORE lcd_init() (bringup's cstart order)
 *   - fm1_input_init() + synchronous fm1_input_scan() (bringup's PLAY rescue)
 *   - fm1_input_tick() inside the TIMER5 ISR
 *   - a .ram_text tone fill inside the ALNK0 ISR
 *
 *   colour  stage
 *   GREEN   0  irq_init (before LCD) + LCD OK
 *   CYAN    1  input init + scan OK
 *   BLUE    2  flash probe + cleanup OK
 *   MAGENTA 3  usb + TIMER5 (with input_tick) + IRQs OK
 *   YELLOW  4  guards + audio (with .ram_text tone fill) + lock_top OK
 *   WHITE   5  main loop reached
 *
 * Read: "last colour before RED = the step that is OK; the step after it faults."
 * A blank screen (no GREEN) means irq_init() before lcd_init() is the fault.
 */
#include <stdint.h>

#define FELUCCA_CDC 0
#define FELUCCA_OTA 1
#define FELUCCA_ID "FM-1_1002"
#define FELUCCA_USB_MANUFACTURER "BeatBax"
#define FELUCCA_USB_PRODUCT "BeatBax Canary"
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

static volatile uint32_t ms;
static int32_t audio_buf[512] __attribute__((aligned(8)));  /* 2 x 256 samples */
static uint8_t flash_ok;

/* ---- LCD (same code as minboot.c) ---- */
#define C(x) ((uint16_t)((((x) >> 8) | ((x) << 8)) & 0xFFFFu))
enum { GRN = 0x07E0, CYN = 0x07FF, BLU = 0x001F, MAG = 0xF81F, YEL = 0xFFE0,
       WHT = 0xFFFF, RED = 0xF800 };
static uint16_t row_px[240];

static void delay_ms_fed(uint32_t ms)
{
    while (ms--) {
        fm1_wdt_feed();
        fm1_delay_ms(1);
    }
}

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

static void lcd_init(void)
{
    static const uint8_t colmod = 0x55, madctl = 0x00;
    fm1_lcd_hw_init(); /* backlight already on (active low) */
    delay_ms_fed(50);
    lcd_cmd(0x01, 0, 0);
    delay_ms_fed(150);
    lcd_cmd(0x11, 0, 0);
    delay_ms_fed(120);
    lcd_cmd(0x3A, &colmod, 1);
    lcd_cmd(0x36, &madctl, 1);
    lcd_cmd(0x21, 0, 0);
    lcd_cmd(0x13, 0, 0);
    lcd_cmd(0x29, 0, 0); /* DISPON — without this the panel stays black */
    fm1_lcd_baud(4);
}

static void lcd_fill(uint16_t rgb)
{
    uint8_t a[4];
    uint32_t i;
    for (i = 0; i < 240; i++)
        row_px[i] = C(rgb);
    a[0] = 0;
    a[1] = 0;
    a[2] = 0;
    a[3] = 239;
    lcd_cmd(0x2A, a, 4);
    lcd_cmd(0x2B, a, 4);
    lcd_cmd(0x2C, 0, 0);
    for (i = 0; i < 240; i++) {
        fm1_lcd_send_data(row_px, 480);
        fm1_lcd_wait();
        fm1_lcd_deselect();
        if ((i & 15u) == 0)
            fm1_wdt_feed();
    }
}

static void stage(uint16_t color)
{
    lcd_fill(color);
    delay_ms_fed(2000);
}

/* ---- .ram_text tone fill (mirrors bringup's RAM-resident callback) ----
 * noinline + called through FL_FAR: a direct XIP -> RAM call is beyond the
 * pi32 23-bit call range and fails to link (R_PI32V2_LONG_JUMP_23M2). */
static RAMFN void tone_fill(int32_t *dst, uint32_t frames)
{
    uint32_t i;
    for (i = 0; i < frames; i++) {
        dst[i * 2u] = 0;
        dst[i * 2u + 1u] = 0;
    }
}

/* ---- interrupts (mirror bringup: input_tick in TIMER5, tone in ALNK0) ---- */
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

void fm1_alnk0_irq(void)
{
    uint8_t p = fm1_audio_pending();
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half();
        FL_FAR(tone_fill)(audio_buf + half * 256u, 128);
        fm1_audio_ack_half();
    }
}

static void fm1_fault(const fm1_crash_t *c)
{
    (void)c;
    lcd_init();      /* best-effort: an early fault may have fired before lcd_init */
    lcd_fill(RED);
    for (;;)
        fm1_wdt_feed();
}

/* ---- OTA hooks (usb.c / ota.c); still flashable by the loader ---- */
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
    (void)step;
    (void)code;
}
static void ota_commit(const uint8_t *parm)
{
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);
}

void fm1_cstart(void)
{
    uint32_t *s, *d;
    uint32_t i;
    fm1_time_init();
    fm1_wdt_arm(0x0F); /* ~32 s; fed during each pause */
    /* No boot-fail -> UBOOT path: a broken stage must stay visible. */
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;

    fm1_irq_init();                          /* BEFORE lcd, as bringup does */
    lcd_init();
    stage(GRN);                              /* 0: irq_init + LCD OK */

    fm1_input_init();
    for (i = 0; i < 4u; i++)
        fm1_input_scan();
    stage(CYN);                              /* 1: input scan OK */

    {
        uint32_t f = irq_save();
        uint32_t id = FL_FAR(fl_jedec_ram)();
        /* 0x856014 original FM-1 flash; 0xB36014 newer UC25HQ80 units */
        flash_ok = (id == 0x856014u || id == 0xB36014u);
        irq_restore(f);
        if (flash_ok)
            ota_boot_cleanup();
    }
    stage(BLU);                              /* 2: flash probe + cleanup OK */

    fm1_adc_init();
    usb_start();
    fm1_timer5_start(isr_timer5, 4);
    fm1_irq_enable_all();
    stage(MAG);                              /* 3: usb + TIMER5(input_tick) OK */

    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    /* Attach ALNK0 BEFORE fm1_guard_lock_top() (it writes the vector table). */
    fm1_audio_init(audio_buf, 256, isr_alnk0, 3);
    fm1_guard_lock_top();
    stage(YEL);                              /* 4: guards + audio OK */

    stage(WHT);                              /* 5: main loop */
    for (;;)
        fm1_wdt_feed();
}
