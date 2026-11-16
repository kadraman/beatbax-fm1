# BeatBax FM-1 Song Model Reference

This document defines the supported BeatBax Game Boy `.bax` subset implemented by the host song library (`host/song/`) and device player/composer.

## Oracle Version Pin

- **BeatBax CLI Oracle:** `@beatbax/cli` / `beatbax` version **0.6.1**
- **Validation Oracle:** `beatbax verify <song.bax>` must exit 0 on all original and canonically serialized fixtures.

---

## Supported Game Boy (`chip gameboy`) Surface

The FM-1 currently targets the Nintendo Game Boy (DMG-01) sound hardware (4 channels):
- **Channel 1:** Pulse 1 with hardware frequency sweep and volume envelope
- **Channel 2:** Pulse 2 with volume envelope
- **Channel 3:** Custom 4-bit 32-sample Wave channel
- **Channel 4:** White/metallic Noise channel (7-bit or 15-bit polynomial LFSR)

### 1. Song Metadata & Global Directives

| Directive | Syntax Example | Notes |
|---|---|---|
| `song name` | `song name "Tutorial Groove"` | Song title string |
| `song artist` | `song artist "BeatBax Tutorial"` | Author / composer |
| `song description` | `song description "..."` | Multi-line or single-line description |
| `song tags` | `song tags "tutorial, gameboy"` | Comma-separated tag string |
| `chip` | `chip gameboy` (or `gb`, `dmg`) | Must be Game Boy |
| `bpm` | `bpm 140` | Beats per minute (integer or float) |
| `stepsPerBar` | `stepsPerBar 16` | Default 16 steps per bar |
| `scale` | `scale G major [warn]` | Musical key / scale constraint (optional `warn` flag) |

### 2. Instruments (`inst`)

Instruments configure channel parameters and envelopes.

#### Pulse 1 & Pulse 2
- `type`: `pulse1` or `pulse2`
- `duty`: `12.5`, `25`, `50`, or `75` (pulse duty cycle percentage)
- `env`: Hardware envelope:
  - Object notation: `{"level": 0..15, "direction": "up"|"down", "period": 0..7}`
  - Compact notation: `gb:<level>,<direction>,<period>` (e.g. `gb:13,down,1`)
- `sweep` (*Pulse 1 only*):
  - Object notation: `{"time": 0..7, "direction": "up"|"down", "shift": 0..7}`
  - Compact notation: `gb:sweep:<time>,<dir>,<shift>`
- `length`: Sound length counter (0..64)
- `vol`: Constant volume level (0..15)
- `vol_env`: Software volume macro array `[v0, v1, ...]`
- `pitch_env`: Software pitch offset array `[p0, p1, ...]`
- `subpat`: Identifier of an associated subpattern macro
- `gm`: General MIDI program number hint (0..127)

#### Wave
- `type`: `wave`
- `volume`: Output level percentage (`0`, `25`, `50`, or `100`)
- `wave`: Array of 16 or 32 nibbles (`0` to `15`) representing wave samples
- `length`: Sound length counter (0..256)
- `subpat`: Identifier of an associated subpattern macro
- `gm`: General MIDI program number hint

#### Noise
- `type`: `noise`
- `gb:width`: LFSR width counter: `7` (metallic / periodic) or `15` (white / pseudo-random)
- `env`: Hardware envelope (`gb:<level>,<direction>,<period>` or JSON object)
- `uge_note`: hUGETracker note name mapping (e.g. `C-6`, `C-7`, `C-8`, `D-8`)
- `note`: Fallback note identifier
- `length`: Sound length counter (0..64)
- `vol_env`: Software volume macro array
- `pitch_env`: Software pitch macro array
- `subpat`: Identifier of an associated subpattern macro
- `gm`: General MIDI program number hint

### 3. Named Effects (`effect`)

- `effect <name> = vib:<speed>,<depth>[,<waveform>]` (Vibrato)
- `effect <name> = arp:<semi1>,<semi2>` (Arpeggio)
- `effect <name> = port:<speed>` (Portamento / Slide)

### 4. Subpatterns (`subpat`)

Used for advanced macro behavior (step-by-step volume, pitch, or effect actions terminating in `halt` or `jump:<step>`).

### 5. Patterns (`pat`)

Pattern definition:
```bax
pat <name> = <token> <token> ...
```

Tokens may include:
- Notes with octaves: `C3`, `D#4`, `Eb5`, `F-6`
- Rests: `.`
- Holds: `-`
- Duration modifiers: `<note>:<duration>` (e.g. `C4:4`, `G5:16`)
- Effect tags: `<note><effectName>` or `<note><effectName>:<duration>` (e.g. `C6<wobble>:2`)
- Inline instruments: `inst <name>` or instrument identifier (e.g. `kick`, `snare`)
- Inline volume: `vol:<level>` (e.g. `vol:8`)

### 6. Sequences (`seq`)

Sequences concatenate patterns:
```bax
seq <name> = <pattern_ref> ...
```
Patterns within sequences can take inline modifiers:
- `:inst(<instName>)`
- `:oct(<semitones_or_octaves>)`
- `:vol(<volume>)`

### 7. Channel Assignments (`channel`)

Routes sequences to the 4 hardware channels:
```bax
channel 1 => [inst <default_inst>] seq <seq_name>... [lock=scale]
channel 2 => [inst <default_inst>] seq <seq_name>... [lock=scale]
channel 3 => [inst <default_inst>] seq <seq_name>... [lock=scale]
channel 4 => [inst <default_inst>] seq <seq_name>... [lock=scale]
```

### 8. Play Statement

- `play` or `play auto`

---

## Unsupported Constructs & Refusal Reasons

When loading songs, the parser strictly refuses unsupported constructs with clear named diagnostic messages:

1. **Non-Game Boy Chips:**
   - E.g. `chip nes`, `chip sms`, `chip spectrum-128`
   - *Reason:* `"unsupported chip: <chip>"`
2. **External / Remote Imports:**
   - E.g. `import "local:..."`, `import "npm:..."`
   - *Reason:* `"unsupported directive: import (device songs must be self-contained)"`
3. **Multi-Chip Directives:**
   - Songs declaring multiple chips or unsupported multi-console syntax
   - *Reason:* `"multi-chip songs not supported in phase 002"`
4. **Invalid Channel Mappings:**
   - E.g. mapping wave voice to channel 1, or binding channel > 4
   - *Reason:* `"invalid channel binding: channel <N> does not support voice type '<type>'"`
5. **Malformed Syntax:**
   - Syntax errors, unclosed strings, missing tokens
   - *Reason:* Syntax error with line/column indicator
