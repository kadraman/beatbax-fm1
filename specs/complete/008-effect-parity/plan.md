# 008 — Named-effect parity — plan

## Approach

The renderer's sample loop (`gb_note_render.c`) is already fixed-point and bit-exact across
host/firmware. The gaps are in **lowering** (host-side `song_parse.c` / `gb_note_desc.c`) and
a few **fixed-point render additions**. Strategy:

1. Make the parser capture the full effect surface (inline expressions, vib waveform/duration/
   delay, bend fractional semitones) and **refuse** out-of-scope tokens with a named reason.
2. Lower those into `gb_note_desc_t` → `gb_song_fx_t` blob (host-side float allowed), keeping
   the **render loop** float-free. `rows → ms` uses `tickMs = 60000/bpm/4` at lower time; the
   renderer converts `ms → samples` from `sample_rate` (same as `bend_delay_ms` today).
3. Add the fixed-point render branches (vib waveform/duration/delay, bend fractional Q16) in
   `gb_note_render.c`, preserving bit-identity between streaming and offline.
4. Wire the per-effect demos into `make test-compare` and add a non-trivial assertion per effect.

## Constitution Check

- **I. Specs first** — no new `.bax` syntax; semantics copied from BeatBax `modulation.ts`
  (spec-112) and `pcmRenderer.ts`. ✓
- **II. Production quality** — fixed-point render, float only host-side (lowering). ✓
- **III. Song model stable core** — parser/model changes are additive (new vib fields, inline
  effect registration); canonical `.bax` write updated in tandem. ✓
- **IV. BeatBax compatibility** — unsupported tokens refused with named reasons (fixes the
  silent-ignore anti-pattern). ✓
- **V. Chip voice isolation** — no change to pattern/sequence meaning. ✓
- **VI. Determinism** — all new math is integer/fixed-point and documented. ✓
- **VII. Tests back behavior** — per-effect fixtures + non-trivial assertions + refusal tests. ✓
- **VIII. Audio budget** — vib waveform is a table/shape lookup (no per-sample division);
  re-measure callback after each render change. ✓
- **IX. Licensing** — host changes only (Apache); firmware render additions are float-free. ✓
- **X. Agent constraints** — follow the 008 spec verbatim. ✓

## Key semantics (from BeatBax `packages/engine/src/effects/modulation.ts`)

- `tickSec = (60 / bpm) / 4` (`pcmRenderer.ts`: `tickSeconds = secondsPerBeat / 4`).
- Vibrato LFO amplitude = `clamp(depth,0,15) × baseFreq² / 131072` (our Q16 code already
  matches this: `amp = (f2>>33)×depth`, `freq += (s×amp)>>15`).
- Vibrato is active only when `t ≥ vibDelay` AND (`vibDuration` unset OR `t < vibDuration`).
- Bend delay/time are **seconds** (not fractions/beats), defaults `durSec/2` and remainder.
- Arp frame `= floor(t×60 + 1e-9)`, offsets `[0, …]`.

## Files touched

- `host/song/song_model.h/.c` — extend `bax_effect_t.params.vib` (waveform/duration/delay rows),
  add inline-effect registration, add refusal plumbing (`bax_error_t` with named reason).
- `host/song/song_parse.c` — parse all 5 vib params, bend fractional + full curve/delay/time,
  inline `<…>` effect expressions, refuse `trem`/`volSlide`/`cut`/`retrig`/`echo`.
- `host/song/song_write.c` — canonical write of the new fields + inline effects.
- `host/render/gameboy/gb_note_desc.c` — lower vib waveform/duration/delay (`rows→ms`) and bend
  fractional (`semitones→Q16`).
- `host/render/gameboy/gb_note_render.h/.c` — descriptor + runtime fields, vib waveform/
  duration/delay and bend fractional fixed-point render.
- `host/render/gameboy/gb_stream.h` — blob `gb_song_fx_t` fields + version bump.
- `host/render/gameboy/gb_embed.c` — serialize new fields.
- `host/render/gameboy/gb_stream.c` — load/validate new fields.
- `host/tests/render/test_song_render.c` (and/or new `test_effects.c`) — per-effect assertions.
- `tools/run-compare.py` — add in-scope effect demos to the default set.
- `specs/STATUS.md` — register feature 008.

## Testing

- Unit: parse each effect form (named + inline, all vib waveforms, bend fractional), assert
  lowered descriptor fields; assert out-of-scope tokens are refused.
- Per-effect non-trivial assertions (arp changes pitch in steps, bend glides, vib modulates
  pitch, port glides, sweep shifts).
- `make test-compare` with the 5 effect demos ≥ 0.95.
- `make test` bit-identity unchanged; firmware emulator correlation 1.0 on a full-effects song;
  re-measure callback timing.

## Risks

- Blob format change is a version bump; old embedded blobs must refuse to load (named reason).
- Fixed-point vib triangle/saw must match `lfoSample` within the 0.95 tolerance (shapes, not
  float-exact).
- Inline-effect registration must not change named-effect dedup or canonical write idempotence.
