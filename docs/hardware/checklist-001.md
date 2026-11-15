# Hardware checklist — feature 001 platform bring-up

Record results before marking `specs/complete/001-platform-bring-up/` **complete**. Host-only PRs do not need this sheet; flashing releases do.

| # | Check | Pass / Fail | Notes |
|---|--------|-------------|-------|
| 1 | `JIELI_TOOLCHAIN` set; `firmware/app/build.sh` produces `beatbax-fm1-001.bin` | Pass | WSL |
| 2 | Checkout SHA matches `firmware/FELUCCA_PIN` (`FELUCCA_BSP_COMMIT`) | Pass | `5942e68…` |
| 3 | Package + install succeeds (`FM-1_1001`) | Pass | M-Flasher |
| 4 | Boot paints diagnostic screen (`BEAT 001`, colour bars, grid, all 5 boot-stage segments lit) | Pass | |
| 5 | Each of 14 buttons lights its cell / LED | Pass | |
| 6 | Each of 27 note keys lights its cell / LED | Pass | |
| 7 | Each of 7 encoders flashes and moves its mini-bar | Pass | |
| 8 | MASTER pot moves the white bar | Pass | |
| 9 | PLAY toggles yellow strip + audible ~440 Hz tone | Pass | |
| 10 | Tone path: no audible glitches under normal USB + key activity (spot check) | Pass | |
| 11 | Optional: callback cost under half of a 256-frame block (ticks) | Pass | Recommended before later audio phases |
| 12 | Host enumerates USB MIDI product `BeatBax FM-1` (manufacturer `BeatBax`) | Pass | |
| 13 | REC held at reboot enters bootloader recovery (m-bootloader); PLAY-hold is the bring-up's own UBOOT escape | Pass | |
| 14 | Reinstall BeatBax bring-up **or** stock / known-good from recovery (M-Flasher) | Pass | |
| 15 | Operator read and followed backup warning in [`recovery.md`](recovery.md) | Pass | |

## Session log

- Date: 2026-11-15
- Operator: kadraman
- Device serial / notes: newer FM-1 unit (flash `0xB36014`, UC25HQ80)
- Toolchain path / version: `JIELI_TOOLCHAIN=~/.jieli/toolchain` (WSL, pi32v2 clang)
- Felucca pin / BSP SHA: `a4f007c…` / `5942e68…` (see `firmware/FELUCCA_PIN`)
- Image / package hash: `beatbax-fm1-001.fwsc` (identity `FM-1_1001`) — record SHA-256 from the build output

## Sign-off

- [x] All required rows passed (1–10, 12–15). Row 11 recommended before calling audio-safe for later phases.
- [x] Results linked from the feature issue or PR (issue #1).
