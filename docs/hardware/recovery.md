# FM-1 recovery and return to stock

Installing custom firmware (BeatBax FM-1, Felucca, or others) can leave the device unable to boot if an update is interrupted. Read this **before** flashing.

## Warnings

1. **Back up first.** Switching firmwares does **not** migrate song or project formats from other firmwares (Felucca projects, stock presets, etc.). Export or photograph anything you care about before you install BeatBax FM-1.
2. **Do not unplug** during an update session once the installer has started writing.
3. This project does **not** publish stock firmware images or private flash dumps. Obtain stock packages from M-VAVE / official channels only.

## Install with M-Flasher

BeatBax FM-1 bring-up is uploaded with **M-Flasher**, alongside a bootloader (m-bootloader) that provides the REC-hold recovery below.

1. Build and package the firmware — see [`firmware/README.md`](../../firmware/README.md).
2. Upload `build/beatbax-fm1-001.fwsc` with M-Flasher (package identity `FM-1_1001`).
3. On a first install from stock, let M-Flasher install the bootloader too.

## REC-hold recovery (primary)

The bootloader gives a recovery mode that needs no USB_KEY dongle:

1. Hold **REC**.
2. Power-cycle (off, then on) while still holding **REC**.
3. The bootloader appears on USB; re-flash with M-Flasher:
   - BeatBax FM-1 bring-up: `build/beatbax-fm1-001.fwsc`
   - Or reinstall **stock** / another known-good package.

This replaces the older USB_KEY (Flipper / lilfm1buddy / Transporter) route on this unit.

## PLAY-hold rescue (bring-up's own escape)

The bring-up firmware also has a built-in escape, independent of the bootloader:

1. Power the FM-1 **off**.
2. Hold **PLAY** (the transport PLAY button).
3. Power **on** while still holding **PLAY**.
4. The device enters ROM download mode (`WL82 UBOOT1.00`); restore a known-good package with the Unbricker.

**UBOOT is recoverable, not a brick** — an **OK** `WL82 UBOOT1.00` in Windows Disk drives is the recoverable ROM download mode.

## OCT- rescue (Felucca / stock convention)

1. Power the FM-1 **off**.
2. Hold **OCT-** alone (do not hold other keys).
3. Power **on** while still holding **OCT-**.
4. Release when the device is in USB rescue / updater mode.
5. Install a known-good package.

Prefer a controlled entry (REC-hold or PLAY-hold) over relying on a crash loop.

## Return to stock

1. Enter recovery with REC-hold (or the soft-key / installer path your current firmware documents).
2. Install the official M-VAVE FM-1 firmware package with M-Flasher or the vendor's updater.
3. Expect **no** migration of BeatBax `.bax` songs into the stock workstation format.

## If install tools fail

- Confirm the USB cable carries data (not charge-only).
- Try another USB port / hub policy (direct to the host).
- If the bootloader itself is damaged (REC-hold no longer works), the last-resort routes are the [FM-1 Unbricker](https://github.com/Quixotic7/MvaveFM1Unbricker) from `WL82 UBOOT1.00` (reached with a USB_KEY dongle such as the Flipper), or the [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter).
- Do not post flash dumps or secrets in issues.

## Related

- Bring-up goals: [`bring-up.md`](bring-up.md)
- Hardware checklist for feature 001: [`checklist-001.md`](checklist-001.md)
- Global hardware contract: [`../../specs/global/hardware.md`](../../specs/global/hardware.md)
