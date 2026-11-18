#!/usr/bin/env bash
# Builds the shared GCC libraries for mips and packages them as gcc_syslibs
# and gcc_syslibs_devel (task M5.4 of indy-plan.md). The bootstrap build
# (haikuporter) takes these two packages as its stage 0, so it does not build
# GCC itself.
#
# Usage: indy/toolchain/syslibs.sh OUTPUT_DIR
#   OUTPUT_DIR  the Haiku build directory (generated.mips): it provides the
#               stub libroot and the "package" tool.
#
# This script has not been run. It records the steps; expect to adjust them.
set -euo pipefail

OUTPUT="${1:?usage: syslibs.sh OUTPUT_DIR}"
TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT="${HAIKU_TOOLCHAIN:-/mnt/europa/haiku-toolchain}"
PREFIX="$ROOT/cross-tools-mips"
SYSROOT="$PREFIX/sysroot"
TARGET=mips-unknown-haiku
JOBS="${JOBS:-$(nproc)}"
STAGE="$ROOT/build/syslibs"

# 1. The target libraries link against libroot. Build the stub libroot and
#    put it and the start files into the sysroot.
(cd "$OUTPUT" && jam -q "<bootstrap>libroot_stubs.so" crti.o crtn.o \
	start_dyn.o init_term_dyn.o)
lib="$SYSROOT/boot/system/develop/lib"
mkdir -p "$lib"
find "$OUTPUT/objects" -name 'libroot_stubs.so' -exec cp {} "$lib/libroot.so" \;
for object in crti.o crtn.o start_dyn.o init_term_dyn.o; do
	find "$OUTPUT/objects" -name "$object" -exec cp {} "$lib/" \;
done

# 2. Rebuild the target libraries with shared libraries enabled.
rm -rf "$ROOT/build/gcc-shared" "$STAGE"
mkdir -p "$ROOT/build/gcc-shared"
cd "$ROOT/build/gcc-shared"
export PATH="$PREFIX/bin:$PATH"
"$ROOT/src/gcc/configure" \
	--prefix=/boot/system --target=$TARGET \
	--disable-nls --enable-shared --with-system-zlib \
	--enable-languages=c,c++ --enable-__cxa-atexit --enable-threads=posix \
	--with-default-libstdcxx-abi=gcc4-compatible \
	--with-sysroot="$SYSROOT" --disable-maintainer-mode \
	--disable-libgomp --disable-libatomic --disable-libsanitizer \
	--disable-libquadmath --disable-libssp --disable-libvtv \
	--enable-multilib \
	--with-arch=mips3 --with-tune=r4600 --with-abi=n32 --with-float=hard \
	--with-gmp="$GMP_PREFIX" --with-mpfr="$MPFR_PREFIX" \
	--with-mpc="$MPC_PREFIX" --with-isl="$ISL_PREFIX"
make -j"$JOBS" all-gcc all-target-libgcc all-target-libstdc++-v3 MAKEINFO=true
make DESTDIR="$STAGE/install" install-target-libgcc \
	install-target-libstdc++-v3 MAKEINFO=true

# 3. Lay out the two packages as build/jam/BuildFeatures expects them and
#    create them with the host "package" tool. The .PackageInfo files follow
#    the gcc_syslibs recipe of HaikuPorts.
version="$("$PREFIX/bin/$TARGET-gcc" -dumpversion)"
installed="$STAGE/install/boot/system"
base="$STAGE/gcc_syslibs"
devel="$STAGE/gcc_syslibs_devel"
softFloat="$PREFIX/lib/gcc/$TARGET/$version/soft-float"
mkdir -p "$base/lib" "$devel/develop/lib" "$devel/develop/headers"

cp -a "$installed"/lib/libgcc_s.so* "$installed"/lib/libstdc++.so* \
	"$installed"/lib/libsupc++.so* "$base/lib/" 2>/dev/null || true
cp "$installed"/lib/libstdc++.a "$installed"/lib/libsupc++.a \
	"$installed/lib/gcc/$TARGET/$version/libgcc.a" \
	"$installed/lib/gcc/$TARGET/$version/libgcc_eh.a" "$devel/develop/lib/"
for variant in kernel boot; do
	cp "$softFloat/libgcc.a" "$devel/develop/lib/libgcc-$variant.a"
	cp "$softFloat/libsupc++.a" "$devel/develop/lib/libsupc++-$variant.a" \
		2>/dev/null || cp "$PREFIX/$TARGET/lib/soft-float/libsupc++.a" \
			"$devel/develop/lib/libsupc++-$variant.a"
done
cp "$softFloat/libgcc_eh.a" "$devel/develop/lib/libgcc_eh-boot.a"
cp -r "$installed/include/c++/$version" "$devel/develop/headers/c++"
mkdir -p "$devel/develop/headers/gcc"
cp -r "$PREFIX/lib/gcc/$TARGET/$version/include" \
	"$devel/develop/headers/gcc/include"

package_info() {
	# package_info DIRECTORY NAME SUMMARY
	cat >"$1/.PackageInfo" <<INFO
name			$2
version			${version}_indy-1
architecture	mips
summary			"$3"
description		"$3, built from the GCC 17 fork for the SGI Indy port."
packager		"Haiku Indy port"
vendor			"Haiku Project"
licenses {
	"GNU GPL v3"
	"GNU LGPL v3"
}
copyrights {
	"Free Software Foundation, Inc."
}
provides {
	$2 = ${version}_indy-1
}
urls {
	"https://gcc.gnu.org"
}
INFO
}

package_info "$base" gcc_syslibs "GCC runtime libraries"
package_info "$devel" gcc_syslibs_devel "GCC runtime libraries, development files"

packageTool="$(find "$OUTPUT/objects" -type f -name package -path '*tools*' | head -1)"
for name in gcc_syslibs gcc_syslibs_devel; do
	(cd "$STAGE/$name" && "$packageTool" create -q \
		"$STAGE/$name-${version}_indy-1-mips.hpkg")
done

echo "syslibs.sh: packages in $STAGE"
ls -l "$STAGE"/*.hpkg
