#!/usr/bin/env bash
# Builds the programs of indy/guest for the target with the cross compiler.
# Usage: indy/guest/build.sh [OUTPUT_DIR]   (default generated.mips/guest)
set -euo pipefail
TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT="${HAIKU_TOOLCHAIN:-/mnt/europa/haiku-toolchain}"
OUT="${1:-$TOP/generated.mips/guest}"
SYSROOT="$ROOT/native/sysroot"
mkdir -p "$OUT"
"$ROOT/cross-tools-mips/bin/mips-unknown-haiku-gcc" --sysroot="$SYSROOT" \
	-O2 -I"$SYSROOT/boot/system/develop/headers/os" \
	-I"$SYSROOT/boot/system/develop/headers/posix" \
	-o "$OUT/muse_log" "$TOP/indy/guest/muse_log.c" -lroot
echo "$OUT/muse_log"
