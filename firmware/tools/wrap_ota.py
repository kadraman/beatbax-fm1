#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Wrap a raw update-loader RAM image (loader.bin) as JieLi ota.bin for fm1pkg.py.

Layout matches Felucca tools/build.py ldr_wrap(): outer JLFS head + inner head
(load address 0x01C0A800) + LZ4 blocks. Uses firmware/tools/lz4blk.py
(Felucca, GPL-3.0-only).
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lz4blk  # noqa: E402

LOADER_LOAD = 0x01C0A800
LOADER_NAME = b"usb_hid_ota.bin"


def crc16(d: bytes, c: int = 0) -> int:
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else c << 1
        c &= 0xFFFF
    return c


def ldr_head(dcrc: int, a: int, b: int, attr: int, name: bytes) -> bytes:
    body = struct.pack("<HIIBBH16s", dcrc, a, b, attr, 0, 0, name)
    return struct.pack("<H", crc16(body)) + body


def ldr_wrap(image: bytes) -> bytes:
    blocks = bytearray()
    for i in range(0, len(image), 4096):
        raw = image[i : i + 4096]
        c = lz4blk.compress(raw)
        if len(c) > 4096 or lz4blk.decompress(c) != raw:
            raise SystemExit(f"wrap_ota: block {i // 4096} does not compress below 4096 B")
        blocks += struct.pack("<II", len(c), len(raw)) + c
    inner = ldr_head(crc16(image), len(image), LOADER_LOAD, 0, LOADER_NAME)
    body = inner + bytes(blocks)
    return ldr_head(crc16(body), 0x20, len(body), 0x41, LOADER_NAME) + body


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("loader_bin", type=Path)
    ap.add_argument("ota_bin", type=Path)
    a = ap.parse_args()
    image = a.loader_bin.read_bytes()
    if not image or len(image) > 0x14000:
        raise SystemExit(f"wrap_ota: image empty or too big ({len(image)} B)")
    ota = ldr_wrap(image)
    a.ota_bin.write_bytes(ota)
    print(f"ota.bin: {len(ota)} B (loader image {len(image)} B @ {LOADER_LOAD:#x})")


if __name__ == "__main__":
    main()
