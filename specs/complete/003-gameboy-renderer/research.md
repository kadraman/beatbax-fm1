# Research: BeatBax Game Boy (DMG-01) rendering semantics

Reference for feature 003. Confirmed by reading the BeatBax engine source
(`C:\Users\kadraman\repos\beatbax\packages\engine\src`, package `@beatbax/engine`,
the compatibility oracle) and the installed CLI `beatbax` **0.6.1**.

The WAV oracle is the **offline PCM renderer** (`audio/pcmRenderer.ts`), which is what
`beatbax export wav` drives. The WebAudio path (`chips/gameboy/pulse.ts` etc.) shares
the same math and is the reference where the PCM renderer delegates.

## 1. Note → frequency → period

`chips/gameboy/apu.ts`:

- `noteNameToMidi(note, octave)` → `midi = (octave + 1) * 12 + semi`, with `C=0 … B=11`,
  `#` sharp and `B` flat (`Db`→1, `Eb`→3, `Gb`→6, `Ab`→8, `Bb`→10).
- `midiToFreq(midi)`:
  - `index = midi - 36`; if `0 ≤ index < 72`, return `freqFromRegister(GB_PERIOD_TABLE[index])`.
  - else fall back to equal temperament `440 * 2^((midi-69)/12)`.
- `GB_PERIOD_TABLE` (72 entries, index 0 = C2 = MIDI 36), in
  `chips/gameboy/periodTables.ts`. The renderer aligns every note through
  `freqFromRegister(registerFromFreq(freq))`, so in practice note → period register
  → `freq = 131072 / (2048 - reg)`.

`chips/gameboy/periodTables.ts`:

- `GB_CLOCK = 4194304` Hz.
- `freqFromRegister(reg) = 131072 / (2048 - clamp(reg, 0, 2047))`.
- `registerFromFreq(freq) = round(2048 - 131072 / freq)`, clamped `0..2047`.

## 2. Pulse channels (1 & 2)

Source: `chips/gameboy/pulse.ts`, `audio/pcmRenderer.ts` (`renderPulse`).

- **Duty**: fraction `12.5% → 0.125`, `25% → 0.25`, `50% → 0.5`, `75% → 0.75`
  (`duty > 1 ? duty/100 : duty`). Square wave: per sample `phase += effFreq/sr`,
  `phase %= 1`, `sample = phase < duty ? +1 : -1` (bipolar).
- **Output gain** `PULSE_OUTPUT_GAIN = 0.25` (applied as `sample *= 0.25`).
- **Envelope** (GB): `stepPeriod = period * (1/64)` s; volume steps `±1` per step,
  clamped `0..15`, starting at `initial`; direction `up` → `min(15, initial+step)`,
  `down` → `max(0, initial-step)`; `flat`/`period=0` → constant `initial/15`.
  Sample gain = `volume/15` (PCM `getEnvelopeValue`).
- **Sweep** (pulse 1 only, instrument `sweep=`): interval `sweep.time/128` s;
  each step `delta = currentReg >> shift`; `up` → `reg += delta`, `down` → `reg -= delta`;
  clamp `0..2047`; if `reg > 2047` → silence. Recomputes `freqFromRegister(reg)`.
  (PCM `renderPulse` line ~934.)

## 3. Wave channel

Source: `chips/gameboy/wave.ts`, `audio/pcmRenderer.ts` (`renderWave`).

- 32 × 4-bit samples; short tables tile to 32; 32-nibble hex string parsed directly.
- **AC coupling**: `mean = sum/32`; sample value `(table[i] - mean) / 15`.
- Zero-order hold (staircase, no interpolation): `tablePos = phase * 32`,
  `i = floor(tablePos) % 32`.
- **Volume** (`volume=`/`vol=`): `{0: 0, 25: 0.25, 50: 0.5, 100: 1.0}` (`volMul`).
- Sample gain = `(table[i] - mean)/15 * volMul`. No separate envelope in the GB path
  (wave has no envelope register; `length` counter is ignored in the PCM renderer).

## 4. Noise channel

Source: `chips/gameboy/noiseNote.ts`, `audio/pcmRenderer.ts` (`renderNoise`).

- **LFSR** (`stepGameBoyLfsr`): 15-bit (`mask 0x4000`) or 7-bit (`mask 0x4040`);
  feedback `newHighBit = ((lfsr ^ (lfsr>>1)) ^ 1) & 1`; `next = (lfsr>>1) & 0x7fff`
  then set/clear the high-bit mask. Trigger resets to 0 then steps until bit 0 is set.
- **Output**: `gameBoyNoiseSample(lfsr) = (lfsr & 1) ? +1 : -1`, scaled by
  `NOISE_OUTPUT_GAIN = 0.25`.
- **Clock** (`resolveNoiseClock`): priority `divisor`/`shift` instrument override →
  `uge_note` → `getNotePoly(noteIndex)` → defaults `divisor=3, shift=4`.
  `getNotePoly` is the hUGEDriver `get_note_poly` port (NR43 bits).
  LFSR rate `noiseClockToLfsrHz(shift, divisor) = GB_CLOCK / (ratio * 2^shift)`
  where `ratio = (divisor==0) ? 0.5 : divisor*16`.
- **Width**: `width=7` → NR43 bit 3 set (7-bit LFSR), else 15-bit.
- **Envelope**: same GB envelope as pulse (`getEnvelopeValue`).

## 5. Software macros (`vol_env`, `pitch_env`, `duty_env`, `arp_env`)

Source: `chips/gameboy/instrumentProgram.ts`.

- Macros lower to a **tick program** advanced at **60 Hz** (`HUGE_TICK_SEC = 1/60`),
  independent of the song tick rate. In `renderPulse`/`renderWave`/`renderNoise`, the
  program row is selected by `tick = floor(t * 60)` with `t` = note-local time.
- `pitch_env` / `arp_env` → semitone offset per tick (`offset`), applied as
  `freq * 2^(offset/12)` (clamped offset −36…+36). `pitch_env` wins over `arp_env`.
- `vol_env` → volume 0–15 per tick (`volScale = v/15`), replacing the envelope.
- `duty_env` → duty index 0–3 per tick (→ 0.125/0.25/0.5/0.75).
- Loop point (`loopPoint >= 0`) → jump to loop row; otherwise a silence/halt row is
  appended. `subpat` (native subpattern) wins over macros when present.

## 6. Named effects (`vib`, `arp`, `port`, `bend`)

Source: `audio/pcmRenderer.ts` (`renderPulse`), `audio/inlineMacroEffects.ts`,
`effects/index.ts`, `util/arpOffsets.ts`.

- **vib** (`vib:depth,rate[,waveform[,durRows]]`): per-channel LFO phase; GB-accurate
  deviation `amplitudeHz = trackerDepth * effFreq^2 / 131072`, `trackerDepth =
  clamp(round(depth), 0, 15)`; `effFreq += sin(phase) * amplitudeHz`; phase advances
  `2π * rate / sr`. (A discrete hUGEDriver step variant exists for the wave path.)
- **arp** (`arp:semi1,semi2[,semi3]`): cycle at **60 Hz** (Game Boy); offsets
  `arpCycleOffsets` (leading 0 root); `effFreq *= 2^(offset/12)`.
- **port** (`port:speed`): smoothstep glide from the previous note's frequency on the
  same channel to the current frequency; duration derived from speed or explicit rows.
- **bend** (`bend:semitones[,curve[,delay[,time]]]`): `effFreq *= 2^(semi*progress/12)`
  with progress shaped by `curve` (`linear`/`exp`→x²/`log`→1−(1−x)²/`sine`).

## 7. Mixer / output

- Summed additively into a mono (or interleaved stereo with `panToGains`) buffer.
- **Pan**: center `L=R=1`; hard L/R and in-between use equal-power
  `cos`/`sin` of `(p+1)/2 * π/2`. Instrument `gb:pan` / `pan` or event pan.
- `normalize=false` on export → only peak-limited if clipping (`peakLimitForPlayback`).

## 8. Sample rate

- `sampleRate` defaults to 44100; phase accumulator `phase += effFreq/sampleRate`
  (float). The device target 44117.6 Hz is a rate-parameterization concern for 004.

## 9. Scheduler / expansion

`expand/compositionExpand.ts`, `patterns/expand.ts`, `sequences/expand.ts`,
`refExpander.ts`, `song/resolver.ts` lower the song into a **flat per-channel
`events[]` array** (one event per step). The PCM renderer consumes it with
`time = stepIndex * tickSeconds`.

- **Tick/step grid**: one step = one sixteenth note = 1/4 beat:
  `tickSeconds = (60 / bpm) / 4`. BPM default **128**. `stepsPerBar` is metadata
  only (bar numbering), not sample timing. Channel-level `speed` is ignored by the
  PCM/WAV renderer.
- **Note-on timing**: `startSample = floor(time * sampleRate)`; the note starts at
  the exact step boundary (sample-accurate).
- **Duration**: a note token lasts `1 + sustainCount` steps (lookahead over `_`/`-`),
  or an explicit `:N` duration (N grid cells). `dur = tickSeconds * (1 + sustain)`.
- **Repeats**: token `*N` and group `( ... )*N` expand inline; sequence item
  `name*N` and `(A B)*N` likewise. `:N` wins over `*N` if both present.
- **Sequence modifiers**: `oct`, `rot`, `rev`, `pal`, `slow`, `fast`, `arp`,
  `clamp`, `fold`, `mute`, `inst`, `pan`, `semitone`/`st`/`trans`/`+N`/`-N`,
  `invert`, `every`, `off`, `pick`, `chunk`, `shuffle`. Transposition is applied
  last; `inst(name)` emits an inline `inst` token; `pan(v)` wraps the token.
- **Composition**: sections generate synthetic seqs `<section>_<role>`; `form` +
  channel `role` bind a channel to a timeline of section seqs; `channel auto form`
  (Beginner Mode) expands to 4 concrete channels (`mel harm bass perc`).
- **Phase continuity**: pulse phase persists across notes per channel
  (`channelPhaseState`); wave phase resets per note (`(t*freq)%1`); noise LFSR
  resets per note (`triggerGameBoyLfsr`).

## 10. WAV output quantization

`export/wavWriter.ts`: interleaved 16-bit signed PCM, little-endian, **forced
2-channel**. `floatTo16BitPCM`: clamp to `[-1,1]`, then
`s = s < 0 ? s*0x8000 : s*0x7FFF` (asymmetric), then **floor**. Our fixed-point
renderer targets this mono/stereo float buffer shape; the comparison harness
downmixes the reference stereo to mono before correlation, so only the waveform
shape (not the exact int16 rounding) must match.

## Scope freeze (feature 003)

In scope (002 round-trips these): pulse 1/2, wave, noise voices; duty, envelope,
sweep (NR10, pulse 1); wave table + volume; noise width + note clock; software macros
`vol_env`/`pitch_env`/`duty_env`/`arp_env`; named effects `vib`/`arp`/`port`/`bend`.
Out of scope for 003: NES/other chips, `trem`/`volSlide`/`cut`/`sweep` effect tokens
(not in the 002-documented named-effect surface), on-device integration.

## Erratum — superseded by spec-112 parity (2026-11-23)

This file froze the pre-spec-112 BeatBax semantics (CLI 0.6.1). BeatBax's spec-112
WAV/Desktop parity work (`bd57ae7d` … `a34cdac1` in `beatbax`) changed the
authoritative `renderSongToPCM` semantics, and feature 004 updated the shared renderer
to match. The following statements above are **stale**:

- **§2 envelope** — `getEnvelopeValue` now linearly interpolates between GB volume
  steps (matching `setValueCurveAtTime`), then holds the last level; not discrete steps.
- **§3 wave** — the wave output is `((table − mean)/15) × 0.9 × 0.6 × volMul` (the
  `WAVE_OUTPUT_SCALE = 0.9` buffer fill and `WAVE_PLAYBACK_GAIN = 0.6` were omitted here).
- **§2/§4 release** — pulse/noise add a 5 ms linear release tail; a bare wave adds a
  20 ms exponential tail (`setTargetAtTime` shape) past the nominal end.
- **§6 port** — pulse portamento is exponential/geometric (`exponentialRampToValueAtTime`),
  wave is linear Hz; the "smoothstep glide" is gone. Contiguous pulse `port` runs share
  one oscillator (`join`/`hold`), and a normal pulse note resets its phase to 0
  (`continuePhase = legato || portJoin`), where §9's "phase persists across notes" no
  longer holds.
- **§6 vib order** — `modulatePitchHz` applies port → arp → bend → vib, with vibrato
  additive and keyed off the un-arpeggiated base frequency (not the glided frequency).

See `docs/renderer.md` ("Spec-112 parity update") and
`specs/complete/004-device-player/plan.md` for the authoritative current semantics.
