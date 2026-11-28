/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 BeatBax FM-1 contributors
 *
 * Unsigned 64-bit division for the fixed-point renderer. The pi32 toolchain
 * emits __udivdi3 for `uint64_t / uint64_t` but ships no runtime helper (the
 * AC79 SDK's copy lives in XIP, which SRAM code cannot far-call). This SRAM
 * helper uses restoring division; correct, and the budget/divisor audit is
 * tracked in docs/hardware/checklist-004.md (row 14).
 */
#include <stdint.h>
#include "fm1_flash.h" /* RAMFN */

RAMFN uint64_t __udivdi3(uint64_t n, uint64_t d)
{
    if (d == 0) {
        return 0; /* renderer guards against zero divisors */
    }
    uint64_t q = 0;
    uint64_t r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> (unsigned)i) & 1u);
        if (r >= d) {
            r -= d;
            q |= (uint64_t)1 << (unsigned)i;
        }
    }
    return q;
}
