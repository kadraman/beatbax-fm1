# Tasks: Game Boy renderer on the host

**Input**: [spec.md](spec.md), [plan.md](plan.md)

## Phase 0: Foundation (blocks stories)

- [ ] T001 Create `host/render/gameboy/` skeleton with fixed-point types and block-render API
- [ ] T002 [P] Write `docs/renderer.md` with rate, tolerance, and CLI reference commands
- [ ] T003 Wire renderer input to `host/song/` model from feature 002

**Checkpoint**: empty renderer links and renders silence deterministically.

## Phase 1: User Story 1 — Render supported songs (P1)

- [ ] T010 [US1] Implement pulse1/pulse2 cores (duty, envelope) in `host/render/gameboy/`
- [ ] T011 [US1] Implement wave channel (wave table + volume) in `host/render/gameboy/`
- [ ] T012 [US1] Implement noise channel in `host/render/gameboy/`
- [ ] T013 [US1] Implement scheduler/expansion tick consuming song model
- [ ] T014 [US1] Implement supported Game Boy effects from the frozen surface list
- [ ] T015 [US1] Unit tests under `host/tests/render/` per voice

**Checkpoint**: fixtures produce non-silent WAV with all used channels.

## Phase 2: User Story 2 — Compare to BeatBax CLI (P1)

- [ ] T020 [US2] Implement `tools/compare-render` harness (events + resampled WAV metric)
- [ ] T021 [US2] Add CI/local script target documenting the BeatBax CLI invocation
- [ ] T022 [US2] Ensure at least three fixtures pass documented tolerances

## Polish

- [ ] T900 Link `docs/renderer.md` from root README development section
- [ ] T901 Move this folder to `specs/complete/003-gameboy-renderer/` and update `specs/STATUS.md` when shipped
