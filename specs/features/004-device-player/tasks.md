# Tasks: Device player

**Input**: [spec.md](spec.md), [plan.md](plan.md)

## Phase 0: Foundation (blocks stories)

- [ ] T001 Confirm features 001 and 003 are at least `planned` before starting implementation
- [ ] T002 Add `docs/hardware/checklist-004.md` template
- [ ] T003 Select fixture path under `songs/gameboy/` and document embed method in `firmware/README.md`

**Checkpoint**: fixture chosen; checklist ready.

## Phase 1: User Story 1 — Play fixture (P1)

- [ ] T010 [US1] Compile shared renderer into `firmware/app/` with SRAM section attributes
- [ ] T011 [US1] Load/embed fixture into RAM song model at startup
- [ ] T012 [US1] Wire PLAY transport and MASTER level into player
- [ ] T013 [US1] Draw minimal graphical status screen (playing/stopped, name)
- [ ] T014 [US1] Ensure main loop calls platform poll / watchdog feed

**Checkpoint**: song plays and stops on device.

## Phase 2: User Story 2 — Budgets (P1)

- [ ] T020 [US2] Instrument callback with tick measurement; record results in checklist
- [ ] T021 [US2] Audit callback for flash/draw/USB waits (code review checklist item)
- [ ] T022 [US2] Run soak test and file results in `docs/hardware/checklist-004.md`

## Polish

- [ ] T900 Update root README status for player milestone
- [ ] T901 Move this folder to `specs/complete/004-device-player/` and update `specs/STATUS.md` when shipped
