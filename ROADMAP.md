# BeatBax FM-1 roadmap

Product direction for turning the M-VAVE FM-1 into a BeatBax-compatible graphical chiptune composer. This file is **not** an engineering spec. New work still needs a feature folder under [`specs/features/`](specs/features/).

## Principles

- `.bax` remains the song file BeatBax Desktop and CLI already understand.
- UI is fully graphical; the 240×240 panel does not host a source text editor.
- Game Boy first; later chips follow BeatBax’s plugin order and isolation rules.
- Host song code stays portable (Apache-2.0). Firmware may link Felucca (GPL).

## Delivery phases (Game Boy)

| ID | Feature | Issue | Goal |
|----|---------|-------|------|
| [001](specs/features/001-platform-bring-up/) | Platform bring-up | [#1](https://github.com/kadraman/beatbax-fm1/issues/1) | Pin Felucca; screen, controls, test tone, USB; recovery docs |
| [002](specs/features/002-bax-round-trip/) | `.bax` round-trip | [#2](https://github.com/kadraman/beatbax-fm1/issues/2) | Host song model for Game Boy subset; CLI verify fixtures |
| [003](specs/features/003-gameboy-renderer/) | Game Boy renderer | [#3](https://github.com/kadraman/beatbax-fm1/issues/3) | Fixed-point four-voice render; compare to BeatBax CLI |
| [004](specs/features/004-device-player/) | Device player | [#4](https://github.com/kadraman/beatbax-fm1/issues/4) | Same renderer on FM-1; play fixture from flash |
| [005](specs/features/005-graphical-composer/) | Graphical composer | [#5](https://github.com/kadraman/beatbax-fm1/issues/5) | Song / Pattern / Chain / Instrument / Mix screens |
| [006](specs/features/006-store-usb-transfer/) | Store and USB | [#6](https://github.com/kadraman/beatbax-fm1/issues/6) | Save/load `.bax` in flash; host transfer tool |
| [007](specs/features/007-live-play-record/) | Live play and record | [#7](https://github.com/kadraman/beatbax-fm1/issues/7) | Keys + USB MIDI; REC into patterns |

## Later chips (after 001–007)

Each chip is a new numbered feature spec. Suggested order (aligned with [BeatBax ROADMAP](https://github.com/kadraman/beatbax/blob/main/ROADMAP.md)):

1. NES / Famicom (Ricoh 2A03)
2. Sega Master System / Game Gear (SN76489)
3. ZX Spectrum 128 / Amstrad CPC (AY)

Chip voices MUST NOT change pattern or sequence meaning in the shared song model.

## Explicitly later / non-goals for 001–007

- On-panel text editing of `.bax` source
- Multi-chip songs on one project
- BeatBax Copilot on device
- On-device UGE / WAV / MIDI export (use BeatBax CLI)
- Bluetooth sync or Brothers Link
- Stock DX7 / official FM voice engine

## Status

Track engineering status in [specs/STATUS.md](specs/STATUS.md).
