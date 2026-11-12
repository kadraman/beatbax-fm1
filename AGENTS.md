# Agent instructions (BeatBax FM-1)

BeatBax FM-1 is custom firmware and host tooling for composing BeatBax `.bax` songs on the M-VAVE FM-1. Treat it as a production-quality sibling of [BeatBax](https://github.com/kadraman/beatbax): correctness, determinism, and compatibility over speed.

## Authority

1. [specs/constitution.md](specs/constitution.md)
2. [specs/global/](specs/global/)
3. The **active** feature under [specs/features/](specs/features/) (or a shipped spec under [specs/complete/](specs/complete/) when changing that behavior; [specs/archive/](specs/archive/) is historical only)
4. User-facing docs in [docs/](docs/) when present
5. Code

Do not invent `.bax` language features or syntax. Do not assume [ROADMAP.md](ROADMAP.md) items are implemented. Ask when a spec is missing.

Index: [specs/STATUS.md](specs/STATUS.md). Process: [specs/README.md](specs/README.md).

## Workflow

1. Read the constitution and relevant global contracts.
2. Open or update `specs/features/NNN-slug/` (`spec.md` → `plan.md` → `tasks.md`).
3. Implement incrementally from `tasks.md`.
4. Add tests for the phase (host round-trip, renderer comparison, or hardware checklist as specified).
5. Update specs and docs if behavior changes.

## Compatibility with BeatBax

- Songs are `.bax` text compatible with BeatBax Desktop and CLI.
- On-device editing uses a structured model; canonical `.bax` is written back.
- Unsupported constructs MUST be refused with a named reason.
- Do not silently simplify songs to “make them fit.”
- Device exports (UGE, WAV, MIDI) stay on the BeatBax CLI; do not reimplement them on-device unless a later spec says so.

## Hardware and firmware rules

- Base the device HAL on a **pinned** Felucca commit (feature 001). Do not use the stock `fm1_open` binary SDK as the synth engine.
- Hot audio runs from SRAM. No flash erase, screen draw, or USB wait inside the audio callback.
- First audio path is **fixed-point**. Enabling the FPU is a separate measured spec.
- UI is fully graphical multi-screen. Do not add a text editor for `.bax` source on the panel.
- Every physical control has a specified job on each screen, or an on-screen “unused here” response.

## Licensing boundaries

- Host song code and Apache-licensed trees MUST NOT link Felucca.
- Firmware that links Felucca is GPL-3.0-or-later. See [LICENSING.md](LICENSING.md).
- Do not copy synth or song-format code from Sidou, Chipbox, OPN-1, or stock firmware into this tree. Copy product patterns only.

## Do not

- Load every file under `specs/complete/` or `specs/archive/` by default.
- Soften validation to hide invalid or unsupported songs.
- Invent chip behavior beyond what BeatBax’s Game Boy surface (and later chip specs) define.
- Start a later phase’s implementation before the previous phase’s spec is at least `planned`.
- Publish stock firmware images, device flash dumps, or secrets.
