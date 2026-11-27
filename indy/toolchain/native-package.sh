#!/usr/bin/env bash
# Packs the native GCC of native.sh into the hpkg gcc_bootstrap. The binutils
# come from the package binutils_bootstrap of the repository, so the files of
# native.sh binutils are left out.
#
# Usage: indy/toolchain/native-package.sh [OUTPUT_DIR]
# Run inside the devshell (nix develop), after native.sh all.
set -euo pipefail

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT="${HAIKU_TOOLCHAIN:-/mnt/europa/haiku-toolchain}"
BOOTSTRAP="${BOOTSTRAP_OUTPUT:-$ROOT/bootstrap}"
TARGET=mips-unknown-haiku
STAGE="$ROOT/native/stage/boot/system"
WORK="$ROOT/native/package"
OUT="${1:-$ROOT/native}"
VERSION=17.0.0
PACKAGE_TOOL="$BOOTSTRAP/objects/linux/x86_64/release/tools/package/package"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:+$LD_LIBRARY_PATH:}$BOOTSTRAP/objects/linux/lib"
STRIP="$ROOT/cross-tools-mips/bin/$TARGET-strip"

rm -rf "$WORK"
mkdir -p "$WORK"
cp -a "$STAGE"/. "$WORK"/
# the binutils package has these
for tool in addr2line ar as c++filt elfedit ld ld.bfd nm objcopy objdump ranlib \
		readelf size strings strip; do
	rm -f "$WORK/bin/$tool"
done
rm -rf "$WORK/$TARGET" "$WORK/share/locale"
# no documentation, no test files of the plugin interface
rm -rf "$WORK/share/info" "$WORK/share/man" "$WORK/lib/gcc/$TARGET/$VERSION/testsuite"

# strip the programs and libraries
find "$WORK" -type f \( -perm -u+x -o -name '*.so*' \) | while read -r file; do
	if head -c 4 "$file" | grep -q ELF; then
		"$STRIP" --strip-unneeded "$file" 2>/dev/null || true
	fi
done

cat > "$WORK/.PackageInfo" <<INFO
name			gcc_bootstrap
version			${VERSION}_indy-1
architecture		mips
summary			"GNU C/C++ compiler and binutils for Muse"
description		"GCC 17 and binutils 2.47 for mips-unknown-haiku, built to run on the target. Compiles for n32, MIPS III, tuned for the R4600."
packager		"irix7 <irix7@users.noreply.github.com>"
vendor			"Muse"
copyrights		"Free Software Foundation, Inc."
licenses {
	"GNU GPL v3"
	"GNU LGPL v3"
}
provides {
	gcc_bootstrap = $VERSION
	gcc = $VERSION
	cmd:gcc = $VERSION
	cmd:cc = $VERSION
	cmd:c++ = $VERSION
	cmd:g++ = $VERSION
	cmd:cpp = $VERSION
	cmd:gcov = $VERSION
}
requires {
	haiku
	binutils_bootstrap
	lib:libstdc++
	lib:libgcc_s
}
INFO

rm -f "$OUT/gcc_bootstrap-${VERSION}_indy-1-mips.hpkg"
"$PACKAGE_TOOL" create -C "$WORK" "$OUT/gcc_bootstrap-${VERSION}_indy-1-mips.hpkg"
ls -l "$OUT/gcc_bootstrap-${VERSION}_indy-1-mips.hpkg"
