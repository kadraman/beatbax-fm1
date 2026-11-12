---
title: "Live play and record"
id: 7
slug: "live-play-record"
status: draft
authors: ["kadraman"]
created: 2026-11-11
updated: 2026-11-11
issue: "https://github.com/kadraman/beatbax-fm1/issues/7"
area: firmware
---

# Feature Specification: Live play and record

## Summary

Play the selected instrument from note keys and USB MIDI over the song, and use REC to write notes into the current pattern in time (or step-advance when stopped). Recorded notes MUST round-trip through `.bax` after save.

## Problem

A composer without live play and record is incomplete for performance and for capturing ideas on the panel. BeatBax Desktop users expect device-captured notes to reopen as normal `.bax`.

## User scenarios

### User Story 1 — Live play from keys and USB MIDI (Priority: P1)

**Why this priority**: Immediate musical feedback.

**Independent test**: Select instrument; play keys; send USB MIDI notes; hear voice over or instead of song per plan.

**Acceptance scenarios**:

1. **Given** a selected instrument, **When** the user presses note keys, **Then** the corresponding Game Boy voice auditions at the current octave.
2. **Given** USB MIDI input, **When** notes arrive on the documented channel, **Then** they play the selected instrument (or channel mapping documented in plan).
3. **Given** a song playing, **When** live notes are played, **Then** they layer according to documented voice-steal / free-voice rules without crashing audio.

### User Story 2 — Record into patterns (Priority: P1)

**Why this priority**: Capture performances into the song model.

**Independent test**: REC while playing; stop; inspect Pattern screen; SAVE; verify on host.

**Acceptance scenarios**:

1. **Given** Pattern screen (or record target channel), **When** REC is armed and transport runs, **Then** played notes are written onto steps in time.
2. **Given** transport stopped and REC armed, **When** the user plays notes, **Then** steps advance (step record) as documented.
3. **Given** recorded notes, **When** the song is saved as `.bax` (006), **Then** `beatbax verify` succeeds and Desktop/CLI play the recorded pitches/rhythms.

### Edge cases

- REC with no pattern selected — refuse with message.
- Overdub vs replace — plan picks one default; document.
- MIDI panic / all notes off.
- Holding notes across step boundaries.

## Requirements

### Functional requirements

- **FR-001**: Note keys and USB MIDI MUST play the selected instrument through the device renderer.
- **FR-002**: REC MUST arm record; PLAY runs transport; behavior differs for running vs stopped as specified.
- **FR-003**: Recorded events MUST land in the song model as normal pattern data expressible in `.bax`.
- **FR-004**: OCT-/+ MUST affect live/record octave.
- **FR-005**: Document MIDI channel map and CC subset (minimum: notes + optional program/volume if plan includes them).
- **FR-006**: Audio callback rules still apply — MIDI/key handling queues events for the render tick; no USB wait in callback.
- **FR-007**: After SAVE, round-trip through BeatBax CLI/Desktop MUST work for recorded content.

### Non-goals

- Full DAW automation / NRPN labyrinth (unless later feature).
- Bluetooth MIDI (later).
- Multi-chip live play.
- Quantization modes beyond a single documented grid.

## Success criteria

- **SC-001**: Live keys and USB MIDI play reliably for a 10-minute soak with song running.
- **SC-002**: A recorded pattern saves to `.bax` and passes `beatbax verify` + CLI play.
- **SC-003**: Hardware checklist 007 covers live, MIDI, realtime record, step record, and round-trip.

## Assumptions

- Features 005 and 006 provide composer UI and store.
- USB MIDI stack from Felucca/001 is available.

## Open questions

- Voice allocation when song uses all four Game Boy channels.
- Default overdub vs replace for REC.

## References

- [specs/global/ui.md](../../global/ui.md)
- [specs/global/compatibility.md](../../global/compatibility.md)
- [specs/global/audio.md](../../global/audio.md)
