# Implementation Plan: Device player

**Spec**: [spec.md](spec.md) | **Date**: 2026-11-11 | **Branch**: `feat/004-device-player`

## Summary

Integrate the shared Game Boy renderer into the Felucca-based firmware, embed one fixture song, wire PLAY/MASTER, keep hot code in SRAM, and measure callback cost on hardware.

## Technical context

- **Packages / surfaces**: `firmware/app/`, `host/render/gameboy/` (shared), `songs/gameboy/`
- **Languages**: C
- **Testing**: host renderer tests remain; new hardware checklist 004
- **Client profile**: firmware
- **Performance / constraints**: < half of 256-frame block; SRAM `.ram_text`

## Constitution Check

- [x] No invented `.bax` syntax — fixture from supported subset
- [x] Song model — load via 002 parser into RAM at boot/load
- [x] Chip isolation — Game Boy only
- [x] Determinism — same fixture as host
- [x] Audio callback rules respected
- [x] UI graphical status only
- [x] Licensing — firmware GPL with Felucca; shared renderer kept license-clean for dual use
- [x] Tests planned — hardware checklist + host still green

**Exceptions**: none

## Project structure

```
firmware/app/player.*
firmware/app/ui_status.*
docs/hardware/checklist-004.md
```

## Implementation

### Host / song model

- Reuse parse-at-build or parse-at-boot of embedded `.bax` bytes via `host/song` sources compiled into firmware.

### Firmware / platform

- SRAM placement for render tick; far calls as required.
- PLAY edge handling in main loop; transport flags read by callback.

### UI screens

- Status screen: song title, playing/stopped, crude playhead optional.

### Audio / renderer

- Callback fills codec buffer using 003 algorithms at 44117.6 Hz.

### Transfer / storage

- Embed fixture; no user store yet.

### Documentation updates

- Checklist with timing method (`fm1_ticks` or equivalent).

## Testing strategy

### Unit tests

- Host renderer continues to pass.

### Integration / CLI comparison

- Same fixture ID as host comparison set.

### Hardware / QA

- checklist-004: boot, play, stop, master, 10-minute soak, callback us measurement.

## Migration and compatibility

- N/A beyond backup-before-flash.

## Open implementation questions

- Build-time conversion of `.bax` to a packed bin vs parse `.bax` text on device at load.
