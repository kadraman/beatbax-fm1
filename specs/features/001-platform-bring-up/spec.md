---
title: "Platform bring-up"
id: 1
slug: "platform-bring-up"
status: draft
authors: ["kadraman"]
created: 2026-11-11
updated: 2026-11-11
issue: "https://github.com/kadraman/beatbax-fm1/issues/1"
area: firmware
---

# Feature Specification: Platform bring-up

## Summary

Pin Felucca as the FM-1 HAL and ship a minimal firmware that boots, paints one diagnostic screen, scans every physical control, emits a test tone within the audio budget, enumerates USB, and documents OCT- rescue plus return to stock — before any song work.

## Problem

Without a proven board bring-up, later Game Boy player and composer work has no safe base. Community firmwares show Felucca is the redistributable path; stock `fm1_open` uses a binary SDK unsuitable as our synth engine. Developers need recovery documented before flashing becomes routine.

## User scenarios

### User Story 1 — Boot diagnostic firmware (Priority: P1)

**Why this priority**: Establishes the only path for all later device features.

**Independent test**: Flash the bring-up image; observe boot screen, control feedback, tone, and USB enumeration; recover via OCT- if needed.

**Acceptance scenarios**:

1. **Given** a Windows host with the documented JieLi pi32 toolchain and pinned Felucca, **When** the developer builds feature 001 firmware, **Then** the build produces an installable package without the stock binary SDK.
2. **Given** the bring-up firmware installed, **When** the device powers on, **Then** a single diagnostic screen paints and remains responsive.
3. **Given** the diagnostic is running, **When** each Felucca-mapped control is actuated, **Then** the screen reflects that control (or a complete control list updates).
4. **Given** the diagnostic is running, **When** the user triggers the documented tone action, **Then** a test tone plays within the [audio budget](../../global/audio.md).
5. **Given** the device is connected by USB, **When** the host enumerates devices, **Then** the bring-up firmware is visible as documented (MIDI and/or CDC as the plan chooses, consistently).

### User Story 2 — Recovery and return to stock (Priority: P1)

**Why this priority**: Flashing without recovery is unsafe.

**Independent test**: Follow the recovery doc from a cold start with OCT- held; install stock or a known-good package.

**Acceptance scenarios**:

1. **Given** the device is powered off, **When** the user holds OCT- alone while powering on, **Then** the device enters the documented USB rescue / updater mode.
2. **Given** rescue mode, **When** the user runs the documented install steps for stock or BeatBax FM-1, **Then** the device boots that firmware.
3. **Given** the README / docs, **When** a new developer reads recovery guidance, **Then** backup-before-switch and format non-migration warnings are present.

### Edge cases

- What happens when the toolchain path is unset?
- What happens when Felucca pin and local checkout disagree?
- What happens if USB fails to enumerate after flash?

## Requirements

### Functional requirements

- **FR-001**: System MUST pin an exact Felucca git commit and record it in docs and release metadata.
- **FR-002**: System MUST build on Windows with the JieLi pi32 clang toolchain family; document version and environment variables.
- **FR-003**: System MUST paint one diagnostic UI screen on the 240×240 LCD.
- **FR-004**: System MUST scan every control Felucca’s panel map reports and show activity on screen.
- **FR-005**: System MUST emit a test tone from the audio callback without flash erase, drawing, or USB waits in that callback.
- **FR-006**: System MUST enumerate on USB as documented for this phase.
- **FR-007**: Documentation MUST describe OCT- rescue and return to stock before song features land.
- **FR-008**: System MUST NOT depend on the stock `fm1_open` binary synth SDK.

### Non-goals

- Song model, `.bax` parse, Game Boy voices, composer screens, flash song store.
- fm1-emu CI gate (may be noted as a later test gate once the build is real).
- Bluetooth / Wi-Fi.
- Second core.

## Success criteria

- **SC-001**: A developer following docs can build and install bring-up firmware on a real FM-1.
- **SC-002**: Diagnostic proves screen, all controls, tone, and USB in one session.
- **SC-003**: Recovery steps are written and exercised at least once on maintainer hardware before calling the phase `complete`.

## Assumptions

- Felucca remains publicly available at the pinned commit.
- Maintainer has (or will obtain) an FM-1 and JieLi toolchain for the hardware checklist.

## Open questions

- Exact USB identity (MIDI-only vs MIDI+CDC) for bring-up — decide in `plan.md`.
- Whether a host simulator stub ships in 001 or waits until after first hardware flash.

## References

- [specs/global/hardware.md](../../global/hardware.md)
- [specs/global/audio.md](../../global/audio.md)
- [Felucca](https://github.com/hugelton/Felucca)
- [fm1_open](https://github.com/m-vave/fm1_open) (toolchain lessons only)
- [fm1-nes](https://github.com/Keitark/fm1-nes) (recovery patterns only)
