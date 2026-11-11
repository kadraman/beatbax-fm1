# Compatibility and determinism

BeatBax FM-1 is a sibling product of BeatBax. Compatibility means the same `.bax` files work across Desktop, CLI, and this device for the supported subset.

## MUST

- Preserve `.bax` as the only song interchange format with BeatBax Desktop and CLI for phases 001–007.
- Keep parse → expand → render **deterministic** for the same source and options on host and device (device may differ in sample rate; see [audio.md](audio.md)).
- Require that songs written by the device pass `beatbax verify` and play with the BeatBax CLI for the supported Game Boy subset.
- Require that songs written by the device open in BeatBax Desktop without requiring a conversion step.
- Fail loudly with a named reason when a `.bax` uses unsupported chips, constructs, or limits.
- Document migration when on-device limits or supported surfaces change.
- Prefer additive changes to host APIs and transfer protocols.

## MUST NOT

- Ship silent breaking changes to written `.bax` meaning.
- Soften validation so unsupported songs load as a reduced subset without telling the user.
- Invent syntax that Desktop/CLI cannot parse.
- Reimplement UGE / WAV / MIDI export on device as a substitute for CLI compatibility (out of scope for 001–007).

## Core change checklist

1. Spec (or amendment) exists under `specs/features/` or `specs/complete/`.
2. Constitution Check in `plan.md` records song-model / timing / chip impact.
3. Tests updated first or in parallel (round-trip fixtures; CLI comparison where required).
4. User-facing docs updated when composers or installers are affected.

## External oracle

- Installed `@beatbax/cli` / `beatbax` is the compatibility oracle for verify and play.
- Pin or document the minimum BeatBax CLI version in feature 002 when implementation starts.
