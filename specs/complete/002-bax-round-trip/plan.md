# Implementation Plan: `.bax` round-trip on the host

**Spec**: [spec.md](spec.md) | **Date**: 2026-11-11 | **Branch**: `feat/002-bax-round-trip`

## Summary

Implement a portable song-model library under `host/` that parses and writes the Game Boy `.bax` subset, with fixtures under `songs/` and a verify script that shells out to the BeatBax CLI.

## Technical context

- **Packages / surfaces**: `host/song/`, `songs/gameboy/`, `tools/verify-fixtures` (names may adjust)
- **Languages**: Prefer **portable C** (or C99) for the model so firmware 004+ can link the same parse/write without a JS runtime. Host tests may use a thin runner (CTest, Python, or Node invoking the CLI).
- **Testing**: unit tests + `beatbax verify` on written fixtures
- **Client profile**: host
- **Performance / constraints**: must fit later on-device RAM limits for typical songs (exact caps in 006); 002 documents provisional limits

## Constitution Check

- [x] No invented `.bax` syntax or undocumented BeatBax language behavior
- [x] Song model / expansion / timing impact identified — model mirrors BeatBax subset only
- [x] Chip voices remain isolated — parse only; no renderer
- [x] Determinism and BeatBax compatibility preserved — canonical write + CLI verify
- [x] Audio callback rules respected — N/A
- [x] UI remains graphical — N/A
- [x] Licensing boundary respected — Apache-2.0 host, no Felucca
- [x] Tests planned for new behavior

**Exceptions**: none

## Project structure

```
host/song/           # parse, model, write
host/tests/
songs/gameboy/       # fixtures
tools/verify-bax.*   # oracle script
docs/song-model.md   # supported surface list
```

## Implementation

### Host / song model

- Tokenize/parse Game Boy subset; build structs; pretty-print canonical `.bax`.
- Error type includes construct name string.
- Freeze supported grammar list against a documented BeatBax version.

### Firmware / platform

N/A (library only; firmware links later)

### UI / Audio / Transfer

N/A

### Documentation updates

- Supported surface checklist; CLI version pin; how to add a fixture.

## Testing strategy

### Unit tests

- Parse happy paths; refusal cases; canonical idempotence.

### Integration / CLI comparison

- Script: write fixtures → `beatbax verify` each.

### Hardware / QA

- N/A

## Migration and compatibility

- When BeatBax grammar evolves, bump oracle version and amend this spec or a follow-on feature.

## Open implementation questions

- Hand-written parser vs generating from BeatBax grammar artifacts (must not invent syntax either way).
- Provisional max pattern/instrument counts for device foreshadowing.
