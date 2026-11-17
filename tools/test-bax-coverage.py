#!/usr/bin/env python3
"""tools/test-bax-coverage.py

Machine-checked feature coverage. Each granular fixture in
tools/bax-coverage.json has an expected outcome:

  * "ok"      — parse → write → parse is idempotent AND beatbax verify passes.
  * "refuse"  — the parser fails with a named reason containing `reason`.

This turns "we think we support X" into an assertion CI can run. Exit 0 only
if every fixture matches its expected outcome.
"""
import json
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MATRIX = os.path.join(REPO, "tools", "bax-coverage.json")
BIN = os.path.join(REPO, "host", "tests", "test_roundtrip.exe" if os.name == "nt" else "test_roundtrip")


def run(cmd, cwd=REPO, timeout=30):
    use_shell = os.name == "nt"
    args = " ".join(cmd) if (use_shell and isinstance(cmd, list)) else cmd
    try:
        r = subprocess.run(args, cwd=cwd, capture_output=True, text=True, timeout=timeout,
                           shell=use_shell, encoding="utf-8", errors="replace")
        return r.returncode, (r.stdout or "") + (r.stderr or "")
    except subprocess.TimeoutExpired:
        return -1, "TIMEOUT"


def main():
    # Compile the round-trip harness first (self-contained, like verify-bax.py).
    compile_cmd = [
        "gcc", "-std=c99", "-Wall", "-Wextra", "-Ihost/song",
        "host/song/song_model.c", "host/song/song_parse.c", "host/song/song_write.c",
        "host/tests/test_roundtrip.c", "-o", BIN,
    ]
    code, out = run(compile_cmd)
    if code != 0:
        print("compile failed:\n" + out)
        sys.exit(1)

    with open(MATRIX, "r", encoding="utf-8") as f:
        matrix = json.load(f)

    failures = 0
    ok_count = refuse_count = 0
    for path, spec in matrix.items():
        full = os.path.join(REPO, path)
        code, out = run([BIN, full])
        expected = spec.get("expect")
        if expected == "ok":
            if code != 0:
                print(f"FAIL (ok expected, parse refused): {path}\n  {out.strip().splitlines()[-1] if out.strip() else ''}")
                failures += 1
                continue
            vcode, _ = run(["beatbax", "verify", "custom.canon.bax"])
            if vcode != 0:
                print(f"FAIL (verify): {path}")
                failures += 1
                continue
            ok_count += 1
            print(f"  ok     {path}")
        else:
            reason = spec.get("reason", "")
            if code == 0:
                print(f"FAIL (refuse expected, round-tripped): {path}")
                failures += 1
                continue
            if reason and reason not in out:
                print(f"FAIL (wrong reason for {path}): expected '{reason}' in output")
                failures += 1
                continue
            refuse_count += 1
            print(f"  refuse {path}")

    print(f"\n{ok_count} supported, {refuse_count} refused, {failures} mismatches")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
