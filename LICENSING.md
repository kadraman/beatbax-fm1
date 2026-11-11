# Licensing

BeatBax FM-1 uses a split license so host tooling can stay Apache-2.0 while firmware that links the Felucca platform remains GPL.

## Project-authored material

| Tree | License | Notes |
|------|---------|-------|
| Specs, docs, issue/PR templates, `AGENTS.md`, `CONTRIBUTING.md`, `ROADMAP.md` | Apache-2.0 | See [LICENSE](LICENSE) |
| Host song model, host tools, fixtures (when added under `host/`, `songs/`) | Apache-2.0 | MUST NOT link Felucca |
| Firmware application that links Felucca (when added under `firmware/`) | GPL-3.0-or-later | Required by Felucca linking |

Root [LICENSE](LICENSE) is Apache-2.0 and applies to project-authored material that is not otherwise marked. Firmware sources that depend on Felucca will carry an SPDX GPL-3.0-or-later notice and a copy of the GPL when those files are added.

## Third-party dependencies (planned)

| Component | Role | License (upstream) | How we use it |
|-----------|------|--------------------|---------------|
| [Felucca](https://github.com/hugelton/Felucca) | FM-1 panel, LCD, USB, audio HAL | GPL (upstream terms) | Pin a commit; link only from firmware |
| BeatBax CLI / language | Compatibility oracle for `.bax` | MIT (BeatBax) | External tool for verify/play; do not vendor the whole monorepo into firmware |
| Sidou / Chipbox / OPN-1 / fm1-nes / fm1_open | Reference patterns | Various (often GPL) | **Patterns and docs only** — do not copy their synth or song-format sources into this tree |
| Official FM-1 binary SDK (`fm1_open`) | Stock workstation | Binary SDK terms | **Not** used as the BeatBax synth engine |

## Rules for contributors and agents

1. Do not copy GPL synth engines from other FM-1 firmwares into Apache-licensed host trees.
2. Do not publish stock firmware images, flash dumps, or chip keys.
3. When adding a dependency, update this file and retain upstream notices.
4. A combined firmware binary that includes Felucca is a GPL work; distribute corresponding source with releases.

## Disclaimer

Trademark and copyright holders for M-VAVE, Nintendo, JieLi, and others retain their rights. This project is independent and unaffiliated.
