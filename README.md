# BeatBax FM-1

**BeatBax FM-1** is custom firmware that turns the [M-VAVE FM-1](https://www.m-vave.com/product?id=fm-1) into a graphical chiptune composer for [BeatBax](https://github.com/kadraman/beatbax) songs.

Songs stay as `.bax` text. Compose on the FM-1’s screens and knobs, then open the same file in BeatBax Desktop or play it with the BeatBax CLI. The first chip is Game Boy (DMG-01). Later chips follow BeatBax’s plugin order.

This repository currently ships **specifications and process** only. Firmware and host tooling land behind the feature folders under [`specs/features/`](specs/features/).

## Why a separate repo

BeatBax is a live-coding language and desktop toolchain. The FM-1 has a 240×240 panel that is too small for source editing, so this project is a fully graphical multi-screen UI that still speaks the same language and file model.

## Compatibility

| Rule | Meaning |
|------|---------|
| `.bax` is the song | Device edits a structured model and writes canonical `.bax` |
| Round-trip | A song saved on the device MUST `beatbax verify`, play via CLI, and open in Desktop |
| Fail loudly | Unsupported constructs are refused with a named reason, never silently simplified |
| No new syntax | Specs quote BeatBax’s Game Boy surface; they do not invent language features |

## Hardware platform

- **Device:** M-VAVE FM-1 (JieLi AC791N / WL82)
- **Display:** 240×240 SPI LCD
- **Audio:** Codec at 44117.6 Hz
- **Controls:** 27 note keys, labeled buttons, four encoders (K1–K4), MASTER pot
- **Base HAL:** [Felucca](https://github.com/hugelton/Felucca) (pinned commit; see feature 001)

Related community firmwares (patterns only, not song formats): [Sidou](https://github.com/dsablic/sidou), [Chipbox](https://github.com/pbarilla/chipbox), [OPN-1](https://github.com/putchdev/opn-1), [fm1-nes](https://github.com/Keitark/fm1-nes). Stock open firmware: [fm1_open](https://github.com/m-vave/fm1_open) (binary SDK — not our app base).

## Project layout

```
beatbax-fm1/
├── specs/            # Spec-driven development (constitution, global, features)
├── .github/          # Issue and pull request templates
├── AGENTS.md         # Instructions for coding agents
├── CONTRIBUTING.md
├── ROADMAP.md
└── LICENSING.md
```

Planned implementation layout (after feature 001):

```
firmware/             # Device application (GPL when linked with Felucca)
host/                 # Portable song model and tools (Apache-2.0)
docs/                 # User and developer notes
songs/                # Fixture .bax songs
```

## Spec-driven development

Engineering behavior is specified under [`specs/`](specs/). Process matches BeatBax:

1. Open a feature or spec issue
2. Create `specs/features/NNN-slug/` with `spec.md` → `plan.md` → `tasks.md`
3. Implement from `tasks.md` with tests
4. Move the folder to `specs/complete/` when shipped

Start with [specs/README.md](specs/README.md) and [specs/constitution.md](specs/constitution.md). Agents: [AGENTS.md](AGENTS.md).

## Phased delivery

| ID | Feature | Issue | Status |
|----|---------|-------|--------|
| 001 | Platform bring-up (Felucca, screen, controls, tone, USB) | [#1](https://github.com/kadraman/beatbax-fm1/issues/1) | draft |
| 002 | `.bax` round-trip on the host | [#2](https://github.com/kadraman/beatbax-fm1/issues/2) | draft |
| 003 | Game Boy renderer on the host | [#3](https://github.com/kadraman/beatbax-fm1/issues/3) | draft |
| 004 | Device player | [#4](https://github.com/kadraman/beatbax-fm1/issues/4) | draft |
| 005 | Graphical composer screens | [#5](https://github.com/kadraman/beatbax-fm1/issues/5) | draft |
| 006 | Store and USB transfer | [#6](https://github.com/kadraman/beatbax-fm1/issues/6) | draft |
| 007 | Live play and record | [#7](https://github.com/kadraman/beatbax-fm1/issues/7) | draft |

See [ROADMAP.md](ROADMAP.md) and [specs/STATUS.md](specs/STATUS.md).

## Disclaimer

Installing custom firmware on an M-VAVE FM-1 is at your own risk. It may void the warranty. An interrupted install can leave the device unusable until recovery (hold **OCT-** at power-on). This project is independent and is not affiliated with M-VAVE, Nintendo, or JieLi.

## License

Host song code and project-authored specs/docs: **Apache-2.0** (see [LICENSE](LICENSE)).  
Firmware linked with Felucca: **GPL-3.0-or-later**. Details: [LICENSING.md](LICENSING.md).
