#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build minboot.bin and package with Felucca's known-good ota.bin.
set -e
TC="${JIELI_TOOLCHAIN:?JIELI_TOOLCHAIN must point at the JieLi pi32 toolchain}"
export AC79_SDK="${AC79_SDK:?AC79_SDK must point at the JieLi AC79 SDK}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BSP="$HERE/../third_party/felucca-bsp"
OUT="$HERE/build"
NAME=minboot

mkdir -p "$OUT"
CC="$TC/pi32v2/bin/clang -target pi32v2"
CFLAGS="-Os -ffunction-sections -fno-builtin -Wall -Wno-unused-function"
INCLUDES="-I$BSP/chip -I$BSP/boards/fm1"

$CC -c "$BSP/chip/crt0.S" -o "$OUT/crt0.o"
$CC -c "$BSP/chip/fm1_vec.S" -o "$OUT/vec.o"
$CC -c "$BSP/chip/fm1_isr.S" -o "$OUT/isr.o"
$CC $CFLAGS $INCLUDES -c "$HERE/minboot.c" -o "$OUT/minboot.o"
"$TC/pi32v2/bin/ld" -T "$HERE/app.ld" "$OUT/crt0.o" "$OUT/vec.o" "$OUT/isr.o" "$OUT/minboot.o" \
    -o "$OUT/$NAME.elf"
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

python3 "$HERE/pack_with_felucca_ota.py" "$OUT/$NAME.bin" "$OUT/$NAME.fwsc" --product FM-1_1000
echo "Built $OUT/$NAME.bin and $OUT/$NAME.fwsc"
echo "Expect a solid green screen. Prefer flashing via TransitFM1."
