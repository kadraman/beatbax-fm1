---
title: "Graphical composer"
id: 5
slug: "graphical-composer"
status: draft
authors: ["kadraman"]
created: 2026-11-11
updated: 2026-11-11
issue: "https://github.com/kadraman/beatbax-fm1/issues/5"
area: ui
---

# Feature Specification: Graphical composer

## Summary

Add fully graphical Song, Pattern, Chain, Instrument, and Mix screens on the FM-1 so users can create and edit Game Boy songs without text editing. Map every physical control per [ui.md](../../global/ui.md). Edits update the in-RAM song model from feature 002; persistence is labeled (SAVE) but storage semantics are owned by feature 006.

## Problem

The 240×240 panel cannot host a comfortable `.bax` source editor. Users need multi-screen graphical editing with knobs and keys, following patterns proven by Sidou/OPN-1 while remaining BeatBax-compatible.

## User scenarios

### User Story 1 — Navigate composer screens (Priority: P1)

**Why this priority**: Without navigation, no editing workflow exists.

**Independent test**: SEQ cycles screens; HOME backs out; hold HOME shows help card.

**Acceptance scenarios**:

1. **Given** the composer firmware, **When** the user presses SEQ repeatedly, **Then** the UI cycles Song → Pattern → Chain → Instrument → Mix → (back per plan).
2. **Given** a nested or detail view, **When** the user presses HOME, **Then** the UI returns one level.
3. **Given** any composer screen, **When** the user holds HOME, **Then** a help/control card for that screen appears.

### User Story 2 — Edit song structure graphically (Priority: P1)

**Why this priority**: Core authoring loop.

**Independent test**: Create/edit pattern steps, chain order, instruments, mix; play to hear changes.

**Acceptance scenarios**:

1. **Given** Song screen, **When** the user adjusts tempo with a bound knob, **Then** the model BPM updates and PLAY uses the new tempo.
2. **Given** Pattern screen on a channel, **When** the user enters notes with note keys, **Then** step cells show notes (graphical cells, not source glyphs) and playback reflects them.
3. **Given** Chain screen, **When** the user arranges sequence tiles, **Then** channel sequences match the model and `.bax` sequence semantics.
4. **Given** Instrument screen, **When** the user edits duty, envelope graph, wave drawing, or noise params with K1–K4 / keys as labeled, **Then** the instrument in the model updates and audition reflects it.
5. **Given** Mix screen, **When** the user changes channel levels or mute/solo, **Then** playback follows.

### User Story 3 — Full control coverage (Priority: P1)

**Why this priority**: Constitution forbids dead buttons.

**Independent test**: Checklist of every Felucca control on each screen.

**Acceptance scenarios**:

1. **Given** each composer screen, **When** K1–K4 turn, **Then** four on-screen labels match the parameters being edited.
2. **Given** PRESETS, SELECT, ALGORITHM, ENV, LFO, FX, ARP, SCL, GLO, **When** pressed, **Then** they jump to the Game Boy parameter those names map to, or show an on-screen unused response.
3. **Given** OCT-/+, EDIT, PLAY, REC, SAVE, MASTER, note keys, **When** used, **Then** behavior matches [ui.md](../../global/ui.md) (REC/SAVE may be stubbed to “coming in 006/007” messages only if explicitly labeled on-screen — prefer wiring labels: SAVE may no-op with “needs store” toast until 006; REC until 007).

### Edge cases

- Pattern longer than visible steps — scroll/page with documented controls.
- Empty song — valid defaults that still write valid `.bax` once 006 lands.
- Attempt to edit unsupported construct — refuse with on-screen reason.

## Requirements

### Functional requirements

- **FR-001**: UI MUST be fully graphical; MUST NOT provide a `.bax` source text editor.
- **FR-002**: MUST implement Song, Pattern, Chain, Instrument, Mix screens as specified.
- **FR-003**: MUST bind K1–K4 to four labeled parameters on every page.
- **FR-004**: MUST define behavior for every Felucca-reported control on each screen (job or unused response).
- **FR-005**: Edits MUST mutate the shared song model (feature 002).
- **FR-006**: PLAY MUST audition the current model using the device player/renderer.
- **FR-007**: Draw dirty regions; avoid full-screen redraw every frame.
- **FR-008**: Note keys audition; on Pattern they enter notes. OCT-/+ set octave. EDIT is modifier.

### Non-goals

- Flash persistence and USB transfer (006).
- Full live record semantics (007) — REC may show armed state stub if needed.
- Multi-chip songs; Copilot; Bluetooth.

## Success criteria

- **SC-001**: A user can build a short four-channel Game Boy loop entirely on device (in RAM) and hear it.
- **SC-002**: Control coverage matrix is complete in docs and verified on hardware.
- **SC-003**: No source-text editor path exists in the firmware UI.

## Assumptions

- Features 002–004 are available for model + render + play.
- SAVE/REC full behavior arrives in 006/007; 005 still labels controls.

## Open questions

- Exact SEQ cycle order and whether Song is home.
- Mapping table for PRESETS/ENV/LFO/FX/ARP to Game Boy macros — freeze in plan/research.

## References

- [specs/global/ui.md](../../global/ui.md)
- Sidou / OPN-1 product patterns (not code)
