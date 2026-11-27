#!/usr/bin/env bash
# Fills the seed directory that stands in for the gcc_syslibs_devel and zlib
# HaikuPorts packages on mips (build/jam/BuildFeatures). The kernel and the
# boot loader need libgcc, libsupc++, the compiler's headers and the zlib
# sources.
#
# Usage: indy/toolchain/seed.sh
# Needs the cross toolchain of build.sh.
set -euo pipefail

ROOT="${HAIKU_TOOLCHAIN:-/mnt/europa/haiku-toolchain}"
PREFIX="$ROOT/cross-tools-mips"
SEED="${HAIKU_MIPS_SEED_DIR:-$ROOT/seed}"
TARGET=mips-unknown-haiku
GCC="$PREFIX/bin/$TARGET-gcc"
ZLIB_VERSION="${ZLIB_VERSION:-1.3.1}"

library() {
	# library FLAGS NAME: path of a library of the multilib FLAGS selects
	local path
	path="$("$GCC" $1 -print-file-name="$2")"
	if [ ! -f "$path" ]; then
		echo "seed.sh: $2 not found for flags \"$1\"" >&2
		exit 1
	fi
	echo "$path"
}

rm -rf "$SEED"
mkdir -p "$SEED/lib" "$SEED/headers" "$SEED/zlib"

# userland: the default (hard-float) multilib
cp "$(library "" libgcc.a)" "$SEED/lib/libgcc.a"
cp "$(library "" libgcc_eh.a)" "$SEED/lib/libgcc_eh.a" 2>/dev/null \
	|| "$PREFIX/bin/$TARGET-ar" rcs "$SEED/lib/libgcc_eh.a"
cp "$(library "" libstdc++.a)" "$SEED/lib/libstdc++.a"
cp "$(library "" libsupc++.a)" "$SEED/lib/libsupc++.a"

# kernel and boot loader: the libraries of "build.sh kernel-libs"
KERNEL_LIBS="$ROOT/build/kernel-libs"
for library in libgcc libgcc_eh libsupc++; do
	if [ ! -f "$KERNEL_LIBS/$library-kernel.a" ]; then
		echo "seed.sh: $KERNEL_LIBS/$library-kernel.a is missing;" \
			"run build.sh kernel-libs" >&2
		exit 1
	fi
	cp "$KERNEL_LIBS/$library-kernel.a" "$SEED/lib/$library-kernel.a"
	cp "$KERNEL_LIBS/$library-kernel.a" "$SEED/lib/$library-boot.a"
done

# headers: libstdc++ and the compiler's own
version="$("$GCC" -dumpversion)"
cp -r "$PREFIX/$TARGET/include/c++/$version" "$SEED/headers/c++"
mkdir -p "$SEED/headers/gcc"
cp -r "$PREFIX/lib/gcc/$TARGET/$version/include" "$SEED/headers/gcc/include"
if [ -d "$PREFIX/lib/gcc/$TARGET/$version/include-fixed" ]; then
	cp -r "$PREFIX/lib/gcc/$TARGET/$version/include-fixed" \
		"$SEED/headers/gcc/include-fixed"
fi

# zlib sources
archive="$ROOT/build/zlib-$ZLIB_VERSION.tar.gz"
mkdir -p "$ROOT/build"
if [ ! -f "$archive" ]; then
	curl -fL -o "$archive" \
		"https://github.com/madler/zlib/releases/download/v$ZLIB_VERSION/zlib-$ZLIB_VERSION.tar.gz"
fi
tar -xzf "$archive" -C "$SEED/zlib"
mv "$SEED/zlib/zlib-$ZLIB_VERSION" "$SEED/zlib/sources"

echo "seed.sh: seed directory $SEED"
ls "$SEED/lib"
