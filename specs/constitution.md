# BeatBax FM-1 Constitution

**Version**: 1.0.0 | **Ratified**: 2026-11-11 | **Last Amended**: 2026-11-11

This document is binding on all specifications, plans, tasks, and implementations. Feature specs MUST NOT contradict it. If they conflict, change the spec — not this constitution — unless the constitution is being amended on purpose.

Every `plan.md` MUST include a **Constitution Check** against these principles.

---

## I. Specifications First (NON-NEGOTIABLE)

- Do not invent `.bax` language features, syntax, or BeatBax behavior.
- Do not infer intent from code alone when a spec exists.
- Align implementations with documented specs (`specs/` is authoritative for engineering behavior).
- Only implement features that are specified under `specs/features/` (or `specs/complete/` for shipped behavior), or that the user has explicitly approved.
- If essential information is missing, ask before implementing.
- [ROADMAP.md](../ROADMAP.md) is product direction, not an engineering spec.

## II. Production Quality (NON-NEGOTIABLE)

BeatBax FM-1 is a production-quality system, not a prototype. Prefer correctness, determinism, and long-term maintainability over speed.

- No stub logic in shipped paths.
- Prefer boring, explicit code.
- Host song code stays portable and testable without hardware.
- Firmware paths that ship MUST meet the audio and UI budgets in [global/audio.md](global/audio.md) and [global/ui.md](global/ui.md).

## III. Song Model Is Stable Core (NON-NEGOTIABLE)

These subsystems are core infrastructure. Changes require spec review, downstream impact analysis, and tests updated first or in parallel:

- `.bax` parse and canonical write for the supported subset
- Song model shapes (instruments, patterns, sequences, channels)
- Pattern and sequence expansion semantics
- Timing / scheduler meaning shared with BeatBax

Do **not** casually change song-model meanings, expansion ordering, or timing semantics.

## IV. BeatBax Compatibility (NON-NEGOTIABLE)

- `.bax` text remains the song. The device edits a structured model and writes canonical `.bax`.
- A song the firmware saves MUST `beatbax verify`, play via the BeatBax CLI, and open in BeatBax Desktop (for the supported subset).
- Unsupported constructs MUST be refused with a **named reason**. Never silently simplify.
- Specs quote BeatBax’s published Game Boy (and later chip) surfaces. They do not add syntax.
- On-device UGE / WAV / MIDI export is out of scope unless a later feature specifies it; use the BeatBax CLI.

## V. Chip Voice Isolation (NON-NEGOTIABLE)

- Chip voices implement isolated audio backends only. They MUST NOT change pattern or sequence meaning in the shared song model.
- Host song-model libraries MUST NOT depend on a specific chip renderer beyond declared chip directives.
- Additional chips are optional, discoverable, and added via their own feature specs.
- Core song-model work MUST preserve chip isolation and MUST NOT assume a fixed set of chips beyond what the active specs declare.

## VI. Determinism (NON-NEGOTIABLE)

- Parse → expand → render MUST be deterministic for the same `.bax` and options.
- No silent breaking changes.
- Do not “fix” problems by softening validation.
- Renderer comparison tolerances against the BeatBax CLI MUST be numeric and documented in the active feature spec.

## VII. Tests Back Behavior (NON-NEGOTIABLE)

- New behavior requires tests.
- Refactors MUST NOT reduce coverage.
- Host changes: automated tests; `.bax` round-trip and CLI comparison where specified.
- Flashing firmware releases: a real FM-1 hardware checklist is required. Host-only changes do not require a flash for every PR.

## VIII. Audio and UI Budgets (NON-NEGOTIABLE)

- Hot audio runs from SRAM. The audio callback MUST NOT erase flash, draw the screen, or wait on USB.
- First audio path is **fixed-point**. Enabling the FPU is a separate measured spec.
- The audio callback MUST stay within the budget in [global/audio.md](global/audio.md) (target: under half of a 256-frame block).
- UI is fully graphical multi-screen on the 240×240 panel. Do not add a `.bax` source text editor on device.
- Draw dirty regions; a full-screen redraw is expensive (~115 KB RGB565 over SPI).
- Every physical control Felucca’s panel map reports has a specified job on each screen, or an on-screen “unused here” response. No dead buttons.

## IX. Platform and Licensing Boundaries (NON-NEGOTIABLE)

- Device HAL is based on a **pinned** Felucca commit. Do not use the stock `fm1_open` binary SDK as the BeatBax synth engine.
- Host song code MUST NOT link Felucca.
- Firmware that links Felucca is GPL-3.0-or-later. Host trees remain Apache-2.0. See [LICENSING.md](../LICENSING.md).
- Do not copy synth or song-format sources from Sidou, Chipbox, OPN-1, fm1-nes, or stock firmware. Product patterns and documentation references only.
- Do not publish stock firmware images, flash dumps, or secrets.

## X. Agent Constraints

Agents MUST NOT:

- hallucinate undocumented `.bax` syntax
- invent language or chip features
- assume roadmap items are implemented
- skip constitution checks in feature plans
- start a later phase’s implementation before the previous phase’s spec is at least `planned`

Agents MUST:

- follow specs verbatim
- ask when uncertain
- preserve BeatBax compatibility and device safety (recovery docs) above all

---

## Authority stack

1. This constitution
2. [`specs/global/`](global/) contracts
3. The active feature folder under [`specs/features/`](features/) (or [`specs/complete/`](complete/) for shipped behavior)
4. User-facing docs under `docs/` — not a substitute for specs
5. Code

[ROADMAP.md](../ROADMAP.md) is product direction, not an engineering spec.
