#!/usr/bin/env bash
# Builds the shared GCC libraries for mips and packages them as gcc_syslibs
# and gcc_syslibs_devel (task M5.4 of indy-plan.md). The bootstrap build
# (haikuporter) takes these two packages as its stage 0, so it does not build
# GCC itself.
#
# Usage: indy/toolchain/syslibs.sh OUTPUT_DIR [all|package]
#   package     skips the GCC build and packages what the last one installed
#
# Environment:
#   BOOTSTRAP_OUTPUT  build directory configured with --bootstrap: the
#                     gcc_bootstrap_syslibs packages are copied into its
#                     HaikuPortsCross repository
#   OUTPUT_DIR  the Haiku build directory (generated.mips): it provides the
#               stub libroot and the "package" tool.
#
# Run inside the devshell (nix develop).
set -euo pipefail

OUTPUT="${1:?usage: syslibs.sh OUTPUT_DIR [all|package]}"
STEP="${2:-all}"
TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT="${HAIKU_TOOLCHAIN:-/mnt/europa/haiku-toolchain}"
PREFIX="$ROOT/cross-tools-mips"
SYSROOT="$PREFIX/sysroot"
TARGET=mips-unknown-haiku
JOBS="${JOBS:-$(nproc)}"
STAGE="$ROOT/build/syslibs"

export PATH="$ROOT/cross-tools-mips/bin:$PATH"

if [ "$STEP" != package ]; then
# 1. The target libraries link against libroot. Put a stub libroot and the
#    start files into the sysroot. The stub is built here: its jam target
#    exists only in a build configured with --bootstrap.
#    haikuporter removes the sysroot of the cross tools after each port it
#    builds, so the headers are staged here again.
export PATH="$PREFIX/bin:$PATH"
lib="$SYSROOT/boot/system/develop/lib"
include="$SYSROOT/boot/system/develop/headers"
mkdir -p "$lib" "$include" "$STAGE"
for directory in config os posix; do
	rm -rf "$include/$directory"
	cp -r "$TOP/headers/$directory" "$include/$directory"
done
OUTPUT="$OUTPUT" "$TOP/indy/jam.sh" '<src!system!glue!mips>start_dyn.o' \
	'<src!system!glue!mips>init_term_dyn.o' \
	'<src!system!glue!arch!mips!mips>crti.o' \
	'<src!system!glue!arch!mips!mips>crtn.o' '<build>package'
glue="$OUTPUT/objects/haiku/mips/release/system/glue"
cp "$glue/start_dyn.o" "$glue/init_term_dyn.o" "$glue/arch/mips/crti.o" \
	"$glue/arch/mips/crtn.o" "$lib/"
$TARGET-gcc -O2 -fno-builtin -w -c -I"$TOP/headers/private/system" \
	"$TOP/src/system/libroot/stubbed/libroot_stubs.c" \
	-o "$STAGE/libroot_stubs.o"
$TARGET-ld -shared -soname=libroot.so \
	--version-script "$TOP/src/system/libroot/libroot_versions" \
	-o "$lib/libroot.so" "$STAGE/libroot_stubs.o"

# 2. Rebuild the target libraries with shared libraries enabled.
rm -rf "$ROOT/build/gcc-shared" "$STAGE/install" "$STAGE/gcc_syslibs" \
	"$STAGE/gcc_syslibs_devel"
mkdir -p "$ROOT/build/gcc-shared"
cd "$ROOT/build/gcc-shared"
CFLAGS="-O2" CXXFLAGS="-O2" "$ROOT/src/gcc/configure" \
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

fi

# 3. Lay out the two packages as build/jam/BuildFeatures expects them and
#    create them with the host "package" tool. The .PackageInfo files follow
#    the gcc_syslibs recipe of HaikuPorts.
version="$("$PREFIX/bin/$TARGET-gcc" -dumpversion)"
installed="$STAGE/install/boot/system"
targetLib="$installed/$TARGET/lib"
gccLib="$installed/lib/gcc/$TARGET/$version"
kernelLibs="$ROOT/build/kernel-libs"
base="$STAGE/gcc_syslibs"
devel="$STAGE/gcc_syslibs_devel"
rm -rf "$base" "$devel" "$STAGE"/*.hpkg
mkdir -p "$base/lib" "$devel/develop/lib" "$devel/develop/headers/gcc"

# libsupc++.so is not a product of the GCC build. As in the HaikuPorts
# recipe, it is linked from the objects of the static library.
$TARGET-gcc -shared -nodefaultlibs -Wl,-soname,libsupc++.so \
	-o "$base/lib/libsupc++.so" -Wl,--whole-archive \
	"$ROOT/build/gcc-shared/$TARGET/libstdc++-v3/libsupc++/.libs/libsupc++convenience.a" \
	-Wl,--no-whole-archive -L"$targetLib" -lgcc_s -lroot

cp -a "$targetLib"/libgcc_s.so.1 "$targetLib"/libstdc++.so* "$base/lib/"
rm -f "$base"/lib/*-gdb.py
ln -s libgcc_s.so.1 "$base/lib/libgcc_s.so"
$TARGET-strip --strip-debug "$base/lib/libgcc_s.so.1" \
	"$base/lib/libsupc++.so" "$base"/lib/libstdc++.so.6.*

cp "$targetLib/libstdc++.a" "$targetLib/libsupc++.a" "$gccLib/libgcc.a" \
	"$gccLib/libgcc_eh.a" "$devel/develop/lib/"
for variant in kernel boot; do
	for library in libgcc libgcc_eh libsupc++; do
		cp "$kernelLibs/$library-kernel.a" \
			"$devel/develop/lib/$library-$variant.a"
	done
done
cp -r "$installed/$TARGET/include/c++/$version" "$devel/develop/headers/c++"
# The compiler's own headers (stddef.h, float.h) are installed with the
# compiler, not with libgcc: they come from the cross tools.
crossGccLib="$PREFIX/lib/gcc/$TARGET/$version"
cp -r "$crossGccLib/include" "$devel/develop/headers/gcc/include"
if [ -d "$crossGccLib/include-fixed" ]; then
	cp -r "$crossGccLib/include-fixed" \
		"$devel/develop/headers/gcc/include-fixed"
fi

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

tools="$OUTPUT/objects/linux/x86_64/release/tools"
packageTool="$tools/package/package"
export LD_LIBRARY_PATH="$(dirname "$(find "$OUTPUT/objects/linux" -name libroot_build.so | head -1)")${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
for name in gcc_syslibs gcc_syslibs_devel; do
	(cd "$STAGE/$name" && "$packageTool" create -q \
		"$STAGE/$name-${version}_indy-1-mips.hpkg")
done

# The same content under the names of the bootstrap repository
# (build/jam/repositories/HaikuPortsCross/mips). A package that exists in
# the build directory of that repository is not built by haikuporter.
for name in gcc_syslibs gcc_syslibs_devel; do
	bootstrapName="gcc_bootstrap_${name#gcc_}"
	sed -i -e "s/^name\t\t\t.*/name\t\t\t$bootstrapName/" \
		-e "s/^\t$name = \(.*\)/\t$bootstrapName = \1\n\t$name = \1/" \
		"$STAGE/$name/.PackageInfo"
	(cd "$STAGE/$name" && "$packageTool" create -q \
		"$STAGE/$bootstrapName-${version}_indy-1-mips.hpkg")
done
if [ -n "${BOOTSTRAP_OUTPUT:-}" ]; then
	repository="$BOOTSTRAP_OUTPUT/objects/haiku/mips/packaging/repositories/HaikuPortsCross-build/packages"
	mkdir -p "$repository"
	cp "$STAGE"/gcc_bootstrap_*.hpkg "$repository/"
fi

echo "syslibs.sh: packages in $STAGE"
ls -l "$STAGE"/*.hpkg
