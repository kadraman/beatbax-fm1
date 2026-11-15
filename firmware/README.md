# BeatBax FM-1 firmware

GPL-3.0-or-later application firmware for the M-VAVE FM-1. The device HAL is the **felucca-bsp** tree pinned from a Felucca commit (see [`FELUCCA_PIN`](FELUCCA_PIN)). Host song code must not link this tree — see [`../LICENSING.md`](../LICENSING.md).

## Feature 001 — platform bring-up

Diagnostic image under [`app/`](app/):

| Surface | Behaviour |
|---------|-----------|
| LCD | One diagnostic screen: `BEAT 001` title, colour bars, control grid |
| Controls | All 14 buttons, 27 note keys, 7 encoders, MASTER pot reflected on screen / LEDs |
| Tone | **PLAY** (matrix button id 12) toggles a fixed-point ~440 Hz square in the audio callback |
| USB | **MIDI only**. Product string `BeatBax FM-1`, manufacturer `BeatBax`. Package id `FM-1_1001` |
| Recovery | Two failed boots → UBOOT; hold **PLAY** at power-on → UBOOT (implemented); see [`../docs/hardware/recovery.md`](../docs/hardware/recovery.md) |

No CDC serial in this phase. No song model.

## Prerequisites

1. **Pinned HAL**
   ```bash
    git submodule update --init firmware/third_party/felucca-bsp
    git -C firmware/third_party/felucca-bsp checkout $(grep FELUCCA_BSP_COMMIT firmware/FELUCCA_PIN | cut -d= -f2)
   ```

    If the checkout SHA disagrees with [`FELUCCA_PIN`](FELUCCA_PIN), stop and reconcile before flashing.
2. **JieLi pi32 toolchain** (`JIELI_TOOLCHAIN`)

   Same family Felucca / felucca-bsp use: clang targeting `pi32v2`, plus `ld` and `objcopy`.
   - **Linux x86-64 / WSL2 / Docker:** follow [Felucca](https://github.com/hugelton/Felucca/blob/main/BUILDING.md) `BUILDING.md` (`tools/get_toolchain.sh` → `~/.jieli/toolchain`).
   - **Windows native:** set `JIELI_TOOLCHAIN` to the root that contains `pi32v2/bin/clang.exe` (or `clang`) and `common/bin/objcopy`. Git Bash or WSL is the supported way to run `app/build.sh` on Windows hosts.

    If `JIELI_TOOLCHAIN` is unset, the build script exits with an error (no silent fallback).

   **WSL tip:** building from `/mnt/c/...` with Git `core.autocrlf=true` rewrites `*.sh` to CRLF and dash fails with `set: Illegal option -`. Prefer a clone under the Linux filesystem (`~/repos/...`), or strip CRs once: `sed -i 's/\r$//' firmware/**/*.sh firmware/third_party/felucca-bsp/tools/*.sh`.
3. **Packaging / install (optional for compile-only)**
   - JieLi AC79 SDK via the BSP fetcher (preferred). The GitHub
     `Jieli-Tech/fw-AC79_AIoT_SDK` remote 404s; use **Gitee** (same pin
     `AC79NN_SDK_V1.2.1_2023-12-13` / `d179b448…`):

     ```bash
     rm -rf ~/.jieli/ac79-sdk   # if a failed fetch left an empty tree
     SDK_URL=https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git \
       sh firmware/third_party/felucca-bsp/tools/get_sdk.sh
     export AC79_SDK="$HOME/.jieli/ac79-sdk"
     ```

     Do **not** point `AC79_SDK` at an empty directory — `fm1pkg` needs
     `cpu/wl82/tools/uboot.boot` from that checkout.
   - Python 3 with `mido` and `python-rtmidi` for `fm1_install.py`
   - Update loader from `firmware/app/build_loader.sh` → `build/ota.bin`

## Build

```bash
export JIELI_TOOLCHAIN=~/.jieli/toolchain   # or your Windows/WSL path
cd firmware/app
sh build.sh
# -> firmware/app/build/beatbax-fm1-001.bin
sh build_loader.sh
# -> firmware/app/build/ota.bin
```

## Package and install

Prefer packaging with Felucca 1.7.0’s known-good `ota.bin` (our wrapped loader has
not been validated on hardware yet):

```bash
export AC79_SDK="$HOME/.jieli/ac79-sdk"
# ota lives at app/known-good/felucca-ota.bin (from felucca-1.7.0.fwsc)
python3 pack_with_felucca_ota.py \
  build/beatbax-fm1-001.bin build/beatbax-fm1-001.fwsc --product FM-1_1001
```

Upload `build/beatbax-fm1-001.fwsc` with **M-Flasher**. On a first install from
stock, let M-Flasher install the bootloader (m-bootloader) too; it adds recovery
by holding **REC** on reboot. See [`../docs/hardware/recovery.md`](../docs/hardware/recovery.md).
Expect USB product `BeatBax FM-1`.

Optional (unvalidated): build our loader with `sh build_loader.sh` and package
with `fm1pkg.py` + `build/ota.bin` as in felucca-bsp’s README.

**Back up** projects from any other firmware before switching. Formats do not migrate. See [`../docs/hardware/recovery.md`](../docs/hardware/recovery.md).

### USB identity (host)

After install, the device enumerates as USB MIDI:

| Field | Value |
|-------|-------|
| Manufacturer | `BeatBax` |
| Product | `BeatBax FM-1` |
| Package / OTA id | `FM-1_1001` |

## Tone and audio rules

- Callback fills one half of a 128-frame stereo block with a Q23 square when `tone_on` is set.
- The callback does **not** erase flash, draw the LCD, or wait on USB.
- Measure callback cost on hardware before calling the phase complete (checklist).

## License

- `app/` — GPL-3.0-or-later (this project)
- `third_party/felucca-bsp/` — MPL-2.0 (upstream); see its `LICENSING.md`
- Combined flashed image that includes the BSP is a GPL work; distribute corresponding source with releases
