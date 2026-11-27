# 008 — Named-effect parity — tasks

## Phase 1 — Parser / model completeness (host only)

- [x] T1 Refuse out-of-scope tokens with a named reason: the **parser** keeps
      `BAX_EFFECT_OTHER` for round-trip fidelity (feature 002), and the **renderer**
      (`gb_expand.c` `gb_song_resolve`) refuses a note-level reference to an out-of-scope
      effect (`trem`/`volSlide`/`cut`/`retrig`/`echo`/`panning`/…) as
      `unsupported effect: <token>`.
      _Note (discovered): sequence-level `pattern:effect` references (e.g.
      `seq s1 = melody:ambient`) and inline `<echo:…>` raw expressions are dropped before
      this check; tracked under T2/T18._
- [x] T2 Inline effect expressions: register `<vib:…>` / `<bend:…>` / `<arp:…>` / `<port:…>`
      from note tokens into the song effect list (dedup by raw expr, `is_inline` flag), so
      `bax_song_find_effect` resolves them like named effects. Canonical write skips inline
      effects (they're written at the note). Verified bit-identical to named effects.
- [x] T3 Vibrato full params: added `duration_rows`/`delay_rows` to `bax_effect_t.params.vib`;
      `parse_effect_expr` parses `vib:depth,rate[,waveform[,duration[,delay]]]` (waveform is now
      the single token, not `"sine,2,1"`).
- [x] T4 Bend fractional semitones: parse `bend:semitones[,curve[,delay[,time]]]` with
      fractional `semitones` (stored as Q16 `bend_semi_q16`). Also fixed: bend `delay`/`time`
      are now clamped to `[0,durSec]` / `(0,durSec−delay]` like BeatBax.
- [x] T5 Canonical write: `host/song/song_write.c` skips `is_inline` effects and preserves
      named-effect `raw_expr` verbatim; `test_roundtrip` green.

## Phase 2 — Lowering + blob format

- [x] T6 `gb_note_desc.c`: lower vib `waveform` (→ `GB_VIB_*` byte) + `duration_rows`/`delay_rows`
      → `ms` (`tickMs = 15000/bpm`, float host-side) and bend fractional → `bend_semi_q16`.
- [x] T7 `gb_note_render.h`: added `vib_waveform`/`vib_duration_ms`/`vib_delay_ms`/
      `bend_semi_q16` to `gb_note_desc_t` (replacing `bend_semi`).
- [x] T8 `gb_stream.h`/`gb_embed.c`/`gb_stream.c`: serialize + load the new fields; blob
      `version` 2→4; refuse mismatched version with a named reason.
- [x] T9 `gb_note_render.c` `gb_note_begin_desc`: derives `vib_duration_samples`/
      `vib_delay_samples` and `bend_semi_q16` state.

## Phase 3 — Fixed-point render

- [x] T10 Vibrato waveforms: `gb_lfo_q15(phase, waveform)` (sine/square/saw/triangle,
      phase 0 = square high / triangle −1 / sine 0) in `pulse_step`.
- [x] T11 Vibrato duration + onset delay: LFO (and its phase advance) gated on
      `t ≥ vib_delay` and `t < vib_duration`, matching BeatBax `modulatePitchHz`.
- [x] T12 Bend fractional: `semi_q16 = (bend_semi_q16 × p_q16) >> 16`; bend delay/time clamped.
- [x] T13 Arp 0.9338: root cause was note-start **truncation**, not the arp rate. Fixed
      `step_to_sample` to `Math.round(time×sr)` (floor(x+0.5), exact for odd BPM) and
      `dur_to_samples` to `round((step+dur)×…) − round(step×…)`, matching `pcmRenderer`.
      arp demo 0.9338 → **0.9575**; `gb_uge_note_demo` 0.9506 → **1.0000**.

## Phase 4 — Tests + wiring

- [x] T14 Add per-effect non-trivial assertions: `test_song_render.c` now has `test_arp`
      (pitch cycles), `test_bend` (pitch glides), `test_vibrato` (pitch modulates); the refusal
      of out-of-scope tokens is covered by `test-coverage` (`bax-coverage.json` refuse cases).
- [x] T15 Wire in-scope effect demos into `tools/run-compare.py` defaults: added
      `gb_arpeggio_demo`, `gb_vibrato_demo`, `gb_sweep_demo`, and a new clean `gb_bend_demo`
      (exp/log/linear/sine + fractional). All ≥ 0.95.
- [x] T16 Full regression: `make test`, `make test-compare`, `make package`; firmware emulator
      correlation 1.0 on a full-effects song.

## Phase 5 — Follow-ups (discovered during Phase 1)

- [x] T17 Bend `sine` curve: replaced smoothstep `3p²−2p³` with BeatBax's `(1−cos(πp))/2`
      (`cos(πp) = sin(π(p+0.5))` via the sine table with 9-bit linear interpolation).
- [x] T18 Sequence-level `pattern:effect` application (`seq s = melody:effect`): added a
      `bax_seq_item_t.effect` field, parsed `:effect`, and applied it in `expand_pattern` as a
      per-note fallback (only to notes without their own effect), matching BeatBax
      `mergeEffectsIntoToken` for the demo's same-type cases. Vibrato demo 0.79 → 0.93.
- [x] T19 `gb_portamento_demo.bax` **resolved (0.9781).** Three parts: (a) channel-ref
      `seq*N` repetition and `seq:oct(-2)` transposition (`parse_channel_ref`); (b) the wave path
      phase **accumulator** + 128-frame-quantum stepped portamento; (c) the wave instrument's
      default volume was 0 (mute) but BeatBax defaults to 100% — fixed `bax_song_add_instrument`
      to default `wave_volume = 100` (canonical write omits the 100 default).
- [x] T22 Pulse portamento glide: root cause was BeatBax's resolver marking every port note
      `legato`. Fixed (a) phase/vibrato continuation for `join || port_speed > 0`; (b) the 440 Hz
      `chrome_default` startup skipped for legato notes; (c) envelope sustain at the previous
      note's final level. Then replaced the ideal square with a **band-limited pulse** (Nyquist-
      capped Fourier series, precomputed 84-harmonic tables per duty) matching BeatBax
      `pulseDc.ts`. Single glide 0.09 → 0.99; run 0.89 → 0.94; demo 0.5174 → 0.9781.
- [x] T20 Wave-channel vibrato: added the same additive LFO (waveform/duration/delay-gated,
      keyed off the un-arpeggiated base) to `wave_step` between the portamento and the phase
      accumulator. Wave vibrato (all 4 waveforms) now correlates 0.985 vs BeatBax.
- [x] T21 Residual 1-sample offset: root cause was the **pulse phase order** — the renderer
      advanced phase *before* sampling (`phase += inc; bip = bipolar(phase)`) while BeatBax
      samples *then* advances (`sample = f(phase); phase += oscFreq/sr`), shifting every pulse
      note by one increment. Reordered to sample-then-advance (phase save still after advance).
      Existing fixtures: tutorial_groove 0.9582→**0.9830**, gb_sweep 0.9891→**0.9985**.

## Acceptance

- All effect demos ≥ 0.95 vs BeatBax CLI.
- Out-of-scope tokens refused with named reason.
- `make test` + `make package` green; firmware render bit-identical and in budget.
- `specs/STATUS.md` updated; `docs/` updated if behavior is user-visible.
