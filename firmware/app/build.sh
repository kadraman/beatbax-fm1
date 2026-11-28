#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build BeatBax FM-1 feature 004 device player firmware.
#
# Prerequisites:
#   export JIELI_TOOLCHAIN=/path/to/jieli/toolchain   # pi32v2 clang + ld + objcopy
#   # Optional for packaging (see ../README.md):
#   export AC79_SDK=/path/to/fw-AC79_AIoT_SDK
#
# Output: build/beatbax-fm1-004.bin
set -e
TC="${JIELI_TOOLCHAIN:?JIELI_TOOLCHAIN must point at the JieLi pi32 toolchain (see firmware/README.md)}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
BSP="$HERE/../third_party/felucca-bsp"
OUT="$HERE/build"
NAME=beatbax-fm1-004

if [ ! -f "$BSP/chip/crt0.S" ]; then
    echo "error: felucca-bsp missing at $BSP — run: git submodule update --init firmware/third_party/felucca-bsp" >&2
    exit 1
fi

mkdir -p "$OUT"
rm -f "$OUT"/*.o 2>/dev/null || true
find "$OUT" -maxdepth 1 -type f ! -path "$OUT/known-good/*" -delete 2>/dev/null || true

CFLAGS="-Os -ffunction-sections -fno-builtin -Wall -Wno-unused-function"
# Shared fixed-point renderer (host/render/gameboy) + compiled song blob.
# `compat/` provides the minimal <string.h> shim (the pi32 toolchain has none).
RENDER_INC="-I$ROOT/host/render/gameboy -I$ROOT/host/song -I$HERE -I$HERE/compat"
# Place the renderer call graph in SRAM (.ram_text): the pi32 `call` range cannot
# span the SRAM<->XIP gap, so hot functions must be co-resident with the callback.
GB_RAMFN_DEF='__attribute__((section(".ram_text"),noinline,used))'
# On-device callback-budget readout (checklist-004 row 11): build with
#   GB_DEBUG_CAPTURE=1 sh build.sh
DEBUG_DEF=""
if [ "${GB_DEBUG_CAPTURE:-0}" = "1" ]; then
    DEBUG_DEF="-DGB_DEBUG_CAPTURE"
fi

cc_pi32() {
    "$TC/pi32v2/bin/clang" -target pi32v2 "$@"
}

cc_pi32 -c "$BSP/chip/crt0.S" -o "$OUT/crt0.o"
cc_pi32 -c "$BSP/chip/fm1_vec.S" -o "$OUT/vec.o"
cc_pi32 -c "$BSP/chip/fm1_isr.S" -o "$OUT/isr.o"
cc_pi32 $CFLAGS $DEBUG_DEF -I"$BSP/chip" -I"$BSP/boards/fm1" $RENDER_INC -c "$HERE/player.c" -o "$OUT/player.o"
cc_pi32 $CFLAGS "-DGB_RAMFN=$GB_RAMFN_DEF" $RENDER_INC -c "$ROOT/host/render/gameboy/gb_period.c" -o "$OUT/gb_period.o"
cc_pi32 $CFLAGS "-DGB_RAMFN=$GB_RAMFN_DEF" $RENDER_INC -c "$ROOT/host/render/gameboy/gb_voice.c" -o "$OUT/gb_voice.o"
cc_pi32 $CFLAGS "-DGB_RAMFN=$GB_RAMFN_DEF" $RENDER_INC -c "$ROOT/host/render/gameboy/gb_macro.c" -o "$OUT/gb_macro.o"
cc_pi32 $CFLAGS "-DGB_RAMFN=$GB_RAMFN_DEF" $RENDER_INC -c "$ROOT/host/render/gameboy/gb_note_render.c" -o "$OUT/gb_note_render.o"
cc_pi32 $CFLAGS "-DGB_RAMFN=$GB_RAMFN_DEF" $RENDER_INC -c "$ROOT/host/render/gameboy/gb_stream.c" -o "$OUT/gb_stream.o"
cc_pi32 $CFLAGS -I"$BSP/chip" -c "$HERE/div64.c" -o "$OUT/div64.o"

"$TC/pi32v2/bin/ld" -T "$HERE/app.ld" "$OUT/crt0.o" "$OUT/vec.o" "$OUT/isr.o" \
    "$OUT/player.o" "$OUT/gb_period.o" "$OUT/gb_voice.o" "$OUT/gb_macro.o" \
    "$OUT/gb_note_render.o" "$OUT/gb_stream.o" "$OUT/div64.o" -o "$OUT/$NAME.elf"
"$TC/common/bin/objcopy" -O binary -j .text "$OUT/$NAME.elf" "$OUT/text.bin"
"$TC/common/bin/objcopy" -O binary -j .data "$OUT/$NAME.elf" "$OUT/data.bin"
"$TC/common/bin/objcopy" -O binary -j .ram_text "$OUT/$NAME.elf" "$OUT/rt.bin"

python3 - "$TC" "$OUT" "$OUT/$NAME.bin" "$NAME" <<'PY'
import re, subprocess, sys
tc, out, dest, name = sys.argv[1:]
elf = f"{out}/{name}.elf"
syms = subprocess.run(
    [f"{tc}/common/bin/objdump", "-t", elf],
    capture_output=True, text=True, check=True,
).stdout
load = int(re.search(r"^([0-9a-f]+) .*\s_data_load$", syms, re.M).group(1), 16)
rt_load = int(re.search(r"^([0-9a-f]+) .*\s_rt_load$", syms, re.M).group(1), 16)
start = int(re.search(r"^([0-9a-f]+) .*\s_start$", syms, re.M).group(1), 16)
assert start == 0x02000120, hex(start)
img = bytearray(open(f"{out}/text.bin", "rb").read())
data = open(f"{out}/data.bin", "rb").read()
rt = open(f"{out}/rt.bin", "rb").read()
for at, part in sorted([(rt_load, rt), (load, data)]):
    if part:
        assert at - 0x02000120 >= len(img), "sections overlap"
        img += b"\xff" * (at - 0x02000120 - len(img)) + part
img += b"\xff" * (-len(img) % 4)
open(dest, "wb").write(img)
print(f"{name}.bin: {len(img)} bytes")
PY

echo "Built $OUT/$NAME.bin"
echo "Package (needs loader + AC79_SDK): see firmware/README.md"
