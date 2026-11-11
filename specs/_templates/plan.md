# Implementation Plan: 

**Spec**: [spec.md](spec.md) | **Date**: YYYY-MM-DD | **Branch**: `feat/NNN-slug`

## Summary

Primary requirement and technical approach.

## Technical context

- **Packages / surfaces**: e.g. `firmware/`, `host/`, `songs/`
- **Languages**: C (firmware fixed-point); host language as chosen in this plan
- **Testing**: host unit/integration; hardware checklist when flashing
- **Client profile**: firmware / host / both
- **Performance / constraints**: audio budget, flash, SRAM (or N/A)

## Constitution Check

GATE: must pass before implementation. Re-check after design changes.

- [ ] No invented `.bax` syntax or undocumented BeatBax language behavior
- [ ] Song model / expansion / timing impact identified (or N/A)
- [ ] Chip voices remain isolated; song model does not gain chip-specific semantics
- [ ] Determinism and BeatBax compatibility preserved (or migration documented)
- [ ] Audio callback rules respected (or N/A for host-only)
- [ ] UI remains graphical multi-screen; no on-panel source editor (or N/A)
- [ ] Licensing boundary respected (host Apache; Felucca only in firmware)
- [ ] Tests planned for new behavior

**Exceptions** (fill only if a principle cannot be met):

| Principle | Why needed | Simpler alternative rejected because |
|-----------|------------|-------------------------------------|
| | | |

## Project structure

List directories and files this feature will add or change.

## Implementation

### Host / song model

### Firmware / platform

### UI screens

### Audio / renderer

### Transfer / storage

### Documentation updates

(`docs/`, `specs/global/` if contracts change)

## Testing strategy

### Unit tests

### Integration / CLI comparison

### Hardware / QA

## Migration and compatibility

## Open implementation questions
