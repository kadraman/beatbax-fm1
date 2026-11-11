# Implementation Plan: Store and USB transfer

**Spec**: [spec.md](spec.md) | **Date**: 2026-11-11 | **Branch**: `feat/006-store-usb-transfer`

## Summary

Define a flash song store with commit markers, wire SAVE/LOAD UI, and ship a host CLI that transfers `.bax` over USB using a documented protocol.

## Technical context

- **Packages / surfaces**: `firmware/app/store/`, `host/tools/bax-xfer/`, `docs/transfer.md`
- **Languages**: C (firmware); host tool in Python or C (match Felucca install tooling comfort)
- **Testing**: host protocol unit tests; hardware save/load/power-loss checklist
- **Client profile**: firmware + host
- **Performance / constraints**: erase only when silent; watchdog fed during long USB ops on main loop

## Constitution Check

- [x] No invented `.bax` syntax — canonical writer only
- [x] Song model — serialize via 002
- [x] Chip isolation — N/A
- [x] Compatibility — verify oracle on downloads
- [x] Audio callback — no flash in callback
- [x] UI — graphical file browser screens
- [x] Licensing — host tool Apache; firmware GPL
- [x] Tests planned

**Exceptions**: none

## Project structure

```
firmware/app/store/
host/tools/bax-xfer/
docs/transfer.md
docs/hardware/checklist-006.md
```

## Implementation

### Transfer / storage

- Slot table + double-buffer or journal commit.
- SAVE writes canonical `.bax` bytes.
- Host tool: list/get/put/delete as specified.

### UI

- Song browser / name entry via keys or encoder (graphical), SAVE confirmation.

### Documentation updates

- Protocol; flash region map (relative, not stock dumps).

## Testing strategy

### Unit tests

- Commit marker logic; parser of store headers on host.

### Integration / CLI comparison

- Downloaded files through `beatbax verify`.

### Hardware / QA

- checklist-006 including power interrupt during save.

## Migration and compatibility

- Store version byte; refuse unknown versions loudly.

## Open implementation questions

- Prefer USB-MIDI SysEx (WebMIDI-friendly) vs custom CDC — default SysEx for ecosystem parity with community tools unless Felucca pin makes CDC easier.
