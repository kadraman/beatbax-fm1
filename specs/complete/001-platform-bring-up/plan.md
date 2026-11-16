# Implementation Plan: Platform bring-up

**Spec**: [spec.md](spec.md) | **Date**: 2026-11-12 | **Branch**: `feat/001-platform-bring-up`

## Summary

Pin Felucca via `firmware/FELUCCA_PIN` and submodule the matching **felucca-bsp** HAL (Felucca’s `firmware/bsp` at that pin). Add a minimal BeatBax FM-1 diagnostic app that draws one screen, polls controls, plays a fixed-point test tone in the audio callback, and enumerates as USB MIDI **BeatBax FM-1**. Document toolchain setup, install, OCT- rescue, and return to stock.

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
  README.md                      # build, flash, pin, USB identity
  FELUCCA_PIN                    # Felucca commit + derived BSP SHA
  third_party/felucca-bsp/       # git submodule (HAL)
  app/                           # BeatBax FM-1 bring-up app (GPL-3.0-or-later)
docs/hardware/
  bring-up.md
  recovery.md
  checklist-001.md
```

## Locked decisions

| Topic | Choice |
|-------|--------|
| Felucca pin | Exact SHA in `firmware/FELUCCA_PIN`. Linked HAL is that commit’s `firmware/bsp` → [felucca-bsp](https://github.com/hugelton/felucca-bsp) submodule (not Felucca’s synth engines). |
| Submodule path | `firmware/third_party/felucca-bsp` |
| USB | MIDI only (`FELUCCA_CDC=0`). Product: `BeatBax FM-1`. Manufacturer: `BeatBax`. |
| Host simulator | Out of scope for 001 |
| Tone gate | Matrix button **PLAY** (id **12**, Felucca `PANEL_DEFAULT`) toggles a fixed-point square tone |

## Implementation

### Host / song model

N/A

### Firmware / platform

- Record Felucca + BSP SHAs in `firmware/FELUCCA_PIN`.
- Wire `firmware/app/` to BSP init (LCD, panel scan, audio, USB MIDI).
- Diagnostic screen: title, version, control activity grid for every Felucca-mapped control.
- Test tone: fixed-point square in the audio callback; PLAY toggles on/off. No flash erase, draw, or USB wait in the callback.
- USB: BSP MIDI path only.

### UI screens

- One screen only. Map every control to a visible indicator.

### Audio / renderer

- Minimal callback fill; measure ticks once on hardware (checklist).

### Transfer / storage

- Install via BSP / Felucca `fm1_install.py`-style flow; document only. No song store.

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

_None — decisions locked above._
