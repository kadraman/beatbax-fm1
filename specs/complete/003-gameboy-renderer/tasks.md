# Tasks: Game Boy renderer on the host

**Input**: [spec.md](spec.md), [plan.md](plan.md)

## Phase 0: Foundation (blocks stories)

- [x] T001 Create `host/render/gameboy/` skeleton with fixed-point types and block-render API
- [x] T002 [P] Write `docs/renderer.md` with rate, tolerance, and CLI reference commands
- [x] T003 Wire renderer input to `host/song/` model from feature 002

**Checkpoint**: empty renderer links and renders silence deterministically.

## Phase 1: User Story 1 — Render supported songs (P1)

- [x] T010 [US1] Implement pulse1/pulse2 cores (duty, envelope) in `host/render/gameboy/`
- [x] T011 [US1] Implement wave channel (wave table + volume) in `host/render/gameboy/`
- [x] T012 [US1] Implement noise channel in `host/render/gameboy/`
- [x] T013 [US1] Implement scheduler/expansion tick consuming song model
- [x] T014 [US1] Implement supported Game Boy effects from the frozen surface list
- [x] T015 [US1] Unit tests under `host/tests/render/` per voice

**Checkpoint**: fixtures produce non-silent WAV with all used channels.

**Notes (this increment)**:
- Effects implemented: `vib`, `arp`, `port`, `bend` (named effects),
  `vol_env`/`pitch_env`/`duty_env`/`arp_env` (software macros), and the pulse-1
  NR10 frequency sweep.
- Composition implemented: `section`/`form`/`channel auto` routing and temporary
  instruments `inst(name[,dur])`.
- Remaining Phase-1 scope: pattern group/token repeats `( … )*N` / `*N` (not used
  by the current fixtures). `grassland_dash.bax` renders with correct note timing
  but a residual phase drift (~0.55 correlation) from unhandled vibrato-duration
  and other continuous-effect precision; the three simpler fixtures pass ≥0.95.

## Phase 2: User Story 2 — Compare to BeatBax CLI (P1)

- [x] T020 [US2] Implement `tools/compare-render` harness (events + resampled WAV metric)
- [x] T021 [US2] Add CI/local script target documenting the BeatBax CLI invocation
- [x] T022 [US2] Ensure at least three fixtures pass documented tolerances

## Polish

- [x] T900 Link `docs/renderer.md` from root README development section
- [x] T901 Move this folder to `specs/complete/003-gameboy-renderer/` and update `specs/STATUS.md` when shipped
