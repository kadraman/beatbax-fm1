# Implementation Plan: Device player

**Spec**: [spec.md](spec.md) | **Date**: 2026-11-22 | **Branch**: `feat/004-device-player`

## Summary

Run the feature 003 Game Boy renderer on the FM-1 in SRAM. The core new work is a
**streaming block renderer** — feature 003 shipped an *offline* renderer that allocates
a whole-song buffer and applies a global peak-limit pass, neither of which fits the
device. We build a float-free, allocation-free block renderer on the shared fixed-point
voice/effect primitives, embed one fixture as a build-time **compiled song**, and wire
PLAY / MASTER / a minimal status screen into the 001 bring-up base.

A follow-on **renderer-fidelity update** (see below) brings the shared fixed-point
core up to BeatBax's spec-112 WAV/Desktop parity semantics so device output matches
the current `beatbax export wav` / Desktop playback. It preserves bit-identity between
the offline and streaming renderers and the baked-scale contract.

## Resolved implementation decisions

These answer the spec's open questions and close the design forks for 004:

1. **Build-time compiled format (no on-device parser/allocator).** A host tool parses the
   `.bax` (reusing the 002 parser), expands it into an event schedule, lowers instruments
   and effects to fixed-point descriptors, and emits a compact blob. The firmware has no
   `.bax` parser, no `malloc`, and no float. Rationale: 96 KB SRAM, deterministic, and the
   blob is verifiable 1:1 against the offline renderer on the host. (On-device `.bax`
   parse/write is a feature 006 concern, not 004.)
2. **Bit-identical output via a baked peak scale.** The offline renderer applies a global
   `0.95` peak ceiling. Streaming cannot scan the whole song at runtime, so the host tool
   computes the exact peak scale and embeds it. The device streamer applies the same scale,
   making its output sample-identical to the host renderer at the **same rate**.
3. **Fixture and rate.** Embed `songs/gameboy/tutorial_groove.bax` (1.7 KB, host CLI
   correlation 0.970). The blob is compiled at the integer device rate **44118 Hz**
   (`GB_RENDER_DEVICE_RATE`; the true codec rate is 24 MHz / 544 = 44117.647… Hz), so
   the device renders sample-for-sample what the host renderer produces at 44118 Hz.
   Host↔CLI correctness at 44100 Hz is already proven by feature 003.
4. **Transport.** PLAY (matrix button 12) toggles start/stop; stopping resets the playhead
   to the beginning. At end of song the transport returns to idle (no loop).
5. **Output path.** The renderer emits mono `int16` Q15. The callback left-shifts by 8 to
   the codec's Q23 `int32` and duplicates to L/R. MASTER (ADC) applies a post-mix gain.

## Renderer fidelity update — match BeatBax spec-112 WAV/Desktop parity (2026-11-22)

The shared renderer (`host/render/gameboy/`) was built against the pre-spec-112 BeatBax
PCM renderer. BeatBax's parity work (`bd57ae7d` … `a34cdac1`, feature 112 in
`beatbax`) changed the authoritative `renderSongToPCM` (WAV export / CLI play)
semantics. This section updates our float-free core to match, keeping bit-identity
between the offline (`gb_song_render`) and streaming (`gb_stream`) paths and the baked
peak-scale contract. Reference commits: `c6ccbe72` (release tails), `08b42b74`
(vibrato/portamento/limiter), `a34cdac1` (portamento voice-holding, volSlide),
`d3807ca6` (Chromium startup — skipped, see §7).

### 1. Wave output scale (pre-existing gap, in scope)

BeatBax renders the wave channel as `((table[i] − mean)/15) × 0.9 × 0.6 × volMul`
(`WAVE_OUTPUT_SCALE = 0.9` buffer fill × `WAVE_PLAYBACK_GAIN = 0.6`). Our
`gb_wave_sample_q15` returns `(table[i] − mean)/15` divided by `vol_div` — i.e. the
`×0.54` net scale is missing, so the wave is ~5.3 dB too loud in the mix.

- Apply a fixed Q15 scale `0.54` (`0.9 × 0.6`, ≈ `17695/32768`) to the wave sample,
  before `vol_div`. `vol_div` already equals `1/volMul`; no volume-map change.
- Update the feature-003 wave research note (it omitted both factors).

### 2. GB envelope linear interpolation (pre-existing gap, in scope)

BeatBax `getEnvelopeValue` (GB mode) interpolates **linearly** between volume steps
(matching WebAudio `setValueCurveAtTime`), then holds the last level; our
`gb_envelope_q15` steps discretely. Rewrite `gb_envelope_q15` to:

- `stepPeriod_samples = period × sample_rate / 64`.
- `saturateSteps = (dir == down) ? initial : (15 − initial)`;
  `nSteps = min(saturateSteps, floor(len × 64 / (period × sample_rate)))`.
- For sample `t`, if `t ≥ nSteps × stepPeriod_samples` hold `initial ± nSteps`; else
  `i0 = floor(t / stepPeriod_samples)`, `frac` = remainder, lerp `initial ± i0` →
  `initial ± (i0+1)`.
- Freeze at note end (`min(t, len)`) during release tails (§3).

### 3. Note release tails (`c6ccbe72`, in scope)

Add `GB_NOTE_RELEASE_SEC = 0.005` and `WAVE_TAIL_SEC = 0.02`.

- **Pulse / noise**: render `len + round(5 ms × sr)`; samples past the nominal end ramp
  linearly 1 → 0 over 5 ms (`linearReleaseGain`). Envelope is frozen at the note end.
- **Wave**: if a GB envelope or program-volume is active, use the 5 ms linear release;
  otherwise use the `setTargetAtTime` shape — an exponential fade with time constant
  20 ms that **starts 20 ms before** the nominal end and extends 20 ms past it
  (`waveExponentialReleaseGain`, floor `0.0001/baseGain`), rendering
  `len + round(20 ms × sr)` samples.
- The exponential needs a float-free `exp`: a Q30 recurrence `g ← g · exp(−1/tail)`.
  `tail = round(20 ms × sr)` rounds to 882 samples at both 44100 and 44118 Hz, so a
  single hard-coded Q30 decay constant serves both rates. The floor term is a small
  per-note constant (`0.0001/baseGain`, ~5/11/22 in Q15 for vol 100/50/25).
- The pulse phase (and vibrato LFO phase) is now **local to the note** and saved back
  to the channel state at the *nominal* end (BeatBax `phaseAtNominalEnd`); the release
  tail continues with the local phase. This keeps stream ↔ offline bit-identical when a
  tail overlaps the next note-on.
- Extend `gb_note_t` with `render_len` + tail type; both the offline mixer and the
  stream scheduler must mix the full extended length, including a note's tail ringing
  into the next note-on on the same channel (brief two-note overlap).

### 4. Portamento curve (spec-112 `modulation.ts`, in scope)

BeatBax dropped smoothstep: pulse portamento is an **exponential** (log-frequency
geometric) glide (`exponentialRampToValueAtTime`), wave is **linear** (Hz lerp). Replace
our smoothstep in `pulse_step`/`wave_step` with the matching curve per voice; duration
`(256 − speed)/256 × noteDur × 0.6` is unchanged and already correct.

### 5. Effect order + vibrato base (`modulatePitchHz`, in scope)

BeatBax applies **port → arp → bend → vib**, with vib *additive* and its amplitude
keyed off the **un-arpeggiated base frequency**. Our order is port → vib → arp → bend,
with vib amplitude keyed off the (glided, pre-arp) frequency. Reorder so:

- Capture `base_freq` (post sweep + `pitch_env` offset, pre-portamento) at note begin.
- Apply portamento → arpeggio → bend to `freq`.
- Apply vibrato last: `freq += sin(phase) × amp`, `amp = depth × base_freq² / 131072`
  (the existing bit-exact fixed-point form, but from `base_freq`); advance the LFO phase
  at the same point.

### 6. Pulse portamento voice-holding (`bd57ae7d` + `a34cdac1`, in scope)

Contiguous pulse `port` notes share **one oscillator**: later notes in a run *join*
(continue phase/vibrato, no envelope re-attack) and the opening note *holds* (no release
fade until the run ends). This mirrors `pulsePortamento.ts` (`join`/`hold`/`voiceStopAt`).

- Host event expansion (`gb_expand`/`gb_embed`) detects contiguous pulse `port` chains
  and tags each event `join`/`hold`; these flags are baked into the compiled blob
  (**bump `GB_BLOB_VERSION`**).
- `gb_note_begin_desc` + `pulse_step` honour the flags: `join` → continue phase + vibrato
  from the channel state; `hold`/`join` → keep the envelope at the opening level and skip
  the release tail mid-run; phase handed to the next note is the phase at the nominal end.
- Matches BeatBax `continuePhase = legato || portJoin`: a *normal* pulse note (not a join)
  resets its oscillator + vibrato phase to 0; only a joined voice continues it.

### 7. Chromium 440 Hz startup (`d3807ca6`, in scope)

A new (non-joined) pulse oscillator in Chromium reads its first sub-quantum samples at
the 440 Hz default before the scheduled frequency takes effect. This is not just a
transient — it **advances the oscillator phase** at 440 Hz for those leading samples
(up to 64), so skipping it phase-shifts every pulse note that does not start on a
128-sample boundary. We replicate it: `chromeDefaultHzSamples = min(start%128, 128 −
start%128)` samples of a new pulse voice play (and advance phase) at 440 Hz.

### 8. Band-limited pulse — accepted residual

The parity renderer synthesizes pulse with a band-limited Fourier series (up to 200
harmonics, capped at Nyquist) plus the `2·duty−1` DC offset (`pulseDc.ts`), not a naive
±1 square. Our fixed-point core keeps the naive square wave; the correlation cost is
~2% (single notes ≈ 0.98). This is accepted rather than porting per-sample Fourier
synthesis into the hot path.

### Out of scope (unchanged)

`volSlide`, `trem`, `cut`, `retrig`, `echo` effect tokens remain out of scope (not part
of the 002/003 named-effect surface our renderer supports); the `a34cdac1` volSlide
capping and spec-112 echo/retrig work are therefore not ported. No `.bax` syntax or
song-model change; the blob is a 1:1 lowering of the 002 model with the new flags.

## Technical context

- **Packages / surfaces**: `host/render/gameboy/` (shared, Apache-2.0), `firmware/app/`
  (GPL, links felucca-bsp), `songs/gameboy/`, `tools/`.
- **Languages**: portable C99 for the shared renderer; C for firmware.
- **Testing**: host unit tests (bit-identity, block-size independence, reset); hardware
  checklist `docs/hardware/checklist-004.md`.
- **Client profile**: firmware.
- **Performance / constraints**: callback < half a 256-frame block; hot path in `.ram_text`;
  no heap, no float, no 64-bit division in the sample loop.

## Constitution Check

- [x] **No invented `.bax` syntax** — the compiled blob is a 1:1 lowering of the 002 model;
      no new song features.
- [x] **Song model** — consumed via the 002 parser on the host at build time; unchanged.
- [x] **Chip isolation** — Game Boy only; the blob is Game Boy specific, no song-model change.
- [x] **Determinism** — same blob + same rate → same PCM; proven by a bit-identity test.
- [x] **Audio callback rules** — render runs in SRAM (`.ram_text`), no flash erase / LCD draw /
      USB wait; float-free sample loop.
- [x] **UI graphical** — a status screen (name, playing/stopped, playhead), not a text editor.
- [x] **Licensing** — shared renderer stays Apache-2.0; firmware (GPL) links the same sources;
      no Felucca in host trees.
- [x] **Tests planned** — host bit-identity + hardware checklist 004.

**Exceptions**: none.

## Project structure

```
host/render/gameboy/gb_note_render.{h,c}  # refactored: float-free stateful note step (shared)
host/render/gameboy/gb_note_desc.{h,c}    # host-only: resolve song model -> fixed-point desc
host/render/gameboy/gb_stream.{h,c}       # compiled-song scheduler + block render (shared)
host/render/gameboy/gb_embed.{h,c}        # host-only: build/validate the compiled blob
host/tests/render/test_stream.c           # bit-identity + block-size + reset tests
tools/embed-bax.c                         # CLI: .bax -> C header blob (baked @44117.6)
firmware/app/player.c                     # device player: callback + transport + status UI
firmware/app/gb_song_embed.h              # generated blob for tutorial_groove (committed)
firmware/app/div64.c                      # SRAM __udivdi3 (pi32 has no 64-bit division helper)
firmware/app/compat/string.h              # minimal <string.h> shim (pi32 toolchain has none)
firmware/app/build.sh                     # + player.o, + shared renderer sources, + GB_RAMFN
docs/hardware/checklist-004.md            # timing/soak/audit checklist
```

## Implementation

### Host / renderer (shared, float-free)

- Refactor `gb_note_render.c` so each voice's per-sample loop is a stateful
  `gb_note_step(note, dst, count)` driven by a `gb_note_t`, with `gb_note_begin_desc` and a
  resolved `gb_note_desc_t`. `gb_render_note_full` becomes `begin + step(len)` and stays
  bit-identical. The sample-generation loop becomes float-free (the two float uses — pulse
  duty `*10.0f` and bend `atof` — move to host-side resolution).
- `gb_stream.c` holds the compiled event schedule (sorted by start sample, channel-tagged),
  a per-channel cursor, and up to four active `gb_note_t`s; `gb_stream_render_block` advances
  the playhead and mixes active channels into a mono buffer, applying the baked scale + clamp.
- `gb_embed.c` (host) builds the blob from a parsed `bax_song_t`: lower instruments/effects to
  descriptors (deduplicated), expand to events (reusing the 003 expansion), adjust noise
  hardware length, compute the peak scale, and serialize. A loader validates magic/version.

### Firmware / platform

- Compile the shared sources (`gb_period.c`, `gb_voice.c`, `gb_macro.c`, `gb_note_render.c`,
  `gb_stream.c`, tables) into `player.c`; mark `gb_stream_render_block` and its callees
  `.ram_text` (RAMFN) and call through `FL_FAR` from the ALNK0 IRQ (the proven 001 pattern).
- Replace the test tone with `player_fill`: render 128 frames, convert Q15→Q23, apply MASTER
  gain, duplicate L/R.
- Main loop: PLAY edge toggles transport; draw the status screen (dirty regions only); keep
  the watchdog feed and `usb_poll` cadence from 001.

### Documentation

- `firmware/README.md`: embed method and how to regenerate the blob.
- `docs/renderer.md`: note the device rate (44117.6) and the baked-scale bit-identity contract.

## Testing strategy

### Unit / host

- `test_stream.c`: for `tutorial_groove.bax`, `stream(blob) == offline render` sample-for-sample
  at 44117.6 Hz (bit-identity, the baked-scale contract); block-size independence
  (render in 256 vs 128 vs 1-frame chunks yields identical output); stop→reset→replay produces
  the same bytes as a fresh render.

### Integration / CLI comparison

- Feature 003 already compares the host renderer to the BeatBax CLI at 44100 Hz; 004 reuses
  `tutorial_groove` from that set. No new CLI work.

### Hardware / QA

- `checklist-004.md`: boot, PLAY start/stop, MASTER level, song-name/status shown, callback
  timing under half a 256-frame block, no-flash/draw/USB audit, 10-minute soak. Signed off
  before `complete`.

## Migration and compatibility

- No song-model or `.bax` change. The blob format is versioned (magic + version); a mismatch
  refuses to load with a named reason.

## Open implementation questions

- Whether to keep `gb_render_block`/`gb_render_state_t` (the Phase-0 silence stub from 003)
  or retire it in favor of `gb_stream`. Plan: leave the stub untouched this phase and point its
  doc comment at `gb_stream`; retire it only if a later feature removes the Phase-0 test.
- Exact MASTER gain curve (linear ADC → gain vs. dB) is a UI nicety; plan uses a simple
  linear `0..1` gain on the Q23 output, refined if the hardware checklist flags it.
  **Resolved:** the hardware checklist flagged the linear taper (full knob = deafening, half
  knob = only −6 dB, all usable range crammed into the last few % of rotation). The MASTER knob
  now uses an **audio (log) taper**: a 128-entry `master_taper[]` Q15 table (index = `knob >> 3`,
  `knob = 0` mutes) that maps −40 dB → 0 dB linear-in-dB. Half knob ≈ −20 dB. See
  `firmware/app/player.c`.

## Notes from the actual firmware build (resolved)

- **SRAM placement** uses a shared `GB_RAMFN` macro (empty on host) that the firmware build
  defines to `.ram_text`. The whole renderer call graph is annotated so the pi32 `call` range
  never has to span the SRAM↔XIP gap (the 001 tone-fill root cause). `.ram_text` comes to
  ~10.5 KB of the 24 KB budget.
- **`<string.h>`** is absent from the pi32 toolchain; a `compat/string.h` shim declares
  `memset`/`memcpy`/`memcmp` (implemented by felucca `libc.c`). The shared renderer no longer
  uses `strcmp`/`strlen`.
- **64-bit division** (`uint64_t / uint64_t`) has no toolchain helper, so `div64.c` provides a
  SRAM `__udivdi3`. The hot loop still divides by the runtime `sample_rate`; the callback
  budget must be measured (checklist row 11), and a reciprocal-multiply optimization is the
  fallback if restoring division is too slow.
