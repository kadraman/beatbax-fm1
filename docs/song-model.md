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
  - Object notation: `{"level": 0..15, "direction": "up"|"down"|"flat", "period": 0..7}`
  - Compact notation: `<level>,<direction>[,<period>]` or `gb:<level>,<direction>[,<period>]`
    (e.g. `12,down`, `13,down,1`, `10,flat`; a missing period defaults to `1`, `flat` forces `0`)
- `sweep` (*Pulse 1 only*):
  - Object notation: `{"time": 0..7, "direction": "up"|"down", "shift": 0..7}`
  - Compact notation: `<time>,<dir>,<shift>` (e.g. `sweep=4,down,7`)
- `length`: Sound length counter (0..64)
- `vol`: Constant volume level (0..15)
- `vol_env`: Software volume macro array `[v0, v1, ...]`
- `pitch_env`: Software pitch offset array `[p0, p1, ...]`
- `duty_env`: Software duty macro array (values `0`–`3`)
- `arp_env`: Software arpeggio offset macro array (signed semitones)
- `pan` / `gb:pan`: `pan=<L|R|C|num>` (generic) or `gb:pan=<L|R|C>` (GB NR51)
- `subpat`: Identifier of an associated subpattern macro
- `gm`: General MIDI program number hint (0..127)

#### Wave
- `type`: `wave`
- `volume`: Output level percentage (`0`, `25`, `50`, or `100`)
- `wave`: Array of 16 or 32 nibbles (`0` to `15`), or a 32-nibble hUGETracker hex string (e.g. `wave=0123456789ABCDEFFEDCBA9876543210`)
- `length`: Sound length counter (0..256)
- `subpat`: Identifier of an associated subpattern macro
- `gm`: General MIDI program number hint

#### Noise
- `type`: `noise`
- `gb:width` (or `width`): LFSR width counter: `7` (metallic / periodic) or `15` (white / pseudo-random)
- `env`: Hardware envelope (`<level>,<direction>[,<period>]`, `gb:...`, or JSON object)
- `uge_note`: hUGETracker note name mapping (e.g. `C-6`, `C-7`, `C-8`, `D-8`)
- `note`: Fallback note identifier
- `length`: Sound length counter (0..64)
- `vol_env`: Software volume macro array
- `pitch_env`: Software pitch macro array
- `subpat`: Identifier of an associated subpattern macro
- `gm`: General MIDI program number hint

### 3. Named Effects (`effect`)

- `effect <name> = vib:<speed>,<depth>[,<waveform>[,<arg>]]` (Vibrato)
- `effect <name> = arp:<semi1>,<semi2>[,<semi3>]` (Arpeggio; up to three chord offsets)
- `effect <name> = port:<speed>` (Portamento / Slide)
- `effect <name> = bend:<semitones>[,<curve>[,<delay>[,<time>]]]` (Pitch bend; `curve` = `linear`|`exp`|`log`|`sine`)

The effect right-hand side is opaque text in BeatBax (parsed only at render time), so the
host model preserves the raw expression verbatim and writes it back byte-stable. This keeps
arbitrary arity and unknown future effects round-tripping without inventing a sub-grammar.

### 4. Subpatterns (`subpat`)

Used for advanced macro behavior (step-by-step volume, pitch, or effect actions terminating in `halt` or `jump:<step>`).

### 5. Patterns (`pat`)

Pattern definition:
```bax
pat <name> = <token> <token> ...
```

Tokens may include:
- Notes with octaves: `C3`, `D#4`, `Eb5`, `F-6`
- Rests: `.` and holds: `-`
- Duration modifiers: `<note>:<duration>` (e.g. `C4:4`, `G5:16`)
- Effect tags: `<note><effectName>` or `<note><effectName>:<duration>` (e.g. `C6<wobble>:2`)
- Inline instruments: `inst <name>` or instrument identifier (e.g. `kick`, `snare`)
- Temporary instruments: `inst(<name>)` or `inst(<name>,<dur>)` (e.g. `inst(melody_hold,2)`)
- Inline volume: `vol:<level>` (e.g. `vol:8`)
- Group repeats and token repeats: `(C4 E4 G4)*2`, `C4*3`

Patterns preserve the raw right-hand side verbatim (whitespace-normalised), so group repeats,
token repeats, and inline effect tags round-trip byte-stable without a pattern sub-grammar.

### 6. Sequences (`seq`)

Sequences concatenate patterns:
```bax
seq <name> = <pattern_ref> ...
```
Pattern references carry modifiers; the host model preserves the full reference verbatim:
- `:inst(<instName>)`, `:oct(<n>)`, `:vol(<n>)`, `:st(<n>)`, `:trans(<n>)`, `:semitone(<n>)`
- `:rev`, `:slow(<n>)`, `:fast(<n>)`, `:pan(<v>)`, named-effect presets (e.g. `:wobble`), `*<n>` repeats

### 7. Channel Assignments (`channel`)

Routes sequences/patterns to the 4 hardware channels, or binds a role to a form:
```bax
channel 1 => [inst <default_inst>] seq <seq_ref>... [lock=<mode>]
channel 2 => [inst <default_inst>] pat <pat_ref>... [lock=<mode>]
channel 3 => [inst <default_inst>] role <role> form <form_name>
```
- `lock=<mode>` accepts `scale`, `root+fifth`, `chord`, `chord7`, or `octaves`.
- `role` + `form` are advanced binding (`song_composition_advanced_demo.bax`); they must appear together.

### 8. Sections (`section`)

Sections group per-role pattern lists into a named song part; BeatBax expands each role to a
synthetic sequence `<section>_<role>`:
```bax
section theme {
  mel  = phrase_a phrase_b
  harm = harm_a harm_b
  bass = bass_a bass_b
  perc = drums_a drums_b
}
```
The Beginner Mode `section <name> use p1 p2 p3 p4` sugar is desugared to the block form with
Game Boy default roles `mel harm bass perc`.

### 9. Forms and Channel Auto Routing (`form` + `channel auto`)

A `form` declares the section order once; `channel auto` binds the four lanes to instruments
using BeatBax's Beginner Mode roles (`mel`, `harm`, `bass`, `perc` on Game Boy):
```bax
form main = theme, build, turn, reprise
channel auto form main inst=melody|harmony|bass2|snare
```
`>` is also accepted as a section separator in `form`; the canonical writer normalizes it to `,`.

### 10. Play Statement

- `play`, `play auto`, or `play auto repeat`

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
5. **Channel-level timing:**
   - E.g. `speed=`, `bpm=` (channel-level tempo)
   - *Reason:* `"unsupported channel directive: <directive> (channel-level timing)"`
6. **Pattern shorthand (bare `name = ...`):**
   - E.g. `a = C4 E4 G4` (implied `pat a = ...`)
   - *Reason:* `"unsupported top-level directive: <name>"`
7. **Unknown instrument options:**
   - Any `key=value` not in the supported list above
   - *Reason:* `"unsupported instrument option: <key>"`
8. **Malformed Syntax:**
   - Syntax errors, unclosed strings, missing tokens
   - *Reason:* Syntax error with line/column indicator

---

## Feature coverage

Machine-checked against BeatBax's own granular feature fixtures in
[songs/features](../songs/features/), [songs/gameboy/effects](../songs/gameboy/effects/), and
[songs/gameboy/instruments](../songs/gameboy/instruments/). Run
`python tools/test-bax-coverage.py` (or `make test-coverage`); the expected outcome per fixture is
declared in [tools/bax-coverage.json](../tools/bax-coverage.json). At the last freeze: **26
supported, 3 refused** (imports, pattern shorthand).
