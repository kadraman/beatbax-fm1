# Hardware checklist — feature 004 device player

Record results before marking `specs/complete/004-device-player/` **complete**.
Host-only PRs do not need this sheet; flashing releases do.

| # | Check | Pass / Fail | Notes |
|---|--------|-------------|-------|
| 1 | `JIELI_TOOLCHAIN` set; `firmware/app/build.sh` produces `beatbax-fm1-004.bin` | PASS | builds clean (`FM-1_1001`) |
| 2 | Checkout SHA matches `firmware/FELUCCA_PIN` (`FELUCCA_BSP_COMMIT`) | PASS | `5942e68b…` = `FELUCCA_BSP_COMMIT` |
| 3 | `make embed` regenerates `firmware/app/gb_song_embed.h` from `grassland_dash.bax` (bit-identical rebuild) | PASS | verified (`bax_name = "Grassland Dash"`) |
| 4 | Package + install succeeds (`FM-1_1001`) | PASS | M-Flasher |
| 5 | Boot paints the player screen (`BEAT 004`, all 5 boot-stage segments lit) | PASS | boots to `BEAT 004` |
| 6 | PLAY starts the fixture from the beginning (audible, green status strip) | PASS | |
| 7 | PLAY again stops (silence, dim status strip, playhead resets) | PASS | |
| 8 | Song ends → transport returns to idle/stopped (no loop) | PASS | |
| 9 | MASTER pot changes output level (full travel silent→loud) | PASS | |
| 10 | Status screen shows song name / playing state / playhead | PASS | `GRASSLAND DASH` rendered on the strip (FR-006); green/dim strip + cyan playhead |
| 11 | **Callback budget:** `fm1_ticks` measurement under half a 256-frame block | PASS | SC-002 — on-device `MAX` = **1409 µs** = 48.6% of the 2901 µs deadline (emulator predicted 677 µs; real flash XIP latency accounts for the ~2×). Read via `GB_DEBUG_CAPTURE=1` build |
| 12 | **Callback audit:** no flash erase, no LCD draw, no USB wait in the callback | PASS | FR-002 / T021 — `alnk0 → player_fill → gb_stream_render_block → gb_note_step` call only audio-HAL + fixed-point renderer; no `fl_erase`/`fl_prog`, no `lcd_*`, no `usb_*`; `FL_FAR` is `fl_far` far-call; only XIP **reads** of the `const` blob |
| 13 | **SRAM placement:** renderer hot path in `.ram_text`; no SRAM→XIP far-call faults | PASS | T010 (`.ram_text` ~12.7 KB of the 24 KB budget, build-verified; no faults during playback) |
| 14 | **64-bit division:** local `__udivdi3` (`app/div64.c`, SRAM) resolves the link; verify callback budget (row 11) — restoring division is O(64), optimize via reciprocal multiply if needed | PASS | links; row 11 confirms budget (1409 µs), no reciprocal-multiply optimization needed |
| 15 | 10-minute soak: no watchdog reset, no sustained underrun/crackling | PASS | US2 |
| 16 | Host enumerates USB MIDI product `BeatBax FM-1` | PASS | `make check-usb` → 2 × `BeatBax FM-1 [SoftwareDevice]` (bidirectional MIDI, EP1 in+out via the Windows MIDI class driver) |
| 17 | REC held at reboot enters bootloader recovery; PLAY-hold enters UBOOT | PASS | |
| 18 | Reinstall BeatBax **or** stock / known-good from recovery (M-Flasher) | PASS | |
| 19 | Operator read and followed backup warning in [`recovery.md`](recovery.md) | PASS | |

## Session log

- Date: 2026-11-27
- Operator: kadraman
- Device serial / notes (flash `0x856014` or `0xB36014`): `0x856014` (flash JEDEC probe)
- Toolchain path / version: `/home/kadraman/.jieli/toolchain` (pi32v2 clang + ld + objcopy)
- Felucca pin / BSP SHA: `a4f007c9211ea5b37298c01ff6d73430fe2cb96a` → BSP `5942e68b89ab9ccd23f2c541d712ca38fe635f11`
- Image / package hash: `beatbax-fm1-004.fwsc` (identity `FM-1_1001`)

## Sign-off

- [x] All required rows passed.
- [x] Results linked from the feature issue or PR (issue #4).
