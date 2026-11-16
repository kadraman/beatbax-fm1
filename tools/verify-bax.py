#!/usr/bin/env python3
"""
tools/verify-bax.py
Oracle verification script for BeatBax FM-1.
Runs the C round-trip test harness and verifies that all original and written
canonical fixtures validate with the BeatBax CLI oracle (`beatbax verify`).
"""

import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

def run_cmd(cmd, cwd=REPO_ROOT):
    print(f">> Running: {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    use_shell = True if os.name == "nt" else isinstance(cmd, str)
    res = subprocess.run(cmd, cwd=cwd, shell=use_shell, capture_output=True, text=True)
    if res.stdout:
        print(res.stdout.strip())
    if res.returncode != 0:
        if res.stderr:
            print(res.stderr.strip(), file=sys.stderr)
        print(f"Error: Command failed with exit code {res.returncode}", file=sys.stderr)
        return False
    return True

def main():
    print("=== BeatBax FM-1 Fixture Verification ===")

    # 1. Compile host test harness
    bin_name = "test_roundtrip.exe" if os.name == "nt" else "test_roundtrip"
    test_bin = os.path.join(REPO_ROOT, "host", "tests", bin_name)
    compile_cmd = [
        "gcc", "-std=c99", "-Wall", "-Wextra",
        "-Ihost/song",
        "host/song/song_model.c",
        "host/song/song_parse.c",
        "host/song/song_write.c",
        "host/tests/test_roundtrip.c",
        "-o", test_bin
    ]

    print("\n1. Compiling roundtrip test harness...")
    if not run_cmd(compile_cmd):
        sys.exit(1)

    # 2. Run unit tests & idempotence tests
    print("\n2. Executing roundtrip test suite...")
    if not run_cmd([test_bin]):
        sys.exit(1)

    # 3. Verify fixtures with beatbax CLI oracle
    print("\n3. Verifying fixtures against BeatBax CLI oracle...")
    fixtures = [
        "songs/gameboy/tutorial_groove.bax",
        "songs/gameboy/tutorial_groove.canon.bax",
        "songs/gameboy/gb_sweep_demo.bax",
        "songs/gameboy/gb_sweep_demo.canon.bax",
        "songs/gameboy/gb_uge_note_demo.bax",
        "songs/gameboy/gb_uge_note_demo.canon.bax",
    ]

    for fix in fixtures:
        fix_path = os.path.join(REPO_ROOT, fix)
        if not os.path.isfile(fix_path):
            print(f"Error: Fixture file not found: {fix_path}", file=sys.stderr)
            sys.exit(1)
        if not run_cmd(["beatbax", "verify", fix_path]):
            print(f"Error: Oracle verification failed for {fix}", file=sys.stderr)
            sys.exit(1)

    print("\nSUCCESS: All fixtures passed round-trip idempotence and beatbax verify oracle!")

if __name__ == "__main__":
    main()
