---
title: "Store and USB transfer"
id: 6
slug: "store-usb-transfer"
status: draft
authors: ["kadraman"]
created: 2026-11-11
updated: 2026-11-11
issue: "https://github.com/kadraman/beatbax-fm1/issues/6"
area: firmware
---

# Feature Specification: Store and USB transfer

## Summary

Persist songs as `.bax` in free flash (erase only while silent) and provide a host tool to read/write songs over USB. Files leaving the device MUST open in BeatBax Desktop and play via the CLI. Power loss MUST NOT corrupt the last good song.

## Problem

Composer edits are only useful if they survive power cycles and can move to the desktop toolchain without a proprietary project format.

## User scenarios

### User Story 1 — Save and load on device (Priority: P1)

**Why this priority**: Core durability.

**Independent test**: Edit, SAVE, power cycle, load, PLAY.

**Acceptance scenarios**:

1. **Given** an in-RAM song from the composer, **When** the user SAVEs under a name, **Then** the song is stored as `.bax` in the device store.
2. **Given** a stored song, **When** the user loads it, **Then** the model matches and PLAY works.
3. **Given** playback is active, **When** the user tries to SAVE, **Then** the device either waits until silent or refuses with a message — it MUST NOT erase flash during audio callback or while sound is actively rendering unsafely (per [audio.md](../../global/audio.md): erase only when silent).

### User Story 2 — USB transfer with host tool (Priority: P1)

**Why this priority**: Desktop/CLI compatibility loop.

**Independent test**: Pull `.bax` to PC; `beatbax verify` and play; push a Desktop-authored `.bax` back.

**Acceptance scenarios**:

1. **Given** a song on device, **When** the host tool downloads it, **Then** the file `beatbax verify` succeeds and plays via CLI.
2. **Given** a supported `.bax` on the host, **When** the host tool uploads it, **Then** the device can load and play it.
3. **Given** an unsupported `.bax`, **When** uploaded or loaded, **Then** the device/tool refuses with a named reason.

### User Story 3 — Power-loss safety (Priority: P1)

**Why this priority**: Data integrity.

**Independent test**: Interrupt power during save (or simulate); previous good song still loads.

**Acceptance scenarios**:

1. **Given** a successful prior save, **When** a subsequent save is interrupted, **Then** the last good song remains loadable.
2. **Given** store metadata, **When** CRC/checks fail, **Then** the UI reports corruption rather than loading garbage.

### Edge cases

- Store full — refuse with clear message.
- Name collisions — replace only after confirm, or version — decide in plan.
- Same file already present — leave alone or confirm replace (prefer Sidou-like confirm when different).

## Requirements

### Functional requirements

- **FR-001**: Durable format MUST be `.bax` text (canonical writer from 002).
- **FR-002**: Flash erase/program MUST occur only when transport is silent and outside the audio callback.
- **FR-003**: Host tool MUST list, get, and put songs over USB (protocol documented).
- **FR-004**: Downloaded songs MUST pass `beatbax verify` and open in Desktop for the supported subset.
- **FR-005**: Power-loss strategy MUST preserve the last good song (e.g. double-buffer / commit marker).
- **FR-006**: SAVE on device MUST be fully wired (no stub).
- **FR-007**: Document flash layout against pinned Felucca without publishing stock dumps.

### Non-goals

- Migrating Sidou/Chipbox/OPN-1/stock project formats.
- Cloud sync; Bluetooth.
- On-device UGE/WAV/MIDI export.

## Success criteria

- **SC-001**: Round-trip device → host → `beatbax verify` → Desktop open on at least three songs.
- **SC-002**: Host → device → PLAY works for those songs.
- **SC-003**: Power-loss test documented with pass result before `complete`.

## Assumptions

- Feature 005 composer exists to create content; 002 writer exists.
- USB MIDI SysEx or vendor protocol via Felucca USB is available — plan picks one.

## Open questions

- Exact free flash region size and addressing.
- SysEx vs bulk USB transport.

## References

- [specs/global/compatibility.md](../../global/compatibility.md)
- [specs/global/hardware.md](../../global/hardware.md)
- Sidou store patterns (behavioral reference only)
