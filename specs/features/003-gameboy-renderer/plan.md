# Implementation Plan: Game Boy renderer on the host

**Spec**: [spec.md](spec.md) | **Date**: 2026-11-19 | **Branch**: `feat/003-gameboy-renderer`

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

## Resolved implementation decisions

- **Primary WAV metric: normalized Pearson correlation ≥ 0.95** on a trimmed non-silent
  region (SC-003). Correlation is self-normalizing, so it is robust to the gain/pan
  scaling differences between a fixed-point renderer and the CLI's float path. The
  harness also prints the RMS dB difference as a diagnostic (not a pass/fail gate) to
  help triage envelope/amplitude bugs when correlation is near the threshold. `--normalize`
  is NOT used on the reference export; both signals are compared at their natural levels
  and the correlation metric handles overall gain.
- **Rate strategy: render both sides at 44100 Hz** (the CLI's default) and compare
  sample-for-sample — no resampling. The renderer is **rate-parameterized** (a single
  `sample_rate` constant drives phase increments and envelope/effect timing); the same
  shared algorithm runs at **44117.6 Hz** on-device in feature 004. This keeps the host
  and device renderers bit-identical by construction and sidesteps the WAV header's
  integer-rate limitation. Event-level comparison (ISM JSON) is rate-independent.
- **Reference channel handling**: the CLI `wav` exporter emits stereo (2ch, 16-bit). The
  harness downmixes the reference to mono (mean of L/R) before correlating against the
  host renderer's mono output.
