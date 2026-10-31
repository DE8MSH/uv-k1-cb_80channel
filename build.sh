#!/usr/bin/env bash
# Builds build/CBSkaner.app, an overlay app for the F4HWN Labs firmware.
#
# The app is compiled against the F4HWN overlay-app API. Its headers and
# packer are not part of this repository: they are fetched from the F4HWN
# repository at F4HWN_REF into build/f4hwn/, or taken from a local checkout
# given as F4HWN_DIR=<firmware repo>.
#
# Needs arm-none-eabi-gcc (Arm GNU Toolchain) on PATH or ARM_TOOLCHAIN=<dir
# with bin/>, python3, and curl when fetching.
set -euo pipefail
cd "$(dirname "$0")"

APP=cbskaner
APP_NAME="CBSkaner"
APP_VER="1.0"
APP_API_MIN=3
APP_VMA=${APP_VMA:-0x20000280}           # overlay address of F4HWN v6.3.0
F4HWN_REPO=armel/uv-k1-k5v3-firmware-custom
F4HWN_REF=${F4HWN_REF:-5a13304d60f850b03b3965ca0e27e0ddfe998ec0}   # v6.3.0+
F4HWN_FILES="app_api.h app_overlay.h pack_app.py"
OUT=build
API="$OUT/f4hwn"

if [ -n "${ARM_TOOLCHAIN:-}" ]; then PATH="$ARM_TOOLCHAIN/bin:$PATH"; fi
CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
command -v "$CC" >/dev/null 2>&1 || { echo "❌ $CC not found (set ARM_TOOLCHAIN)"; exit 1; }

rm -rf "$OUT" && mkdir -p "$API"

for f in $F4HWN_FILES; do
    if [ -n "${F4HWN_DIR:-}" ]; then
        cp "$F4HWN_DIR/App/apps/$f" "$API/$f"
    else
        curl -fsSL -o "$API/$f" \
            "https://raw.githubusercontent.com/$F4HWN_REPO/$F4HWN_REF/App/apps/$f" \
            || { echo "❌ cannot fetch $f from $F4HWN_REPO@$F4HWN_REF"; exit 1; }
    fi
done

CFLAGS="-mcpu=cortex-m0plus -mthumb -Os -std=gnu11 -ffreestanding -fno-builtin -fno-common \
  -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-jump-tables -Wall -Wextra -I$API -DAPP_VERSION=\"${APP_VER}\""
LDFLAGS="-nostdlib -nostartfiles -T app.ld -Wl,--defsym,APP_VMA=${APP_VMA} \
  -Wl,--gc-sections -Wl,-Map=${OUT}/${APP}.map -Wl,--build-id=none -Wl,--no-warn-rwx-segments"

"$CC" $CFLAGS $LDFLAGS "src/${APP}_app.c" -lgcc -o "${OUT}/${APP}.elf"
"$OBJCOPY" -O binary "${OUT}/${APP}.elf" "${OUT}/${APP}.bin"
python3 "$API/pack_app.py" "${OUT}/${APP}.bin" "${OUT}/${APP_NAME// /}.app" \
    --name "$APP_NAME" --ver "$APP_VER" --api-min "$APP_API_MIN" --vma "$APP_VMA" >/dev/null

BYTES=$(wc -c < "${OUT}/${APP}.bin")
if [ "$BYTES" -gt 4096 ]; then
    echo "🚨 ${APP_NAME}: ${BYTES} B, over the 4 KiB overlay"; exit 1
fi
echo "✅ ${APP_NAME} ${APP_VER}: ${BYTES} B ($(( BYTES * 100 / 4096 ))% of 4 KiB) -> ${OUT}/${APP_NAME// /}.app"
