#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the felucca-bsp update loader and wrap it as ota.bin for fm1pkg.py.
#
#   export JIELI_TOOLCHAIN=~/.jieli/toolchain
#   sh firmware/app/build_loader.sh
#   # -> firmware/app/build/loader.bin and firmware/app/build/ota.bin
set -e
TC="${JIELI_TOOLCHAIN:?JIELI_TOOLCHAIN must point at the JieLi pi32 toolchain}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BSP="$HERE/../third_party/felucca-bsp"
OUT="$HERE/build"
LDR="$BSP/loader"

if [ ! -f "$LDR/loader.c" ]; then
    echo "error: felucca-bsp loader missing — run: git submodule update --init firmware/third_party/felucca-bsp" >&2
    exit 1
fi

mkdir -p "$OUT"
CFLAGS="-Os -ffunction-sections -fno-builtin -Wall -Wno-unused-function"

cc_pi32() {
    "$TC/pi32v2/bin/clang" -target pi32v2 "$@"
}

cc_pi32 -c "$LDR/crt0_ldr.S" -o "$OUT/crt0_ldr.o"
cc_pi32 $CFLAGS -I"$BSP/chip" -I"$BSP/boards/fm1" -c "$LDR/loader.c" -o "$OUT/loader.o"
"$TC/pi32v2/bin/ld" --gc-sections -e _start -T "$LDR/loader.ld" \
    "$OUT/crt0_ldr.o" "$OUT/loader.o" -o "$OUT/loader.elf"
"$TC/common/bin/objcopy" -O binary -j .text "$OUT/loader.elf" "$OUT/loader.bin"

python3 "$HERE/../tools/wrap_ota.py" "$OUT/loader.bin" "$OUT/ota.bin"
echo "Built $OUT/loader.bin and $OUT/ota.bin"
