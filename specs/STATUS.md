# Spec status index

Next available feature number: **009**

## In flight (`specs/features/`)

| ID | Slug | Status | Area | Issue | Summary |
|----|------|--------|------|-------|---------|
| 005 | [graphical-composer](features/005-graphical-composer/) | draft | ui | [#5](https://github.com/kadraman/beatbax-fm1/issues/5) | Song / Pattern / Chain / Instrument / Mix screens |
| 006 | [store-usb-transfer](features/006-store-usb-transfer/) | draft | firmware | [#6](https://github.com/kadraman/beatbax-fm1/issues/6) | Flash store + host USB transfer of `.bax` |
| 007 | [live-play-record](features/007-live-play-record/) | draft | firmware | [#7](https://github.com/kadraman/beatbax-fm1/issues/7) | Keys, USB MIDI, REC into patterns |

## Complete (`specs/complete/`)

| ID | Slug | Status | Area | Issue | Summary |
|----|------|--------|------|-------|---------|
| 001 | [platform-bring-up](complete/001-platform-bring-up/) | complete | firmware | [#1](https://github.com/kadraman/beatbax-fm1/issues/1) | Pin Felucca; screen, controls, test tone, USB, recovery |
| 002 | [bax-round-trip](complete/002-bax-round-trip/) | complete | song-model | [#2](https://github.com/kadraman/beatbax-fm1/issues/2) | Host Game Boy `.bax` parse/write; CLI verify |
| 003 | [gameboy-renderer](complete/003-gameboy-renderer/) | complete | audio | [#3](https://github.com/kadraman/beatbax-fm1/issues/3) | Fixed-point Game Boy voices; CLI comparison |
| 004 | [device-player](complete/004-device-player/) | complete | firmware | [#4](https://github.com/kadraman/beatbax-fm1/issues/4) | Play fixture from flash on FM-1 |
| 008 | [effect-parity](complete/008-effect-parity/) | complete | audio | — | Complete vib/arp/port/bend parity; refuse unsupported effect tokens |

## Archive (`specs/archive/`)

_None yet._

## How to update

1. When creating a feature, add a row here and use the next `NNN`.
2. When status changes, update the row and the folder frontmatter.
3. On `complete`, move the folder to `complete/` and update this table.
4. On `superseded` / `wont-do`, move to `archive/` and note why.

opencode -s ses_e060b3ce3ffeA8dtWSl1SWX1UW
