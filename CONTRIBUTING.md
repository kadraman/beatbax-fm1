# Contributing to BeatBax FM-1

Thank you for helping. This project follows the same spec-driven process as [BeatBax](https://github.com/kadraman/beatbax).

## Before you write code

1. Read [specs/constitution.md](specs/constitution.md) and [specs/README.md](specs/README.md).
2. Check [specs/STATUS.md](specs/STATUS.md) for related in-flight or shipped work.
3. Open a **Feature**, **Specification**, or **Bug** issue using the GitHub templates.
4. Ensure `specs/features/NNN-slug/` exists with `spec.md`, `plan.md`, and `tasks.md` before implementation PRs (except trivial docs/chores that do not change behavior).

## Branches

- Features: `feat/NNN-slug`
- Fixes tied to a spec: `fix/NNN-slug`
- Open PRs against `main`

## Pull requests

Use the PR template. Feature and spec work MUST link:

- The GitHub issue
- `spec.md`, `plan.md`, and `tasks.md`

Include a Constitution Check. State which tests you ran for that phase.

## Tests

- Host song-model and renderer changes: automated tests plus comparison against the BeatBax CLI where the active spec requires it.
- Firmware audio/UI changes: host tests first; a real FM-1 hardware checklist is required before a flashing release, not before every host-only change.
- Do not reduce coverage when refactoring.

## Safety when flashing

- Back up projects/samples/voices from any firmware you care about before switching.
- Document recovery (OCT- at power-on; return to stock) in any PR that changes install steps.
- Never commit stock firmware binaries, flash dumps, or private device identifiers.

## License

By contributing, you agree that:

- Specs, docs, and host (Apache) trees are licensed under Apache-2.0 unless a file says otherwise.
- Firmware that links Felucca is GPL-3.0-or-later.

See [LICENSING.md](LICENSING.md).

## Code of conduct expectations

Be precise, kind, and specific. Prefer small PRs with tests. Ask in the issue when a requirement is missing from the spec.
