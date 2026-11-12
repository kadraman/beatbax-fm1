# Audio contract

Real-time audio on the FM-1 is the hardest budget. These rules apply to every firmware feature that produces sound.

## MUST

- Render at the device codec rate **44117.6 Hz** unless a measured feature documents a different path with SRC.
- Keep the audio callback (block fill for the codec / ALNK path) **bounded**. Target: finish in **under half** of a **256-frame** block budget (community firmwares cite ~5.8 ms for 256 frames; leave headroom for USB and key scan).
- Run hot render paths from **SRAM**.
- Use **fixed-point** arithmetic in the first Game Boy renderer (feature 003 / 004). Q15 levels and integer phase accumulators are the default approach.
- Share state with the main loop through `volatile` (or equivalent) with a documented single-writer rule. 32-bit stores are treated as atomic; multi-field structs need a safe publish order.
- Measure callback cost with platform ticks (`fm1_ticks` / TIMER) before claiming a flashing release is audio-safe.
- Silence or soft-fail if a song cannot be rendered within budget; never busy-wait in the callback.

## MUST NOT

- Use `float` / `double` in the first audio path (Dinghy/Felucca-linked builds historically reject soft-float). Enabling the hardware FPU is a **later, measured** feature with its own spec.
- Perform 64-bit division in firmware audio code if the toolchain lacks the helper (prefer shifts / 32-bit math).
- Erase or program flash, draw the LCD, call `platform_poll`-style USB waits, or block on locks inside the audio callback.
- Allocate from a general heap inside the callback.

## Game Boy voices (phases 003–007)

- Four voices: pulse 1, pulse 2, wave, noise — matching BeatBax’s Game Boy surface.
- Envelopes, duty, wave table, and effects that BeatBax already implements for that chip are in scope for 003; inventing new chip behavior is not.
- Host renderer (003) and device renderer (004) MUST share the same fixed-point algorithms so results stay comparable.

## Comparison with BeatBax CLI

- Feature 003 defines numeric tolerances (event timing and/or resampled WAV metrics). “Sounds close” is not a test.
- Device rate vs BeatBax’s host rate: prefer comparing **musical events** plus a **resampled** WAV, or resample both to a common rate — stated explicitly in 003.

## Main loop

- Call platform poll often enough for USB and the watchdog (Felucca/community guidance: every few ms; never starve for seconds).
- Save/erase flash only when transport is silent (see feature 006).
