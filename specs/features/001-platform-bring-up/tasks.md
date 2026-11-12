# Tasks: Platform bring-up

**Input**: [spec.md](spec.md), [plan.md](plan.md)

**Format**: `- [ ] Tnnn [P?] [USn?] Description` with **exact repo paths**.

## Phase 0: Foundation (blocks stories)

- [ ] T001 Create `firmware/README.md` describing Windows JieLi pi32 requirements and env vars
- [ ] T002 Add `firmware/FELUCCA_PIN` with the chosen Felucca commit SHA and fetch instructions
- [ ] T003 [P] Write `docs/hardware/recovery.md` (OCT- rescue, return to stock, backup warning)
- [ ] T004 [P] Write `docs/hardware/bring-up.md` outlining diagnostic goals for this phase
- [ ] T005 Confirm `LICENSING.md` still accurately describes Felucca-linked firmware as GPL

**Checkpoint**: docs and pin exist before app code is required for review of process PRs; app tasks follow when implementing.

## Phase 1: User Story 1 — Boot diagnostic firmware (P1)

- [ ] T010 [US1] Scaffold `firmware/app/` entry linked against pinned Felucca
- [ ] T011 [US1] Implement LCD init + single diagnostic frame in `firmware/app/`
- [ ] T012 [US1] Poll all panel controls and reflect activity on the diagnostic UI
- [ ] T013 [US1] Implement fixed-point test tone in the audio callback (no flash/draw/USB waits)
- [ ] T014 [US1] Enable USB enumeration as documented in `firmware/README.md`
- [ ] T015 [US1] Document build and package commands in `firmware/README.md`

**Checkpoint**: diagnostic image builds and is installable.

## Phase 2: User Story 2 — Recovery (P1)

- [ ] T020 [US2] Complete `docs/hardware/checklist-001.md` with pass/fail rows for screen, controls, tone, USB, OCT-, stock reinstall
- [ ] T021 [US2] Link recovery docs from root `README.md`
- [ ] T022 [US2] Record maintainer checklist results when hardware is available (required before status `complete`)

## Polish

- [ ] T900 Update root `README.md` project layout once `firmware/` exists
- [ ] T901 Move this folder to `specs/complete/001-platform-bring-up/` and update `specs/STATUS.md` when shipped
