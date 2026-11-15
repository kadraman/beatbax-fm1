# Feature 001 — hardware bring-up status

**Date:** 2026-11-13 (updated 2026-11-15)  
**Scope:** Platform bring-up flash / boot on one M-VAVE FM-1 (Windows host + WSL build)  
**Spec:** [`specs/complete/001-platform-bring-up/`](../../specs/complete/001-platform-bring-up/)  
**Related:** [`bring-up.md`](bring-up.md), [`recovery.md`](recovery.md), [`checklist-001.md`](checklist-001.md)

## Current status

| Item | State |
|------|--------|
| Feature 001 | **Booting on hardware** — `BEAT 001`, controls, USB working; checklist rows still to be recorded |
| Device (last known) | Running **BeatBax bring-up** (`BEAT 001`); boots, USB MIDI up |
| Host build | `beatbax-fm1-001.bin` builds in WSL with pinned felucca-bsp |
| Packaging | `fm1pkg` + Felucca 1.7.0 `ota.bin` validated (bit-identical repack of official Felucca app) |
| Our wrapped `ota.bin` (`wrap_ota.py` / `build_loader.sh`) | **Not validated** on hardware — do not use for installs until proven |
| Install path | **M-Flasher** (with m-bootloader installed alongside for recovery) |
| Recovery | Hold **REC** on reboot → bootloader; PLAY-hold → `WL82 UBOOT1.00` |

**Blocked on:** nothing — the unit is recovered and boots BeatBax bring-up. Remaining: record the checklist rows. The two-boot → UBOOT guard is **disabled** (it fires falsely on this unit); recovery is via REC-hold or PLAY-hold.

## What worked

1. **WSL build** of JieLi pi32 images (`JIELI_TOOLCHAIN`, `AC79_SDK`).
2. **SDK blobs** match Felucca’s tested hashes (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`).
3. **BSP pin** `5942e68…` matches Felucca v1.7.0’s `firmware/bsp` submodule.
4. **`fm1pkg.flash_image` + `ufw`**: repacking official `felucca-1.7.0-app.bin` + extracted Felucca `ota.bin` produces a package **byte-identical** to `felucca-1.7.0.fwsc`.
5. **Official Felucca 1.7.0** installs and runs on this unit (from stock / after Unbrick).
6. **TransitFM1** installs and shows its READY screen.
7. **minboot v2** (green screen, Felucca ota, no PC/stack guards, never calls `fm1_enter_uboot`) **boots and shows green**.
8. **Unbricker** successfully restored official V15 when `WL82 UBOOT1.00` was present.
9. **fm-static** works for installs when the running firmware has USB MIDI (pygame MIDI backend needed on this Windows box; `python-rtmidi` crashes under Python 3.14/3.13).

## What failed

| Attempt | Result |
|---------|--------|
| BeatBax bring-up (first packages, our `ota.bin`) | Black screen → **UBOOT** |
| Upstream **hwtest** + our `ota.bin` | Black → **UBOOT** |
| hwtest + **Felucca** `ota.bin` | Black → **UBOOT** |
| BeatBax after “screen before audio” tweak | Still **UBOOT** |
| minboot v1 (had boot-fail → `fm1_enter_uboot` + early `fm1_guard_enable`) | Black → **UBOOT** |
| OCT− / OCT−+OCT+ / OCT−+OCT++HOME holds from minboot | No effect |
| Unbricker while on green minboot | `No WL82 UBOOT1.00 device found` (expected: not in ROM boot) |

## Issues / root causes

### 1. Device stranded on USB-less minboot (active)

minboot proved the image/boot path but **omitted USB MIDI and OTA** and **removed** the two-fail → UBOOT path. The unit runs, but:

- No MIDI → no `fm1_install` / fm-static / softkey into UBOOT  
- No crash loop → no automatic ROM boot  
- Documented key holds did not enter UBOOT on this unit  

**Recovery requires a USB_KEY dongle** (lilfm1buddy, Flipper FM-1 Recovery, or FM-1 Transporter). See [`recovery.md`](recovery.md) and community Unbricker docs.

### 2. Early boot guards turned init faults into UBOOT

felucca-bsp-style bring-up that:

1. Sets `guard.pending` and after two failed boots calls `fm1_enter_uboot()`, and/or  
2. Enables `fm1_guard_enable(...| FM1_GUARD_PC)` **before** the LCD is up  

…produced black screens and ROM UBOOT. minboot v2 (no early PC/stack guards, no UBOOT entry) ran. BeatBax bring-up was updated to paint the screen and clear the boot-fail guard **before** enabling hardware guards / USB / audio — **not yet re-flashed** after that change.

### 3. Our loader wrap unvalidated

Packages using `firmware/tools/wrap_ota.py` + `build_loader.sh` bricked in the same way as app issues early on. Prefer **`app/known-good/felucca-ota.bin`** (from Felucca 1.7.0) via `app/pack_with_felucca_ota.py` until our ota is proven.

### 4. Windows MIDI tooling

- WSL: no `/dev/snd/seq` → cannot install from WSL.  
- Windows: `python-rtmidi` heap-crashes; use **`$env:MIDO_BACKEND='mido.backends.pygame'`** or browser installers (fm-static / Felucca / TransitFM1).  
- PnP lists many **Unknown** ghost names (`Felucca Update`, etc.); trust **Status = OK** only. Live UBOOT = **OK** `WL82 UBOOT1.00` under Disk drives.

### 5. Install UX pitfalls

- fm-static **“device did not come back”** after minboot is **normal** (no USB to answer identity).  
- Do **not** power-cycle twice while in update mode (`Felucca Update`) — that can drop into ROM UBOOT.  
- `build.sh` previously wiped `build/known-good/`; reference files now live under **`firmware/app/known-good/`**.

### 6. Identity note

Package / OTA id is **`FM-1_1001`** (digits only — `fm1_install` rejects non-numeric suffixes such as `FM-1_BB01`). USB product string remains **BeatBax FM-1**.

## Artifacts

| Path | Role |
|------|------|
| `firmware/app/build/beatbax-fm1-001.bin` / `.fwsc` | Latest BeatBax bring-up (Felucca ota, post-guard fix) — **not confirmed on device** |
| `firmware/app/build/minboot.bin` / `.fwsc` | Green-screen probe (currently on device) |
| `firmware/app/known-good/felucca-1.7.0.fwsc` | Known-good full package |
| `firmware/app/known-good/felucca-ota.bin` | Known-good update loader for `pack_with_felucca_ota.py` |
| `firmware/app/known-good/transit-0.1.3.fwsc` | TransitFM1 stopover |
| `firmware/app/pack_with_felucca_ota.py` | Pack app.bin + Felucca ota |
| `firmware/app/minboot.c`, `build_minboot.sh` | Minimal probe sources |

## Next steps

### A. Recover the unit (blocking)

1. Force **`WL82 UBOOT1.00`** with a USB_KEY tool (lilfm1buddy / Flipper / Transporter).  
2. Admin PowerShell: `py fm1_unbrick.py restore FM-1.fwsc` (official V15).  
3. Confirm stock UI; confirm **no** OK `WL82 UBOOT1.00`.

### B. Re-flash BeatBax carefully

1. Install **TransitFM1** (`app/known-good/transit-0.1.3.fwsc`).  
2. Install **`app/build/beatbax-fm1-001.fwsc`** (rebuild first if the tree moved).  
3. Expect **`BEAT 001`** diagnostic; USB product **BeatBax FM-1**.  
4. Fill [`checklist-001.md`](checklist-001.md).

### C. Process / firmware follow-ups

1. **Never again** flash a USB-less image without a hardware escape (e.g. hold PLAY at boot → `fm1_enter_uboot`, or keep MIDI+OTA).  
2. Validate or replace **`wrap_ota.py`** against Felucca’s `ota.bin` on hardware.  
3. Optionally re-test **upstream hwtest** with Felucca ota + screen-before-guards pattern.  
4. Document Windows install: fm-static / Felucca installer + TransitFM1; pygame MIDI if using `fm1_install.py`.  
5. Keep `app/known-good/` out of `build.sh` cleans (already moved off `build/`).

### D. Spec gate

Feature 001 stays **in progress** until checklist rows (especially package/install, screen, USB, OCT− rescue, stock reinstall) are recorded after a successful BeatBax run.

## 2026-11-15 — root cause and recovery plan

### Why the earlier builds went "black screen → UBOOT"

The failing images (hwtest and the first bring-up builds) all did three things the
working `minboot` does not: enable the hardware guards in `fm1_cstart()` before any
screen, run a boot-fail counter that calls `fm1_enter_uboot()` after two failed
boots, and probe the flash / start USB / audio. When any of those faulted or hung
during early init — before `guard.pending` was cleared — `fm1_fault()` rebooted, the
`.noinit` guard remembered `pending=1`, two boots bumped `failed` to 2, and the third
boot jumped straight into ROM download mode (`WL82 UBOOT1.00`, black screen).

The status doc named the **guards** as the likely culprit, but that is **not yet
isolated**: minboot also skips the **flash probe** (`fl_jedec_ram` / `ota_boot_cleanup`)
and **USB/OTA/audio**, so the fault could equally be the flash path on the newer
**UC25HQ80 (0xB36014)** part (this unit differs from hwtest's "0x856014 every FM-1 has").

### The bricking bug that also mattered

The post-fix `bringup.c` cleared `guard.pending` **before** the flash probe and
`usb_start()`. A hang there would then produce a **USB-less crash loop with no UBOOT
escape** — the exact "stranded" state we are trying to avoid. Fixed: the healthy point
now lives in the main loop, ~2 s after USB and the 10 kHz tick are proven alive.

### Fixes in `firmware/app/bringup.c`

1. **Healthy point relocated** into the main loop (after USB + tick). Early faults
   again count toward the two-boot UBOOT fallback.
2. **PLAY-hold rescue**: hold PLAY at power-on → `fm1_enter_uboot()`. Deterministic,
   works even when the two-boot counter is cleared by a later crash loop.
3. **Boot-stage indicator** (5 segments, top-right): 0 screen, 1 flash, 2 USB,
   3 guards+IRQs, 4 audio. A test that ends in UBOOT shows which stage it reached.

### How to use it (after recovering the unit, below)

Rebuild `beatbax-fm1-001.bin`, re-package with Felucca's ota
(`pack_with_felucca_ota.py … --product FM-1_1001`), and flash with **M-Flasher**.
Expected: `BEAT 001` screen, all five stage segments lit green, USB product
`BeatBax FM-1`. If it instead reaches stage **N** and then UBOOT after two boots, the
faulting subsystem is the step after **N**:

| Last green segment | Next step to suspect |
|--------------------|----------------------|
| 0 | flash probe / cleanup |
| 1 | USB start |
| 2 | guards / IRQ enable |
| 3 | audio init |
| 4 | (running) — a later main-loop fault |

No brick is possible: REC-hold (bootloader) and PLAY-hold both land in a recoverable
state, and the running app keeps USB MIDI + OTA.

### Root cause found (2026-11-15)

The boot canary (`firmware/app/canary.c`) isolated **two** faults:

1. **Audio IRQ attach after `guard_lock_top()`.** `fm1_audio_init()` calls
   `fm1_irq_attach(FM1_IRQ_ALNK0, …)`, which writes the vector table at `0x01C7FE00`;
   `fm1_guard_lock_top()` had already write-protected `0x01C7FD50..0x01C7FFFF`, so the
   attach tripped the write guard → CPU exception → two-boot UBOOT. Fixed: attach ALNK0
   **before** `fm1_guard_lock_top()`.
2. **`.ram_text` tone callback called directly from XIP.** `tone_fill` lives in SRAM
   (`.ram_text`), but `fm1_alnk0_irq` (XIP) called it directly; the pi32 `call` range is
   only 23-bit, so the relocation (`R_PI32V2_LONG_JUMP_23M2`) fails to link unless the
   compiler happens to inline it (which silently ran the callback from flash, violating
   the "hot audio from SRAM" rule). Fixed: `tone_fill` is `RAMFN` (`noinline`) and called
   through `FL_FAR()`.

**Result: the bring-up image now boots** — `BEAT 001` diagnostic, controls update the
screen, USB enumerates. The title glyph for `T` was also corrected (it was drawn as `H`).

**Note:** the two-boot → UBOOT guard is **disabled** in `bringup.c` — it fires falsely on
this unit even with a clean counter (the device boots fine with it off). Recovery is via
REC-hold (m-bootloader) or PLAY-hold. Investigate the guard before re-enabling.

## Quick reference — PnP

```powershell
Get-PnpDevice | Where-Object { $_.FriendlyName -match 'Felucca|BeatBax|FM-1|UBOOT|Midi' } |
  Format-Table Status, Class, FriendlyName -AutoSize
Get-CimInstance Win32_DiskDrive | Format-Table Index, Model
```

| OK device | Meaning |
|-----------|---------|
| `WL82 UBOOT1.00` | ROM download — use Unbricker |
| `Felucca Update` / `ota-…` | Update loader — re-run same install, do not double power-cycle |
| `BeatBax FM-1` / Felucca / `FM-1 Midi` | App running |
| Status **Unknown** | Ignore (Windows ghosts) |
