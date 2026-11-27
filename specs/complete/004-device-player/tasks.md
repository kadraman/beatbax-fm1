# Tasks: Device player

**Input**: [spec.md](spec.md), [plan.md](plan.md)

## Phase 0: Foundation (blocks stories)

- [x] T001 Confirm features 001 and 003 are at least `planned` before starting implementation
- [x] T002 Add `docs/hardware/checklist-004.md` template
- [x] T003 Document fixture + embed method in `firmware/README.md`

**Checkpoint**: fixture (`tutorial_groove.bax`) chosen; checklist ready.

## Phase 1: User Story 1 — Play fixture (P1)

- [x] T010 Refactor `gb_note_render.{h,c}` into a float-free stateful
      `gb_note_begin_desc`/`gb_note_step` (resolved `gb_note_desc_t`), keep
      `gb_render_note_full` bit-identical via `gb_note_desc.c`
- [x] T011 Add `gb_stream.{h,c}` compiled-song scheduler + `gb_stream_render_block`
      (mixes active channels, baked scale + clamp, no heap/float)
- [x] T012 Add `gb_embed.{h,c}` (host) to build/validate the compiled blob
      (lower instruments/effects, expand events, adjust noise length, bake peak scale)
- [x] T013 Add `host/tests/render/test_stream.c`: bit-identity vs offline render,
      block-size independence, stop→reset→replay
- [x] T014 Add `tools/embed-bax.c` CLI; generate + commit `firmware/app/gb_song_embed.h`
      for `tutorial_groove.bax` at 44118 Hz
- [x] T015 Write `firmware/app/player.c`: compile shared renderer into firmware,
      SRAM `player_fill`, PLAY toggle, MASTER gain, minimal status screen, main-loop poll
- [x] T016 Update `firmware/app/build.sh` (+ Makefile host target) for player sources

**Checkpoint**: host bit-identity green; firmware source written. On-device
play/stop still needs the JieLi build + flash (checklist rows 4–10).

## Phase 2: User Story 2 — Budgets (P1)

- [ ] T020 Instrument callback with `fm1_ticks`; record results in checklist
      _(emulator measured 672 µs = 23% of the 2901 µs deadline; the spec's
      "on maintainer hardware" figure is still pending — checklist row 11)_
- [x] T021 Audit callback for flash/draw/USB waits: `fm1_alnk0_irq` → `player_fill` →
      `gb_stream_render_block` → `gb_note_step` call only audio-HAL + fixed-point renderer
      code. No flash erase/write (only XIP **reads** of the `const` blob; `FL_FAR` is a far-call
      via `fl_far`, not an erase), no LCD draw, no USB wait, no heap, no float. Recorded in
      `docs/hardware/checklist-004.md` row 12.
- [ ] T022 Run soak test and file results in `docs/hardware/checklist-004.md`
      _(requires real hardware — pending)_

## Phase 3: Fidelity — per-sample output (wave scale, envelope)

- [x] T030 Apply wave output scale ×0.54 (0.9 buffer × 0.6 playback gain) in
      `gb_wave_sample_q15`; keep host bit-identity
- [x] T031 Rewrite `gb_envelope_q15` to linear-interpolate between GB envelope steps
      (saturate + hold last level), matching BeatBax `getEnvelopeValue`
- [x] T032 Host unit test: wave scale + envelope interpolation golden samples

## Phase 4: Fidelity — note release tails

- [x] T040 Add `GB_NOTE_RELEASE_SEC` / `WAVE_TAIL_SEC` constants + a float-free
      `exp` decay table for the wave exponential release
- [x] T041 Pulse/noise: render `len + 5 ms` with a linear release ramp; freeze the
      envelope at the note end
- [x] T042 Wave: 5 ms linear (GB envelope/program-volume) or 20 ms exponential
      (`setTargetAtTime` shape) release; render `len + tail`
- [x] T043 Extend `gb_note_t`/`gb_note_begin_desc` with render length + tail type
- [x] T044 Stream scheduler: let a note's release tail keep rendering past the next
      note-on on the same channel (brief overlap), preserving block-size independence
- [x] T045 `gb_embed`/`gb_song_render`: account for tails in event length + peak scale

## Phase 5: Fidelity — pitch modulation (portamento curve + effect order)

- [x] T050 Pulse portamento → exponential (log-freq) glide; wave → linear (Hz) glide;
      drop smoothstep
- [x] T051 Reorder modulation to port → arp → bend → vib; vib additive keyed off the
      un-arpeggiated base frequency (captured at note begin)
- [x] T052 Host unit test: portamento curve + vib/arp order golden check

## Phase 6: Fidelity — pulse portamento voice-holding

- [x] T060 `gb_expand`/`gb_embed`: detect contiguous pulse `port` runs; tag events
      `join`/`hold`; bump `GB_BLOB_VERSION`
- [x] T061 `gb_note_begin_desc` + `pulse_step`: `join` → continue phase/vibrato;
      `hold`/`join` → keep opening envelope + skip mid-run release fade
- [x] T062 Stream scheduler + blob: carry `join`/`hold` flags; run renders as one voice
- [x] T063 Host unit test: portamento-run envelope continuity + bit-identity

## Phase 7: Fidelity — verify + docs + regenerate

- [x] T070 Re-run host bit-identity (stream == offline) and BeatBax correlation;
      record the new correlation
- [x] T071 Implement + document the Chromium 440 Hz pulse startup (phase advance)
      in the renderer + `docs/renderer.md`
- [x] T072 Add a spec-112 parity erratum to
      `specs/complete/003-gameboy-renderer/research.md`
- [x] T073 Regenerate + commit `firmware/app/gb_song_embed.h`; rebuild firmware in WSL;
      refresh `docs/hardware/checklist-004.md` rows
      _(regenerated from `grassland_dash.bax`, rebuilt clean; checklist rows 1–19 signed off)_

## Polish

- [x] T900 Update root README status for the player milestone
- [x] T901 Move this folder to `specs/complete/004-device-player/` and update
      `specs/STATUS.md` when shipped
