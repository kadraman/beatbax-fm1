# Implementation Plan: Game Boy renderer on the host

**Spec**: [spec.md](spec.md) | **Date**: 2026-11-11 | **Branch**: `feat/003-gameboy-renderer`

## Summary

Add a fixed-point DMG renderer beside the song model, plus a harness that compares against BeatBax CLI output using event timing and a resampled WAV metric.

## Technical context

- **Packages / surfaces**: `host/render/gameboy/`, `host/tests/`, `tools/compare-render`
- **Languages**: Portable C fixed-point (shared with future firmware)
- **Testing**: unit + comparison harness
- **Client profile**: host
- **Performance / constraints**: algorithm must be suitable for SRAM on device; no heap in render tick loop

## Constitution Check

- [x] No invented `.bax` syntax
- [x] Song model impact — consumes 002 model only
- [x] Chip voices isolated — Game Boy backend only
- [x] Determinism preserved — same seed/options → same PCM
- [x] Audio callback rules — host simulation of block render OK; no float in shared code
- [x] UI N/A
- [x] Licensing Apache host
- [x] Tests planned

**Exceptions**: none

## Project structure

```
host/render/gameboy/    # voices, mixer, scheduler tick
host/tests/render/
tools/compare-render.*
docs/renderer.md        # tolerances, rates, how to run harness
```

## Implementation

### Host / song model

- Expand sequences/patterns using 002 semantics into a tick/event stream.

### Audio / renderer

- Pulse, wave, noise cores; envelope; duty; wave RAM; supported effects.
- Block render API mirroring device callback shape (e.g. 256 frames) for later 004.

### Documentation updates

- Tolerances, resampling method, CLI commands.

## Testing strategy

### Unit tests

- Per-voice parameter tests; determinism smoke.

### Integration / CLI comparison

- Export reference via BeatBax CLI; compare events + WAV metric.

### Hardware / QA

- N/A

## Migration and compatibility

- Renderer version noted in docs when tolerances change.

## Open implementation questions

- Primary WAV metric: correlation vs RMS dB — choose one in `docs/renderer.md` at implementation start and keep SC-003 satisfied.
- Whether to compare at 44117.6 Hz native then resample, or render host path at 44100 for tests while keeping a 44117.6 mode for device parity.
