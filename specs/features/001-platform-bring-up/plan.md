# Implementation Plan: Platform bring-up

**Spec**: [spec.md](spec.md) | **Date**: 2026-11-11 | **Branch**: `feat/001-platform-bring-up`

## Summary

Vendor or submodule a pinned Felucca tree, add a minimal BeatBax FM-1 app that draws a diagnostic screen, polls controls, plays a fixed-point test tone in the audio callback, and exposes USB. Document Windows toolchain setup, install, OCT- rescue, and return to stock.

## Technical context

- **Packages / surfaces**: `firmware/` (new), `docs/hardware/` (new)
- **Languages**: C (firmware, fixed-point tone)
- **Testing**: build smoke on CI if possible without device; hardware checklist required for `complete`
- **Client profile**: firmware
- **Performance / constraints**: audio callback under half of 256-frame budget; dirty LCD updates only

## Constitution Check

GATE: must pass before implementation. Re-check after design changes.

- [x] No invented `.bax` syntax or undocumented BeatBax language behavior
- [x] Song model / expansion / timing impact identified (or N/A) — N/A
- [x] Chip voices remain isolated; song model does not gain chip-specific semantics — N/A
- [x] Determinism and BeatBax compatibility preserved (or migration documented) — N/A (no songs yet)
- [x] Audio callback rules respected (or N/A for host-only) — tone only; no flash/draw/USB in callback
- [x] UI remains graphical multi-screen; no on-panel source editor (or N/A) — single diagnostic screen
- [x] Licensing boundary respected (host Apache; Felucca only in firmware)
- [x] Tests planned for new behavior — build + hardware checklist

**Exceptions**: none

## Project structure

```
firmware/
  README.md                 # build, flash, pin
  third_party/felucca/      # or submodule + pin file
  app/                      # BeatBax FM-1 bring-up app
docs/hardware/
  bring-up.md
  recovery.md
  checklist-001.md
```

## Implementation

### Host / song model

N/A

### Firmware / platform

- Record Felucca commit in `firmware/FELUCCA_PIN` (or equivalent).
- Wire app entry to Felucca platform init, LCD, panel scan, audio, USB.
- Diagnostic screen: title, firmware version, control activity log or grid.
- Test tone: fixed-point square or sine table; gated by a documented button (e.g. highest key or PLAY).
- USB: prefer the Felucca MIDI path already proven by community firmwares; add CDC only if needed for logs.

### UI screens

- One screen only. Map every control to a visible indicator.

### Audio / renderer

- Minimal callback fill; measure ticks once on hardware.

### Transfer / storage

- Install via Felucca/`fm1_install.py`-style flow; document only. No song store.

### Documentation updates

- Root README install pointer; `docs/hardware/recovery.md`; toolchain notes.

## Testing strategy

### Unit tests

- Host-compile of pure tone math if extracted; otherwise N/A.

### Integration / CLI comparison

- N/A

### Hardware / QA

- Checklist: build, flash, boot paint, every control, tone, USB enum, OCT- rescue, reinstall.

## Migration and compatibility

- Switching from other firmwares loses their projects; document backup.

## Open implementation questions

- Submodule vs vendored snapshot of Felucca.
- USB MIDI device name string for BeatBax FM-1.
