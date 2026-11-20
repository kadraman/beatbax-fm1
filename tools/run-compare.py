#!/usr/bin/env python3
"""
Feature 003 Phase 2 comparison harness (T020-T022).

For each fixture, render with the fixed-point host renderer (tools/render-bax),
export the reference with the BeatBax CLI (`beatbax export wav`), and compare via
normalized Pearson correlation over the trimmed non-silent region. Exit non-zero
if any fixture falls below the documented tolerance (0.95).

Usage: run-compare.py [--threshold T] [fixture ...]
Defaults to the three shipped fixtures that define the SC-002 acceptance set.
"""
import os
import subprocess
import sys
import wave
import struct
import math

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_FIXTURES = [
    "songs/gameboy/tutorial_groove.bax",
    "songs/gameboy/gb_sweep_demo.bax",
    "songs/gameboy/gb_uge_note_demo.bax",
]
THRESHOLD = 0.95
SAMPLE_RATE = 44100


def read_mono(path):
    with wave.open(path, "rb") as w:
        n = w.getnframes()
        ch = w.getnchannels()
        sw = w.getsampwidth()
        data = w.readframes(n)
    if sw != 2:
        raise ValueError("bad sampwidth %d" % sw)
    samples = struct.unpack("<%dh" % (n * ch), data)
    if ch == 1:
        return list(samples)
    return [(samples[i * 2] + samples[i * 2 + 1]) // 2 for i in range(n)]


def correlation(a, b):
    thr = 200
    L = min(len(a), len(b))
    # Reject unmatched non-silent audio beyond the shorter recording (missing
    # endings or unintended extra audio must not pass with perfect overlap).
    longer = a if len(a) > len(b) else b
    if any(abs(x) >= thr for x in longer[L:]):
        return 0.0
    a = a[:L]
    b = b[:L]
    start = 0
    while start < L and abs(b[start]) < thr:
        start += 1
    end = L
    while end > start and abs(b[end - 1]) < thr:
        end -= 1
    if end - start < 1000:
        return 0.0
    x = a[start:end]
    y = b[start:end]
    mx = sum(x) / len(x)
    my = sum(y) / len(y)
    num = sum((xx - mx) * (yy - my) for xx, yy in zip(x, y))
    dx = math.sqrt(sum((xx - mx) ** 2 for xx in x))
    dy = math.sqrt(sum((yy - my) ** 2 for yy in y))
    return num / (dx * dy) if dx > 0 and dy > 0 else 0.0


def run(cmd):
    use_shell = os.name == "nt"
    return subprocess.run(cmd, cwd=REPO_ROOT, shell=use_shell, capture_output=True, text=True)


def compare_fixture(fixture, render_bin, tmpdir, threshold):
    stem = os.path.splitext(os.path.basename(fixture))[0]
    fm1 = os.path.join(tmpdir, stem + "_fm1.wav")
    ref = os.path.join(tmpdir, stem + "_bb.wav")

    r1 = run([render_bin, fixture, fm1, str(SAMPLE_RATE)])
    if r1.returncode != 0:
        return ("ERROR", 0.0, r1.stderr.strip() or "render failed")
    r2 = run(["beatbax", "export", "wav", fixture, "-o", ref])
    if r2.returncode != 0:
        return ("ERROR", 0.0, r2.stderr.strip() or "beatbax export failed")

    corr = correlation(read_mono(fm1), read_mono(ref))
    status = "PASS" if corr >= threshold else "FAIL"
    return (status, corr, "")


def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="Render fixtures with the host renderer and compare to BeatBax CLI.")
    parser.add_argument("--threshold", type=float, default=THRESHOLD,
                        help="minimum normalized correlation (default %.2f)" % THRESHOLD)
    parser.add_argument("fixtures", nargs="*",
                        help=".bax fixtures to compare (default: the SC-002 acceptance set)")
    args = parser.parse_args()
    threshold = args.threshold
    fixtures = args.fixtures if args.fixtures else list(DEFAULT_FIXTURES)

    render_bin = os.path.join(REPO_ROOT, "tools", "render-bax")
    if os.name == "nt":
        render_bin += ".exe"
    if not os.path.exists(render_bin):
        print("render-bax binary not found (build with `make render-bax` or `make test-compare`).", file=sys.stderr)
        return 2

    print("=== BeatBax FM-1 renderer comparison (threshold %.2f) ===" % threshold)
    tmpdir = os.path.join(REPO_ROOT, ".cmp-tmp")
    os.makedirs(tmpdir, exist_ok=True)
    failed = 0
    for fixture in fixtures:
        status, corr, msg = compare_fixture(fixture, render_bin, tmpdir, threshold)
        print("%-6s %-45s corr=%.4f" % (status, fixture, corr))
        if msg:
            print("       %s" % msg, file=sys.stderr)
        if status != "PASS":
            failed += 1

    print()
    if failed:
        print("FAILED: %d fixture(s) below threshold %.2f" % (failed, threshold))
        return 1
    print("SUCCESS: all %d fixture(s) pass correlation >= %.2f" % (len(fixtures), threshold))
    return 0


if __name__ == "__main__":
    sys.exit(main())
