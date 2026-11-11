# Tasks: Store and USB transfer

**Input**: [spec.md](spec.md), [plan.md](plan.md)

## Phase 0: Foundation (blocks stories)

- [ ] T001 Confirm features 002 and 005 are at least `planned`
- [ ] T002 Document flash region plan in `docs/transfer.md` (against Felucca pin)
- [ ] T003 Scaffold `firmware/app/store/` with header/CRC/commit marker design

**Checkpoint**: store design reviewed before erasing flash on hardware.

## Phase 1: User Story 1 — Save/load on device (P1)

- [ ] T010 [US1] Implement silent-only erase/program path in `firmware/app/store/`
- [ ] T011 [US1] Wire SAVE to canonical `.bax` writer from `host/song/`
- [ ] T012 [US1] Implement load + song browser UI
- [ ] T013 [US1] Refuse SAVE while unsafe/not silent with on-screen reason

**Checkpoint**: power cycle retains saved song.

## Phase 2: User Story 2 — USB host tool (P1)

- [ ] T020 [US2] Define protocol messages in `docs/transfer.md`
- [ ] T021 [US2] Implement device USB handlers for list/get/put
- [ ] T022 [US2] Implement `host/tools/bax-xfer/` CLI
- [ ] T023 [US2] Verify downloaded files with `beatbax verify` in automated or scripted check
- [ ] T024 [US2] Refuse unsupported uploads with named reasons

**Checkpoint**: device ↔ host ↔ CLI/Desktop loop works.

## Phase 3: User Story 3 — Power-loss safety (P1)

- [ ] T030 [US3] Implement double-buffer or commit marker so last good song survives interrupt
- [ ] T031 [US3] Add corruption detection UI path
- [ ] T032 [US3] Record power-loss test in `docs/hardware/checklist-006.md`

## Polish

- [ ] T900 Document host tool in root README
- [ ] T901 Move this folder to `specs/complete/006-store-usb-transfer/` and update `specs/STATUS.md` when shipped
