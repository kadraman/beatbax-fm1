# Tasks: Live play and record

**Input**: [spec.md](spec.md), [plan.md](plan.md)

## Phase 0: Foundation (blocks stories)

- [ ] T001 Confirm features 005 and 006 are at least `planned`
- [ ] T002 Write `docs/midi.md` with channel map and note ranges
- [ ] T003 Scaffold `firmware/app/live/` event queue API safe for render tick consumption

**Checkpoint**: queue API reviewed against audio rules.

## Phase 1: User Story 1 — Live play (P1)

- [ ] T010 [US1] Wire note keys → live audition of selected instrument
- [ ] T011 [US1] Wire USB MIDI note on/off into the same live path
- [ ] T012 [US1] Document and implement voice allocation / steal rules
- [ ] T013 [US1] Respect OCT-/+ for live pitch
- [ ] T014 [US1] Ensure no USB wait inside audio callback (code review + checklist)

**Checkpoint**: keys and MIDI play over/with song safely.

## Phase 2: User Story 2 — Record (P1)

- [ ] T020 [US2] Implement REC arm/disarm UI feedback
- [ ] T021 [US2] Realtime record into current pattern while PLAY runs
- [ ] T022 [US2] Step-record when stopped
- [ ] T023 [US2] Write pattern cells expressible in `.bax` via song model
- [ ] T024 [US2] SAVE + host `beatbax verify` round-trip test for a recorded fixture
- [ ] T025 [US2] Complete `docs/hardware/checklist-007.md` including soak

## Polish

- [ ] T900 Update README feature list for live/record
- [ ] T901 Move this folder to `specs/complete/007-live-play-record/` and update `specs/STATUS.md` when shipped
