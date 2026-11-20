# BeatBax FM-1 Game Boy Renderer (`host/render/gameboy/`)

Fixed-point Game Boy (DMG-01) renderer that consumes the feature 002 song model
(`host/song/`) and produces mono PCM. This is the algorithm that feature 004
runs on-device in SRAM, so the hot path is portable C (C99), fixed-point, and
allocation-free.

## Licensing and Boundaries

- **License:** Apache-2.0 (see [LICENSE](../../../LICENSE))
- **No Felucca Rule:** This directory is part of the host song codebase. It
  **MUST NOT** link, include, or depend on Felucca or `felucca-bsp` (GPL/MPL,
  restricted to `firmware/`).
- Code here stays self-contained portable C with no external dependencies beyond
  standard C headers, so it can run standalone on the host or be linked into
  firmware.

## Architecture

- **`gb_fixed.h`**: Fixed-point scalar types (Q15 output, 16.16 phase).
- **`gb_render.h` / `gb_render.c`**: Public block-render API and render state.

## Status (Phase 0)

The skeleton links against the song model and renders **silence**
deterministically. Voice cores (pulse 1/2, wave, noise), scheduler/expansion,
and supported effects are implemented in Phase 1 (`specs/complete/003-gameboy-renderer/tasks.md`).