/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 BeatBax FM-1 contributors
 *
 * Minimal boot probe: solid green LCD, then idle.
 * No USB/OTA/audio, no hardware PC/stack guards, never enters UBOOT —
 * so a crash stays a crash-loop instead of ROM download mode.
 */
#include <stdint.h>

#define FELUCCA_CDC 0
#define FELUCCA_OTA 0
#define FELUCCA_ID "FM-1_1000"

#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_lcd_hw.h"
#include "libc.c"

extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _rt_start[], _rt_end[], _rt_load[];

void fm1_timer5_irq(void) {}
void fm1_alnk0_irq(void) {}
static void fm1_fault(const fm1_crash_t *c)
{
    (void)c;
    fm1_reboot();
}

#define C(x) ((uint16_t)((((x) >> 8) | ((x) << 8)) & 0xFFFFu))
enum { GRN = 0x07E0 };
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

void fm1_cstart(void)
{
    uint32_t *s, *d;
    fm1_time_init();
    fm1_wdt_arm(0x0F); /* ~32 s — longest; fed in the idle loop */
    /* No boot-fail → UBOOT path. No fm1_guard_enable. */
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;
    lcd_init();
    lcd_fill(GRN);
    for (;;)
        fm1_wdt_feed();
}
