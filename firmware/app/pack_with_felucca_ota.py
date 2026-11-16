#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package an app.bin with Felucca 1.7.0's known-good ota.bin via fm1pkg."""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FW = HERE.parent
sys.path.insert(0, str(FW / "third_party" / "felucca-bsp" / "tools"))
import fm1pkg  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("app", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--product", default="FM-1_1000")
    ap.add_argument(
        "--ota",
        type=Path,
        default=HERE / "known-good" / "felucca-ota.bin",
    )
    a = ap.parse_args()
    if not a.ota.is_file():
        raise SystemExit(f"missing {a.ota}: extract from felucca-1.7.0.fwsc first")
    os.environ.setdefault("AC79_SDK", str(Path.home() / ".jieli" / "ac79-sdk"))
    pkg = fm1pkg.ufw(
        fm1pkg.flash_image(a.app.read_bytes(), fm1pkg.KEY),
        a.ota.read_bytes(),
        a.product,
    )
    a.out.write_bytes(pkg)
    print(f"package {a.out}: {len(pkg)} B, identity {a.product} (Felucca ota)")


if __name__ == "__main__":
    main()
