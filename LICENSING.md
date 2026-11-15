# Licensing

BeatBax FM-1 uses a split license so host tooling can stay Apache-2.0 while firmware that links the Felucca platform remains GPL.

## Project-authored material

| Tree | License | Notes |
|------|---------|-------|
| Specs, docs, issue/PR templates, `AGENTS.md`, `CONTRIBUTING.md`, `ROADMAP.md` | Apache-2.0 | See [LICENSE](LICENSE) |
| Host song model, host tools, fixtures (when added under `host/`, `songs/`) | Apache-2.0 | MUST NOT link Felucca |
| Firmware application that links Felucca (when added under `firmware/`) | GPL-3.0-or-later | Required by Felucca linking |

Root [LICENSE](LICENSE) is Apache-2.0 and applies to project-authored material that is not otherwise marked. Firmware sources that depend on Felucca will carry an SPDX GPL-3.0-or-later notice and a copy of the GPL when those files are added.

## Third-party dependencies

| Component | Role | License (upstream) | How we use it |
|-----------|------|--------------------|---------------|
| [Felucca](https://github.com/hugelton/Felucca) | Pin authority for the FM-1 HAL commit | GPL-3.0-only (upstream) | Exact SHA in `firmware/FELUCCA_PIN`; we do **not** link Felucca’s synth engines |
| [felucca-bsp](https://github.com/hugelton/felucca-bsp) | FM-1 panel, LCD, USB, audio HAL | MPL-2.0 | Git submodule at `firmware/third_party/felucca-bsp` (SHA Felucca pins as `firmware/bsp`); linked only from firmware |
| BeatBax CLI / language | Compatibility oracle for `.bax` | MIT (BeatBax) | External tool for verify/play; do not vendor the whole monorepo into firmware |
| Sidou / Chipbox / OPN-1 / fm1-nes / fm1_open | Reference patterns | Various (often GPL) | **Patterns and docs only** — do not copy their synth or song-format sources into this tree |
| Official FM-1 binary SDK (`fm1_open`) | Stock workstation | Binary SDK terms | **Not** used as the BeatBax synth engine |

Firmware application sources under `firmware/app/` carry SPDX `GPL-3.0-or-later`. A combined image that includes felucca-bsp is distributed as a GPL work with corresponding source.

## Rules for contributors and agents

1. Do not copy GPL synth engines from other FM-1 firmwares into Apache-licensed host trees.
2. Do not publish stock firmware images, flash dumps, or chip keys.
3. When adding a dependency, update this file and retain upstream notices.
4. A combined firmware binary that includes Felucca is a GPL work; distribute corresponding source with releases.

## Disclaimer

Trademark and copyright holders for M-VAVE, Nintendo, JieLi, and others retain their rights. This project is independent and unaffiliated.
