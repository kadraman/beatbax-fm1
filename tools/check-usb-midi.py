#!/usr/bin/env python3
"""
Feature 004 hardware checklist helper (row 16): verify USB MIDI enumeration.

Checks that a connected FM-1 enumerates as a class-compliant USB-MIDI device
with the BeatBax product string. Primary signal: list the host's MIDI ports via
`mido` (the same dependency as fm1_install.py) and look for the product string
among inputs and outputs. Fallback (when mido / python-rtmidi is unavailable):
query Windows Plug-and-Play devices via PowerShell.

The FM-1 firmware (firmware/app/usb.c) presents one bidirectional MIDI endpoint
(EP1), so it should appear as BOTH an input and an output port.

Exit codes:
  0  device enumerated as USB MIDI (PASS)
  1  device not found (FAIL)
  2  tooling missing / environment error

Usage: check-usb-midi.py [--product NAME]
"""

import argparse
import os
import subprocess
import sys

PRODUCT = "BeatBax FM-1"
MANUFACTURER = "BeatBax"


def mido_ports():
    """Return (inputs, outputs) port-name lists, or None if mido is unusable."""
    try:
        import mido
    except ImportError:
        return None
    try:
        ins = mido.get_input_names()
        outs = mido.get_output_names()
    except Exception as exc:  # backend unavailable (no rtmidi/pygame/…)
        print("  mido present but no usable backend: %s" % exc, file=sys.stderr)
        return None
    return ins, outs


def windows_pnp():
    """Return PnP friendly names matching BeatBax on Windows, else None."""
    if os.name != "nt":
        return None
    ps = (
        "Get-PnpDevice -PresentOnly | "
        "Where-Object { $_.FriendlyName -like '*BeatBax*' } | "
        "ForEach-Object { $_.FriendlyName + ' [' + $_.Class + ']' }"
    )
    try:
        r = subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command", ps],
            capture_output=True, text=True, timeout=30,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if r.returncode != 0:
        return None
    return [ln.strip() for ln in r.stdout.splitlines() if ln.strip()]


def _match(names, needle):
    n = needle.lower()
    return [name for name in names if n in name.lower()]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--product", default=PRODUCT,
        help="USB product string to look for (default: %(default)r)")
    args = parser.parse_args(argv)
    product = args.product

    print("=== BeatBax FM-1 USB MIDI enumeration check ===")
    print("looking for product string: %r" % product)

    ports = mido_ports()
    if ports is not None:
        ins, outs = ports
        hit_in = _match(ins, product)
        hit_out = _match(outs, product)
        print("MIDI inputs : %s" % (", ".join(ins) if ins else "(none)"))
        print("MIDI outputs: %s" % (", ".join(outs) if outs else "(none)"))
        if hit_in or hit_out:
            print()
            print("PASS: %r present as a MIDI port" % product)
            if hit_in:
                print("  input : %s" % ", ".join(hit_in))
            if hit_out:
                print("  output: %s" % ", ".join(hit_out))
            return 0
        # mido worked but did not list the device: it is not a MIDI port.
        print()
        print("FAIL: %r not found among MIDI ports." % product)
        print("      (Device may be absent, unpowered, or enumerating as a non-MIDI class.)")
        return 1

    # mido unavailable -> best-effort OS-level check.
    print("MIDI ports : (mido / python-rtmidi not available; using OS-level check)")

    pnp = windows_pnp()
    if pnp is not None:
        hit = _match(pnp, product) or _match(pnp, MANUFACTURER)
        print("PnP devices: %s" % (", ".join(pnp) if pnp else "(none)"))
        print()
        if hit:
            print("PASS: USB device %r present (PnP)." % product)
            print("      Install `mido` + `python-rtmidi` for a full MIDI-port check.")
            return 0
        print("FAIL: %r not found among PnP devices." % product)
        return 1

    print()
    print("ERROR: no MIDI backend and no OS-level fallback available.", file=sys.stderr)
    print("       Install `mido` + `python-rtmidi` (see firmware/README.md).", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
