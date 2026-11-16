# Feature 001 — platform bring-up goals

Prove the FM-1 board path for BeatBax FM-1 **before** any song work.

## Goals

1. Pin Felucca’s HAL ([`firmware/FELUCCA_PIN`](../../firmware/FELUCCA_PIN)) and build against that BSP only — never the stock `fm1_open` binary synth SDK.
2. Boot a single diagnostic screen on the 240×240 LCD.
3. Scan every Felucca-mapped control and show activity on screen (and LEDs where applicable).
4. Emit a fixed-point test tone from the audio callback within the [audio budget](../../specs/global/audio.md), with no flash erase, draw, or USB wait in that callback.
5. Enumerate on USB as MIDI device **BeatBax FM-1** (MIDI only).
6. Document OCT- rescue and return to stock ([`recovery.md`](recovery.md)).

## Non-goals (this phase)

- `.bax` parse / song model
- Game Boy voices beyond the test tone
- Composer screens
- Flash song store / USB song transfer
- Host simulator stub
- CDC serial console

## How to run the diagnostic

1. Build and install per [`firmware/README.md`](../../firmware/README.md).
2. Power on: title `BEAT 001`, colour bars, control grid.
3. Press buttons and keys: cells and LEDs light.
4. Turn encoders: cells flash green (CW) or red (CCW).
5. Move MASTER: white bar under the colour strip.
6. Press **PLAY**: yellow tone strip appears and a ~440 Hz square plays; press again to stop.
7. Host: confirm USB MIDI product `BeatBax FM-1`.

## Pass / fail

Use [`checklist-001.md`](checklist-001.md). Feature 001 is not `complete` until a maintainer records a hardware pass, including OCT- rescue and a stock (or known-good) reinstall.

Live session notes (device state, failed flashes, recovery blockers): [`status-001.md`](status-001.md).
