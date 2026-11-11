# Hardware contract (M-VAVE FM-1)

Facts this project depends on. Cite community measurements (Sidou hardware notes, Felucca, fm1-nes) rather than copying proprietary JieLi manuals.

## MUST

- Target the **M-VAVE FM-1** with JieLi **AC791N / WL82** (typically AC7911B8-class: ~512 KB on-chip SRAM, **1 MiB** in-package flash, **no PSRAM/SDRAM**).
- Use a **pinned Felucca** commit as the device HAL for panel, LCD, USB, and audio. Record the pin in feature 001 and in release notes.
- Treat the display as a **240×240** SPI LCD (ST7789-class). A full RGB565 frame is about **115 KB**; draw dirty regions.
- Drive audio through the device codec at **44117.6 Hz** (24 MHz / 544), matching Felucca-based firmwares.
- Scan the full Felucca panel map: **27 note keys**, labeled buttons, **four encoders (K1–K4)**, and **MASTER** on the ADC.
- Document **OCT- held at power-on** rescue and a path back to stock firmware before shipping any flashing release.
- Keep hot audio code in **SRAM** (`.ram_text` / equivalent). Calls between XIP flash and SRAM MUST use the platform’s far-call pattern where required.
- Preserve the bootloader when updating; prefer app/package update flows Felucca documents. Do not publish stock images or private flash dumps.

## MUST NOT

- Use the stock `fm1_open` **binary SDK** as the BeatBax synth engine.
- Assume SDRAM, PSRAM, or overlays exist on this part.
- Erase or program flash from inside the audio callback.
- Rely on Wi-Fi or Bluetooth for phases 001–007 unless a later feature specifies them.
- Treat fm1-nes’s experimental flash writer as the default install path.

## Build environment

- Firmware builds use the **JieLi Windows pi32** toolchain family (same family as `fm1_open` / community firmwares). Feature 001 records the exact version and how to set the toolchain path.
- Host tools build and test without the JieLi toolchain.

## Recovery (user-facing minimum)

1. Hold **OCT-** alone while powering on to enter USB rescue / updater mode.
2. Install a known-good package (BeatBax FM-1 or stock) with the documented installer.
3. Switching firmwares does **not** migrate song/project formats from other firmwares; users MUST back up first.

## Open / deferred

- Exact free flash regions for the song store are decided in feature 006 against the pinned Felucca layout.
- Optional FPU use is deferred; see [audio.md](audio.md).
- Second-core use is out of scope until a measured feature specifies it.
