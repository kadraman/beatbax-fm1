# Implementation Plan: Live play and record

**Spec**: [spec.md](spec.md) | **Date**: 2026-11-11 | **Branch**: `feat/007-live-play-record`

## Summary

Add a live voice path fed by key matrix and USB MIDI, plus REC modes that write into the current pattern (realtime while playing, step while stopped). Persist via existing SAVE (006) and verify round-trip.

## Technical context

- **Packages / surfaces**: `firmware/app/live/`, `firmware/app/record/`, `docs/midi.md`
- **Languages**: C
- **Testing**: host unit tests for quantize/step write; hardware checklist 007
- **Client profile**: firmware
- **Performance / constraints**: event queues drained in render tick; no USB blocking in callback

## Constitution Check

- [x] No invented `.bax` syntax — notes as pattern cells only
- [x] Song model — writes 002 structures
- [x] Chip isolation — Game Boy voices
- [x] Compatibility — saved `.bax` verifies
- [x] Audio callback rules — queued events
- [x] UI — REC/PLAY graphical feedback
- [x] Licensing — firmware
- [x] Tests planned

**Exceptions**: none

## Project structure

```
firmware/app/live/
firmware/app/record/
docs/midi.md
docs/hardware/checklist-007.md
```

## Implementation

### Firmware

- Live synth path sharing renderer voices with documented steal policy (prefer free noise/pulse when song holds channels — decide and document).
- MIDI IN parse on main loop; push note events to lock-free/single-producer queue.
- REC state machine: off / armed / recording; realtime vs step.

### UI

- REC indicator; step cursor for step-record; clear feedback when refuse.

### Documentation updates

- MIDI map; record modes; soak results.

## Testing strategy

### Unit tests

- Step placement math; quantization to pattern grid.

### Integration / CLI comparison

- Save recorded song; `beatbax verify`; optional render compare.

### Hardware / QA

- checklist-007: keys, MIDI, both REC modes, save round-trip, soak.

## Migration and compatibility

- N/A

## Open implementation questions

- Default: replace steps on record vs overdub (recommend **replace within armed channel** for clarity).
- MIDI channel: channel 1 = selected instrument vs fixed map to GB channels 1–4.
