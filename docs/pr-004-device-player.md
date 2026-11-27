# PR description — feat/004-device-player (device player + effect parity)

> Copy the block below into the GitHub PR body. Title: `feat: device player (004) + effect parity (008)`.

## Summary

Implements the **device player** (feature 004) — running the fixed-point Game Boy renderer on the M-VAVE FM-1 from an embedded `.bax` fixture — together with the **effect-parity** work (feature 008) that brings the renderer up to BeatBax spec-112 WAV/Desktop semantics. 004 depends on 008 (the streaming renderer builds on the parity renderer), so they land together.

## Feature 008 — effect parity

- Named effects: **vibrato** (4 waveforms + duration/delay), **bend** (fractional semitones + exp/log/linear/sine), **arpeggio**, **portamento** (pulse exponential / wave linear+quantized), **sweep**.
- **Band-limited pulse** — Nyquist-capped harmonic tables per duty (`gb_pulse_tables.h`).
- Inline `<vib>`/`<bend>`/`<arp>`/`<port>` auto-registered in the song model.
- Out-of-scope effect tokens are **refused with named reasons** at the renderer (parser keeps round-trip fidelity).

## Feature 004 — device player

- Float-free **streaming renderer** (`gb_stream`) + build-time **compiled blob** (`gb_embed`) — no heap, no float, no `.bax` parser on device.
- Firmware `player.c`: PLAY transport, **MASTER log taper** (−40 dB → 0 dB), graphical status screen with the **song name rendered** (`GRASSLAND DASH`).
- Renderer call graph placed in `.ram_text` (SRAM); local `__udivdi3` for the missing 64-bit division helper.
- Default embedded song: `grassland_dash.bax`.

## Verification

- `make test` — green (26 supported / 3 refused / 0 mismatches; all voice/note/song/stream render tests pass).
- `make test-compare` — all 8 fixtures ≥ 0.95 vs BeatBax CLI:

| Fixture | corr |
|---|---|
| gb_uge_note_demo | 1.0000 |
| gb_sweep_demo | 0.9991 |
| effects/gb_sweep_demo | 0.9983 |
| gb_arpeggio_demo | 0.9982 |
| tutorial_groove | 0.9939 |
| gb_vibrato_demo | 0.9886 |
| gb_portamento_demo | 0.9781 |
| gb_bend_demo | 0.9678 |

- Firmware renders the embedded song **byte-identical** to the host renderer (correlation 1.0).
- **Callback budget on hardware: 1409 µs** = 48.6% of the 2901 µs deadline; `.ram_text` ≈ 12.7 KB of the 24 KB budget.
- Hardware checklist `docs/hardware/checklist-004.md` **signed off** (all 19 rows pass).

## Notes / review pointers

- 004 and 008 share the renderer call graph (`host/render/gameboy/`), so the split here is by commit (`abe7f46` = 008, `6636d6f`/`9b579d0` = 004) rather than by file.
- `cef44d1` is a small cross-cutting fix: subprocess output decoded as UTF-8 to silence a non-fatal `UnicodeDecodeError` on Windows.
- No `.bax` syntax invented; the compiled blob is a 1:1 lowering of the existing song model.

Closes #4 (device player) and the 008 effect-parity work.
