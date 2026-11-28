/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 BeatBax FM-1 contributors
 *
 * Minimal <string.h> for the JieLi pi32 build. The toolchain ships no <string.h>;
 * the implementations live in felucca-bsp's libc.c (memset/memcpy/memcmp), which
 * player.c includes. Signatures match libc.c exactly (`unsigned` == size_t on this
 * 32-bit target). Host builds use the real <string.h> — this directory is not on
 * their include path.
 */
#ifndef BAX_FW_STRING_H
#define BAX_FW_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
int memcmp(const void *a, const void *b, unsigned n);

#ifdef __cplusplus
}
#endif

#endif /* BAX_FW_STRING_H */
