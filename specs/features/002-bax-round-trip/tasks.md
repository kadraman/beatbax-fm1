# Tasks: `.bax` round-trip on the host

**Input**: [spec.md](spec.md), [plan.md](plan.md)

## Phase 0: Foundation (blocks stories)

- [ ] T001 Create `host/song/README.md` stating Apache-2.0 and “no Felucca” rule
- [ ] T002 [P] Add `docs/song-model.md` listing the Game Boy subset and BeatBax CLI version pin
- [ ] T003 [P] Add at least three fixtures under `songs/gameboy/` (including a BeatBax tutorial-class song if license-compatible, or originals)
- [ ] T004 Scaffold `host/song/` parse/model/write API headers (or modules)

**Checkpoint**: fixtures exist and verify under stock BeatBax CLI before testing our writer.

## Phase 1: User Story 1 — Parse and write (P1)

- [ ] T010 [US1] Implement parser for supported Game Boy `.bax` into `host/song/` model
- [ ] T011 [US1] Implement canonical writer in `host/song/`
- [ ] T012 [US1] Add unit tests under `host/tests/` for round-trip idempotence
- [ ] T013 [US1] Add `tools/verify-bax` script that runs `beatbax verify` on written fixtures

**Checkpoint**: written fixtures pass `beatbax verify`.

## Phase 2: User Story 2 — Refuse unsupported (P1)

- [ ] T020 [US2] Add negative fixtures under `songs/unsupported/` (or `host/tests/data/`)
- [ ] T021 [US2] Return named errors for unsupported chip and constructs
- [ ] T022 [US2] Tests assert error strings name the construct

## Polish

- [ ] T900 Cross-link `docs/song-model.md` from root `README.md`
- [ ] T901 Move this folder to `specs/complete/002-bax-round-trip/` and update `specs/STATUS.md` when shipped
