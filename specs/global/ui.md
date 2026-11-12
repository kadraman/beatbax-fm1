# UI contract (graphical multi-screen)

The FM-1 panel is too small for comfortable `.bax` source editing. The product UI is a set of **graphical screens** driven by physical controls.

## MUST

- Present a fully graphical UI. Pattern steps, chains, envelopes, and waves are drawn as cells, tiles, and graphs — **not** as editable source text.
- Implement at least these screens in feature 005 (names may match on-device labels):

  | Screen | Role |
  |--------|------|
  | Song | Tempo, chip, four lanes, playhead |
  | Pattern | One channel; step cells for notes/effects |
  | Chain | Sequence tiles |
  | Instrument | Duty, envelope graph, wave drawing, noise params |
  | Mix | Channel levels / mute-solo as specified in 005 |

- **SEQ** cycles the composer screens (or returns to the last song view, as feature 005 specifies).
- **HOME** backs out one level. **Hold HOME** shows that screen’s help / control card.
- **K1–K4** always bind to **four labeled on-screen parameters** on the current page.
- **Note keys** audition the selected instrument; on Pattern they enter notes (feature 005 / 007 detail).
- **OCT- / OCT+** set entry / audition octave.
- **EDIT** is the modifier key for secondary actions defined per screen.
- **PLAY** starts/stops transport. **REC** arms record (behavior fully specified in 007; button may be labeled earlier).
- **SAVE** is labeled on the panel; persistence behavior is specified in 006.
- **MASTER** controls output level (ADC pot).
- **PRESETS, SELECT, ALGORITHM, ENV, LFO, FX, ARP, SCL, GLO** jump to the instrument or effect control those names map to for **Game Boy parameters only** (feature 005). If a button has no Game Boy meaning on the current screen, show an on-screen unused response — do not leave the control silent.
- Draw **dirty regions** only when practical. Avoid full-screen redraw every frame.
- Provide a first-run or help path so every control’s job on the current screen is discoverable without a computer.

## MUST NOT

- Add a panel text editor for `.bax` source in phases 001–007.
- Leave any Felucca-reported control without a specified job or unused response on a shipped screen.
- Perform heavy flash erase or full-screen SPI bursts inside the audio callback (see [audio.md](audio.md)).
- Copy Sidou/Chipbox/OPN-1 screen bitmaps as a license-unsafe dependency; redesign within BeatBax branding.

## Bring-up (feature 001)

Before the composer exists, a single diagnostic screen that paints and reflects control activity is acceptable. It MUST still scan every control and MUST document the mapping used for the diagnostic.

## References

- Product patterns (not code): Sidou multi-screen tracker, OPN-1 knob pages, Chipbox dirty-draw discipline.
