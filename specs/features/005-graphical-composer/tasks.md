# Tasks: Graphical composer

**Input**: [spec.md](spec.md), [plan.md](plan.md)

## Phase 0: Foundation (blocks stories)

- [ ] T001 Confirm features 002–004 are at least `planned`
- [ ] T002 Create `docs/ui/controls-matrix.md` listing every control × screen (fill jobs during implementation)
- [ ] T003 Scaffold `firmware/app/ui/nav.c` screen stack (SEQ / HOME / hold-HOME help)

**Checkpoint**: navigation shell shows placeholder screens.

## Phase 1: User Story 1 — Navigate (P1)

- [ ] T010 [US1] Implement SEQ cycling among Song, Pattern, Chain, Instrument, Mix
- [ ] T011 [US1] Implement HOME back navigation
- [ ] T012 [US1] Implement hold-HOME help card per screen

**Checkpoint**: navigation and help work without editing.

## Phase 2: User Story 2 — Edit structure (P1)

- [ ] T020 [US2] Song screen: tempo, chip label, four lanes, playhead
- [ ] T021 [US2] Pattern screen: step cells, note entry from keys, channel select
- [ ] T022 [US2] Chain screen: sequence tiles editing channel sequences
- [ ] T023 [US2] Instrument screen: duty, envelope graph, wave draw, noise
- [ ] T024 [US2] Mix screen: levels / mute-solo per plan
- [ ] T025 [US2] Ensure PLAY uses live model via device renderer

**Checkpoint**: in-RAM loop authorable and audible.

## Phase 3: User Story 3 — Control coverage (P1)

- [ ] T030 [US3] Bind K1–K4 with on-screen labels on every page
- [ ] T031 [US3] Map PRESETS, SELECT, ALGORITHM, ENV, LFO, FX, ARP, SCL, GLO (or unused response)
- [ ] T032 [US3] Wire OCT±, EDIT, MASTER, note audition; SAVE/REC stubs only if on-screen message until 006/007
- [ ] T033 [US3] Complete and verify `docs/ui/controls-matrix.md` on hardware (`docs/hardware/checklist-005.md`)

## Polish

- [ ] T900 Update README with composer screen list
- [ ] T901 Move this folder to `specs/complete/005-graphical-composer/` and update `specs/STATUS.md` when shipped
