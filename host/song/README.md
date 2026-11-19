# BeatBax FM-1 Song Model (`host/song/`)

Portable C library for parsing, representing, and canonically serializing BeatBax `.bax` songs for the supported Game Boy (DMG-01) subset.

## Licensing and Boundaries

- **License:** Apache-2.0 (see [LICENSE](../../LICENSE))
- **No Felucca Rule:** This directory is part of the host song codebase. It **MUST NOT** link, include, or depend on Felucca or `felucca-bsp` (which are GPL/MPL and restricted to `firmware/`).
- Code in this directory must remain clean, self-contained, portable C (C99) with no external library dependencies beyond standard C headers, capable of compiling standalone on the host or being linked into firmware.

## Architecture

- **`model.h` / `model.c`**: In-memory structured representation of a BeatBax song (metadata, chip, tempo, instruments, patterns, sequences, channel routing).
- **`parse.h` / `parse.c`**: Parser transforming `.bax` text into the structured model, with detailed diagnostics identifying unsupported constructs by name.
- **`write.h` / `write.c`**: Canonical `.bax` writer producing formatted song text that validates against the `beatbax verify` CLI oracle.
