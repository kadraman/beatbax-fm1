# 008 — Named-effect parity (vib / arp / port / bend)

**Status**: complete
**Issue**: (to be filed)
**Area**: audio (host renderer + firmware blob)

## Why

Feature 003 froze the named-effect surface (`vib` / `arp` / `port` / `bend`) as in-scope
and feature 004 ported the spec-112 envelope / wave / release / portamento semantics. But the
per-effect demo songs in `songs/gameboy/effects/` (BeatBax's own fixtures) were never wired
into the comparison harness. Running them against the BeatBax CLI shows the surface is
incomplete:

| Effect | Demo | vs BeatBax |
|--------|------|------------|
| sweep (NR10) | `gb_sweep_demo` | 0.9503 ✓ |
| arpeggio | `gb_arpeggio_demo` | 0.9338 |
| pitch bend | `gb_pitchbend_demo` | 0.0000 |
| vibrato | `gb_vibrato_demo` | 0.2293 |
| portamento | `gb_portamento_demo` | no playable events |

Root causes are (a) silently-ignored effect parameters, which violates the constitution's
"refuse unsupported constructs with a named reason" rule, and (b) missing lowering for parts
of the `vib`/`bend` syntax that BeatBax supports.

## Scope

**In scope (complete the 003 surface — match BeatBax spec-112 `modulation.ts` exactly):**

- **Vibrato** `vib:depth,rate[,waveform[,durationRows[,delayRows]]]`
  - `depth` = p0 (0–15), `rate` = p1 in Hz (default 4 when omitted/zero, clamped ≥ 0.1).
  - `waveform` = p2: `sine` (default), `square`, `saw`/`sawtooth`, `triangle`/`tri`.
  - `durationRows` = p3 → active for `floor(durationRows) × tickSec` (only when > 0).
  - `delayRows` = p4 → onset after `delayRows × tickSec` (only when > 0).
  - `tickSec = (60 / bpm) / 4` (4 ticks per beat).
  - LFO amplitude `= clamp(depth,0,15) × baseFreq² / 131072`, added to the pitch in Hz,
    keyed off the **un-arpeggiated** base frequency, applied **last** (port → arp → bend → vib).
  - LFO shape (phase 0 = square high, triangle −1, sine 0):
    - sine `sin(2πp)`, square `p<0.5 ? +1 : −1`, saw `2p−1`, triangle `p<0.5 ? 4p−1 : 3−4p`.
- **Bend** `bend:semitones[,curve[,delay[,time]]]`
  - `semitones` = p0, **fractional allowed** (e.g. `+0.5`).
  - `curve` = p1: `linear` (default), `exp`/`exponential`, `log`/`logarithmic`, `sine`/`sin`.
  - `delay` = p2 in **seconds** (default `durSec/2`, clamped `[0, durSec]`).
  - `time` = p3 in **seconds** (default `durSec − delay`, clamped `[0, durSec − delay]`).
  - progress curves: linear `p`, exp `p²`, log `1−(1−p)²`, sine `(1−cos(πp))/2`.
  - `freq ×= 2^(semitones × p / 12)`.
- **Arpeggio** `arp:off1,off2[,off3…]`
  - offsets cycle as `[0, …offsets]` (root prepended); 60 Hz frame rate; `frame = floor(t×60)`;
    `semi = offsets[frame % len]`.
  - Verify the 0.9338 gap and bring to ≥ 0.95.
- **Portamento** `port:speed`
  - speed clamped `[1,255]`; `portDur = max(0.001, (256−speed)/256 × noteDur × 0.6)`;
    pulse = exponential (log-freq lerp), wave = linear Hz. Already implemented; re-verify.
- **Inline effect expressions** `<vib:…>` / `<bend:…>` / `<arp:…>` / `<port:…>` in note
  tokens, resolved the same as named effects (BeatBax supports both; today only named
  effects resolve, inline raw expressions are dropped).

**Out of scope (refuse with a named reason, never silently ignore):**

- `trem`, `volSlide`, `cut`, `retrig`, `echo` effect tokens (not part of the 002/003
  named-effect surface).

## Success criteria

1. Each in-scope effect demo (`gb_arpeggio_demo`, `gb_pitchbend_demo`, `gb_portamento_demo`,
   `gb_vibrato_demo`, `gb_sweep_demo`) compares to the BeatBax CLI at **≥ 0.95** Pearson
   correlation in `make test-compare`.
2. A song using an out-of-scope token (`trem`/`volSlide`/`cut`/`retrig`/`echo`) is refused
   with a named reason, not silently rendered.
3. Existing fixtures (`tutorial_groove`, `gb_sweep_demo`, `gb_uge_note_demo`, `grassland_dash`)
   stay ≥ 0.95 and the streaming/offline renderer bit-identity (`make test`) is unchanged.
4. Firmware renders a song exercising all four effects byte-identical to the host renderer
   (emulator correlation 1.0), with the audio callback still under budget.

## Non-goals

- No new effect types or `.bax` syntax beyond BeatBax's published Game Boy surface.
- No on-device editor for effects; this is render/lower/parse parity only.
- `volSlide`/`trem`/`echo`/`retrig`/`cut` audio behavior is not implemented (refused).
