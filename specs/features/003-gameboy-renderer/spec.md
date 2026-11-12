---
title: "Game Boy renderer on the host"
id: 3
slug: "gameboy-renderer"
status: draft
authors: ["kadraman"]
created: 2026-11-11
updated: 2026-11-11
issue: "https://github.com/kadraman/beatbax-fm1/issues/3"
area: audio
---

# Feature Specification: Game Boy renderer on the host

## Summary

Implement a fixed-point Game Boy (DMG-01) renderer on the host that plays the supported song model: pulse 1, pulse 2, wave, noise, envelopes, duty, wave table, and effects BeatBax already implements for that chip. Compare musical events and rendered audio to the BeatBax CLI with documented numeric tolerances.

## Problem

Device playback (004) needs an algorithm that fits FM-1 audio rules. Building and proving it on the host first lets us compare against BeatBax without flashing.

## User scenarios

### User Story 1 — Render supported songs (Priority: P1)

**Why this priority**: Core sound for all later device features.

**Independent test**: Render fixtures to WAV (or raw PCM) at a documented rate; listen/automated checks.

**Acceptance scenarios**:

1. **Given** a parsed Game Boy fixture, **When** rendered, **Then** all four voices that the song uses produce output consistent with channel bindings.
2. **Given** instruments with duty/envelope/wave/noise parameters from the BeatBax Game Boy surface, **When** rendered, **Then** those parameters affect the output as specified (unit tests per voice).
3. **Given** supported effects in the subset, **When** rendered, **Then** effect behavior matches the frozen BeatBax surface list from feature 002/003 research.

### User Story 2 — Compare to BeatBax CLI (Priority: P1)

**Why this priority**: Compatibility without inventing chip behavior.

**Independent test**: Automated comparison harness with numeric thresholds.

**Acceptance scenarios**:

1. **Given** the same fixture, **When** BeatBax CLI exports/plays reference audio and the host renderer writes audio, **Then** comparison uses the method and tolerances defined in Success criteria (events and/or resampled WAV).
2. **Given** a fixture that fails tolerance, **When** the harness runs, **Then** it fails the test (no silent pass).

### Edge cases

- Device rate 44117.6 Hz vs CLI host rate — resample or compare events + resampled WAV as specified below.
- Songs that use only a subset of channels.
- Silence / rests / zero-length patterns.

## Requirements

### Functional requirements

- **FR-001**: Renderer MUST implement pulse1, pulse2, wave, and noise in fixed-point.
- **FR-002**: Renderer MUST consume the feature 002 song model (no parallel ad-hoc format).
- **FR-003**: Renderer MUST NOT use float/double in the shared algorithm destined for firmware.
- **FR-004**: Comparison harness MUST document numeric tolerances (not “sounds close”).
- **FR-005**: Comparison MUST account for 44117.6 Hz device rate via resampling and/or event-level comparison, stated explicitly in this feature’s plan and tests README.
- **FR-006**: Effects/macros in scope are only those BeatBax’s Game Boy surface already defines and that 002 can round-trip.

### Non-goals

- On-device integration (004).
- NES or other chips.
- Softening BeatBax validation.
- FPU-accelerated path.

## Success criteria

- **SC-001**: Unit tests cover each voice’s basic parameter matrix.
- **SC-002**: At least three fixtures pass the CLI comparison harness under documented tolerances.
- **SC-003**: Default tolerances (amend only via spec update):
  - Musical events: note-on times within **1 ms** of CLI-derived reference events (same score expansion).
  - Resampled WAV: after resampling both to a common rate (e.g. 44100 Hz), normalized correlation **≥ 0.95** on a trimmed non-silent region **or** RMS difference below a documented dB threshold chosen in `plan.md` and recorded in the harness README — pick one primary metric and stick to it.

## Assumptions

- Feature 002 song model API is available or developed in lockstep to `planned`/`in-progress`.
- BeatBax CLI can produce reference audio or MIDI/event dumps sufficient for comparison; if only realtime play exists, use WAV export from CLI.

## Open questions

- Exact CLI command for reference WAV (`beatbax export` or equivalent) — confirm against installed CLI in plan.
- Whether event extraction uses BeatBax ISM JSON export.

## References

- [specs/global/audio.md](../../global/audio.md)
- [specs/global/compatibility.md](../../global/compatibility.md)
- BeatBax Game Boy docs: https://beatbax.com/docs
