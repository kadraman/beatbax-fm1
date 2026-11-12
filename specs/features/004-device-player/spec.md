---
title: "Device player"
id: 4
slug: "device-player"
status: draft
authors: ["kadraman"]
created: 2026-11-11
updated: 2026-11-11
issue: "https://github.com/kadraman/beatbax-fm1/issues/4"
area: firmware
---

# Feature Specification: Device player

## Summary

Run the feature 003 Game Boy renderer on the FM-1 in SRAM: play one fixture song from flash with PLAY transport. No editor. Measure the audio callback against half of a 256-frame block. Main loop keeps the watchdog / platform poll fed.

## Problem

Host rendering does not prove the FM-1 audio budget, flash XIP constraints, or panel transport. A player-only milestone de-risks sound before the composer UI.

## User scenarios

### User Story 1 — Play fixture from flash (Priority: P1)

**Why this priority**: First musical use of the device.

**Independent test**: Install firmware with embedded or stored fixture; press PLAY; hear the song; press PLAY to stop.

**Acceptance scenarios**:

1. **Given** bring-up firmware base (001) and renderer (003), **When** the device player image is installed, **Then** a fixture song is available without the composer.
2. **Given** the player is idle, **When** the user presses PLAY, **Then** the song starts from the beginning (or documented loop behavior).
3. **Given** the song is playing, **When** the user presses PLAY again, **Then** playback stops.
4. **Given** playback, **When** MASTER is turned, **Then** output level changes.

### User Story 2 — Meet audio and main-loop budgets (Priority: P1)

**Why this priority**: Crackles or watchdog resets block all later work.

**Independent test**: On-device tick measurements logged or shown; hardware checklist.

**Acceptance scenarios**:

1. **Given** the fixture playing, **When** callback duration is measured, **Then** it stays under half of the 256-frame budget on maintainer hardware.
2. **Given** playback for a documented duration (e.g. 10 minutes), **When** observed, **Then** no watchdog reset and no sustained underrun/crackling as defined in the checklist.
3. **Given** the audio callback source, **When** reviewed, **Then** it performs no flash erase, LCD draw, or USB wait.

### Edge cases

- Fixture missing or corrupt — show error on screen, no crash loop.
- PLAY while already stopping — idempotent stop.

## Requirements

### Functional requirements

- **FR-001**: Device MUST link the same fixed-point renderer algorithms as feature 003 (shared sources or verified port).
- **FR-002**: Hot path MUST reside in SRAM per [audio.md](../../global/audio.md).
- **FR-003**: PLAY toggles transport; MASTER controls level.
- **FR-004**: At least one Game Boy fixture MUST play from flash (embedded for this phase is acceptable).
- **FR-005**: Main loop MUST call platform poll often enough for USB/watchdog.
- **FR-006**: Minimal on-screen transport status (playing/stopped, song name) — graphical, not a text editor.
- **FR-007**: No pattern/instrument editing in this feature.

### Non-goals

- Composer screens (005), USB song transfer (006), live record (007).
- Multi-song browser (can be stub single-song).

## Success criteria

- **SC-001**: Fixture is recognizably the same arrangement as host render of that `.bax`.
- **SC-002**: Callback budget measured and recorded in `docs/hardware/checklist-004.md`.
- **SC-003**: Hardware checklist signed off before status `complete`.

## Assumptions

- Features 001 and 003 are at least `planned`; implementation gated per specs/README.
- Felucca pin from 001 remains in force.

## Open questions

- Embed fixture in `app.bin` vs tiny flash partition — plan picks embed for 004 simplicity.
- Whether USB MIDI clock is ignored until 007.

## References

- [specs/global/audio.md](../../global/audio.md)
- [specs/global/hardware.md](../../global/hardware.md)
- Feature 003 renderer
