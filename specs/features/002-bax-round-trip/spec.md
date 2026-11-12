---
title: ".bax round-trip on the host"
id: 2
slug: "bax-round-trip"
status: draft
authors: ["kadraman"]
created: 2026-11-11
updated: 2026-11-11
issue: "https://github.com/kadraman/beatbax-fm1/issues/2"
area: song-model
---

# Feature Specification: `.bax` round-trip on the host

## Summary

Ship a portable Apache-2.0 host library that parses the BeatBax Game Boy `.bax` subset into a structured song model and writes canonical `.bax` back. Fixtures MUST pass `beatbax verify`. Unsupported constructs are refused with a named reason.

## Problem

The FM-1 UI will edit a structured model, not source text. Without a host round-trip library, firmware cannot safely load or save songs that Desktop and the CLI already understand.

## User scenarios

### User Story 1 — Parse and write supported songs (Priority: P1)

**Why this priority**: Blocks device player and composer persistence.

**Independent test**: Load fixture `.bax` files; write them back; `beatbax verify` both original and written forms.

**Acceptance scenarios**:

1. **Given** a supported Game Boy fixture under `songs/`, **When** the host library parses it, **Then** the model exposes instruments, patterns, sequences, channels, and tempo/chip metadata.
2. **Given** a parsed supported song, **When** the library writes canonical `.bax`, **Then** `beatbax verify` succeeds on the written file.
3. **Given** the same fixture parsed and written twice, **When** compared, **Then** the second write is byte-identical to the first (canonical form is stable).

### User Story 2 — Refuse unsupported input (Priority: P1)

**Why this priority**: Compatibility rule forbids silent simplification.

**Independent test**: Feed unsupported `.bax` samples; assert named errors.

**Acceptance scenarios**:

1. **Given** a `.bax` with an unsupported chip directive, **When** parsed, **Then** the library fails with a reason naming the chip.
2. **Given** a `.bax` using a construct outside the Game Boy subset this phase supports, **When** parsed, **Then** the library fails naming that construct.
3. **Given** a truncated or empty file, **When** parsed, **Then** the library fails with a clear error (no crash).

### Edge cases

- Remote imports / instrument imports present in BeatBax but not supported on device — refuse by name.
- Comments and insignificant whitespace — canonical write may normalize; verify must still pass.

## Requirements

### Functional requirements

- **FR-001**: Library MUST support `chip gameboy` songs for the BeatBax Game Boy surface quoted in [song-model.md](../../global/song-model.md).
- **FR-002**: Library MUST parse into a structured model suitable for later firmware linking (portable C or a host language with a C-ABI / portable subset — decided in plan without inventing syntax).
- **FR-003**: Library MUST write canonical `.bax` that `beatbax verify` accepts.
- **FR-004**: Library MUST refuse unsupported input with a named reason.
- **FR-005**: Library MUST NOT link Felucca; license Apache-2.0.
- **FR-006**: Document the minimum BeatBax CLI version used as the oracle.

### Non-goals

- Device UI or flash store (006).
- Audio rendering (003).
- New language syntax.
- Full BeatBax plugin ecosystem on device.

## Success criteria

- **SC-001**: At least three Game Boy fixtures round-trip through parse → write → `beatbax verify`.
- **SC-002**: Automated tests cover supported parse and unsupported refusals.
- **SC-003**: CI (or documented local script) runs the oracle against written fixtures.

## Assumptions

- BeatBax CLI is installable for developers (`npm install -g @beatbax/cli` or repo-local).
- Phase 001 may be in progress in parallel for docs, but 002 implementation does not require a flashed device.

## Open questions

- Host implementation language (C for easy firmware link vs TypeScript calling into a shared IR). Plan must pick one without changing `.bax` meaning.
- Exact supported effect/macro list — freeze against a BeatBax version in plan/research.

## References

- [specs/global/song-model.md](../../global/song-model.md)
- [specs/global/compatibility.md](../../global/compatibility.md)
- https://beatbax.com/docs
- https://github.com/kadraman/beatbax
