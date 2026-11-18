#!/usr/bin/env bash
# Builds the cross toolchain for Haiku on the SGI Indy: binutils 2.47 and
# GCC 17 from the owner's forks, target mips-unknown-haiku (big-endian, n32,
# MIPS III, tuned for the R4600).
#
# Usage: indy/toolchain/build.sh [binutils|gcc|kernel-libs|all]
#        (default: all)
# Run inside the devshell (nix develop).
#
# Layout under $HAIKU_TOOLCHAIN (default /mnt/europa/haiku-toolchain):
#   src/binutils, src/gcc   git worktrees of the forks, branch haiku-mips
#   build/                  object trees
#   cross-tools-mips/       install prefix
#
# A missing worktree is created from the fork and patched with
# indy/toolchain/patches/*.patch. Nothing is committed.
set -euo pipefail

STEP="${1:-all}"
TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT="${HAIKU_TOOLCHAIN:-/mnt/europa/haiku-toolchain}"
GCC_FORK="${GCC_FORK:-/home/matt/projects/sgi-toolchain-new/.scratch/rust-lang-gcc}"
BINUTILS_FORK="${BINUTILS_FORK:-/home/matt/projects/sgi-toolchain-new/.scratch/binutils-gdb-fork}"
TARGET=mips-unknown-haiku
PREFIX="$ROOT/cross-tools-mips"
SYSROOT="$PREFIX/sysroot"
JOBS="${JOBS:-$(nproc)}"

worktree() {
	# worktree NAME FORK PATCH
	local path="$ROOT/src/$1"
	if [ ! -d "$path" ]; then
		mkdir -p "$ROOT/src"
		git -C "$2" worktree add "$path" -b haiku-mips \
			|| git -C "$2" worktree add "$path" haiku-mips
		git -C "$path" apply "$3"
	fi
}

stage_headers() {
	# The Haiku headers GCC's target libraries are compiled against.
	local include="$SYSROOT/boot/system/develop/headers"
	mkdir -p "$include" "$SYSROOT/boot/system/develop/lib"
	for directory in config os posix; do
		rm -rf "$include/$directory"
		cp -r "$TOP/headers/$directory" "$include/$directory"
	done
}

build_binutils() {
	worktree binutils "$BINUTILS_FORK" \
		"$TOP/indy/toolchain/patches/binutils-haiku-mips.patch"
	rm -rf "$ROOT/build/binutils"
	mkdir -p "$ROOT/build/binutils" "$SYSROOT"
	cd "$ROOT/build/binutils"
	CFLAGS="-O2" CXXFLAGS="-O2" "$ROOT/src/binutils/configure" \
		--prefix="$PREFIX" --target=$TARGET \
		--disable-nls --disable-shared --disable-werror \
		--disable-gdb --disable-gdbserver --disable-sim --disable-gprofng \
		--with-sysroot="$SYSROOT" --disable-maintainer-mode \
		--enable-multilib
	make -j"$JOBS" MAKEINFO=true
	make install MAKEINFO=true
}

build_gcc() {
	worktree gcc "$GCC_FORK" "$TOP/indy/toolchain/patches/gcc-haiku-mips.patch"
	stage_headers
	rm -rf "$ROOT/build/gcc"
	mkdir -p "$ROOT/build/gcc"
	cd "$ROOT/build/gcc"
	export PATH="$PREFIX/bin:$PATH"
	CFLAGS="-O2" CXXFLAGS="-O2" "$ROOT/src/gcc/configure" \
		--prefix="$PREFIX" --target=$TARGET \
		--disable-nls --disable-shared --with-system-zlib \
		--enable-languages=c,c++ --enable-lto --enable-frame-pointer \
		--enable-__cxa-atexit --enable-threads=posix \
		--with-default-libstdcxx-abi=gcc4-compatible \
		--with-sysroot="$SYSROOT" --disable-maintainer-mode \
		--disable-libgomp --disable-libatomic --disable-libsanitizer \
		--disable-libquadmath --disable-libssp --disable-libvtv \
		--enable-multilib \
		--with-arch=mips3 --with-tune=r4600 --with-abi=n32 \
		--with-float=hard \
		--with-gmp="$GMP_PREFIX" --with-mpfr="$MPFR_PREFIX" \
		--with-mpc="$MPC_PREFIX" --with-isl="$ISL_PREFIX"
	make -j"$JOBS" all-gcc MAKEINFO=true
	make install-gcc MAKEINFO=true
	make -j"$JOBS" all-target-libgcc MAKEINFO=true
	make install-target-libgcc MAKEINFO=true
	make -j"$JOBS" all-target-libstdc++-v3 MAKEINFO=true
	make install-target-libstdc++-v3 MAKEINFO=true
}

build_kernel_libs() {
	# Kernel versions of libgcc and libsupc++, as the HaikuPorts gcc recipe
	# builds them: soft-float, freestanding, no threads, no TLS. The results
	# go to $ROOT/build/kernel-libs, where seed.sh takes them from. They are
	# position independent, because kernel add-ons link them too.
	local flags="-ffreestanding -O2 -D_KERNEL_MODE -msoft-float"
	local target="$ROOT/build/gcc/$TARGET/soft-float"
	local out="$ROOT/build/kernel-libs"
	export PATH="$PREFIX/bin:$PATH"
	rm -rf "$out"
	mkdir -p "$out"

	cd "$target/libgcc"
	rm -rf saved
	mkdir saved
	mv libgcc.a saved/
	[ -f libgcc_eh.a ] && mv libgcc_eh.a saved/
	make clean
	ln -sfn "$ROOT/src/gcc/libgcc/gthr-single.h" gthr-default.h
	make -j"$JOBS" CFLAGS="$flags" CXXFLAGS="$flags" MAKEINFO=true
	cp libgcc.a "$out/libgcc-kernel.a"
	if [ -f libgcc_eh.a ]; then
		cp libgcc_eh.a "$out/libgcc_eh-kernel.a"
	else
		"$PREFIX/bin/$TARGET-ar" rcs "$out/libgcc_eh-kernel.a"
	fi
	ln -sfn "$ROOT/src/gcc/libgcc/gthr-posix.h" gthr-default.h
	mv saved/* .
	rmdir saved

	cd "$target/libstdc++-v3/libsupc++"
	local bits="../include/$TARGET/bits"
	rm -rf saved
	mkdir saved
	mv .libs/libsupc++* saved/
	cp "$bits/gthr-default.h" ../config.h "$bits/c++config.h" saved/
	make clean
	cp "$bits/gthr-single.h" "$bits/gthr-default.h"
	sed -i -e 's,#define _GLIBCXX_HAS_GTHREADS 1,/* #undef _GLIBCXX_HAS_GTHREADS */,' \
		../config.h
	sed -i -e 's,#define _GLIBCXX_HAVE_TLS 1,/* #undef _GLIBCXX_HAVE_TLS */,' \
		"$bits/c++config.h"
	make -j"$JOBS" CFLAGS="$flags" CXXFLAGS="$flags" MAKEINFO=true
	cp .libs/libsupc++.a "$out/libsupc++-kernel.a"
	mv saved/libsupc++* .libs/
	mv saved/gthr-default.h "$bits/"
	mv saved/config.h ..
	mv saved/c++config.h "$bits/"
	rmdir saved
}

case "$STEP" in
	binutils)		build_binutils ;;
	gcc)			build_gcc ;;
	kernel-libs)	build_kernel_libs ;;
	all)			build_binutils; build_gcc; build_kernel_libs ;;
	*)
		echo "usage: build.sh [binutils|gcc|kernel-libs|all]" >&2
		exit 2
		;;
esac

echo "build.sh: done; tools in $PREFIX/bin"
