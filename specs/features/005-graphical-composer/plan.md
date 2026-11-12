# Implementation Plan: Graphical composer

**Spec**: [spec.md](spec.md) | **Date**: 2026-11-11 | **Branch**: `feat/005-graphical-composer`

## Summary

Build a screen stack on the device player base: navigation (SEQ/HOME/help), five editor screens mutating the song model, and a control-map table ensuring every panel control is handled.

## Technical context

- **Packages / surfaces**: `firmware/app/ui/`, `docs/ui/`
- **Languages**: C
- **Testing**: host UI logic tests where extracted; hardware checklist for navigation and editing
- **Client profile**: firmware UI
- **Performance / constraints**: dirty rect drawing; no draw in audio callback

## Constitution Check

- [x] No invented `.bax` syntax — model edits only expressible constructs
- [x] Song model — mutates 002 structures
- [x] Chip isolation — Game Boy params only
- [x] Determinism — N/A for UI; model write still deterministic later
- [x] Audio callback rules — UI on main loop only
- [x] UI graphical multi-screen — yes
- [x] Licensing — firmware
- [x] Tests planned

**Exceptions**: none

## Project structure

```
firmware/app/ui/screens/{song,pattern,chain,instrument,mix}.c
firmware/app/ui/nav.c
firmware/app/ui/help.c
firmware/app/ui/controls_map.c
docs/ui/controls-matrix.md
docs/hardware/checklist-005.md
```

## Implementation

### UI screens

- Implement five screens with shared widgets (step grid, tiles, envelope polyline, wave editor).
- Navigation and help card overlays.
- Control map: function table per screen × control.

### Host / song model

- Optional host preview of screen layouts later; not required for 005.

### Audio

- Reuse player; live-audition instrument on note keys.

### Documentation updates

- Controls matrix; screen map screenshots placeholders.

## Testing strategy

### Unit tests

- Pure functions for step index math, chain tile ops if extracted.

### Integration / CLI comparison

- After 006: exported `.bax` from edited song verifies — foreshadow tests.

### Hardware / QA

- checklist-005: navigate all screens, edit each, play, control matrix spot-check.

## Migration and compatibility

- N/A

## Open implementation questions

- Widget toolkit vs immediate-mode draw helpers.
- Whether Mix mute/solo maps to BeatBax channel volume only (yes unless model has mute — then use volume 0 + UI flag that does not invent syntax; prefer volume-only if mute is not in `.bax`).
