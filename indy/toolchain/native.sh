#!/usr/bin/env bash
# Builds the native toolchain for Muse: binutils and GCC that run on
# mips-unknown-haiku and produce code for it (a Canadian cross: build
# x86_64 Linux, host and target mips-unknown-haiku).
#
# Usage: indy/toolchain/native.sh [sysroot|binutils|gcc|all]   (default: all)
# Run inside the devshell (nix develop). Needs the cross toolchain of
# build.sh in $PREFIX and the sysroot package of the bootstrap build.
#
# Results: $ROOT/native/stage, a tree below /boot/system for the image
# (bin, lib, libexec, share). Object trees are in $ROOT/build/native-*.
set -euo pipefail

STEP="${1:-all}"
TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT="${HAIKU_TOOLCHAIN:-/mnt/europa/haiku-toolchain}"
BOOTSTRAP="${BOOTSTRAP_OUTPUT:-$ROOT/bootstrap}"
TARGET=mips-unknown-haiku
BUILD_MACHINE="$(uname -m)-pc-linux-gnu"
PREFIX="$ROOT/cross-tools-mips"
JOBS="${JOBS:-$(nproc)}"
NATIVE="$ROOT/native"
STAGE="$NATIVE/stage"
HOSTSYSROOT="$NATIVE/sysroot"
PACKAGE_TOOL="$BOOTSTRAP/objects/linux/x86_64/release/tools/package/package"
export PATH="$PREFIX/bin:$PATH"
# The programs are built for the target by the cross compiler with the
# sysroot of this script. The devshell sets CC and CXX for the build machine.
export CC="$TARGET-gcc --sysroot=$HOSTSYSROOT"
export CXX="$TARGET-g++ --sysroot=$HOSTSYSROOT"
export CC_FOR_BUILD=gcc
export CXX_FOR_BUILD=g++
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:+$LD_LIBRARY_PATH:}$BOOTSTRAP/objects/linux/lib"

sysroot() {
	# The libraries and headers the programs link with: the stage 1 sysroot
	# package, and the C/C++ runtime (gcc_syslibs and its devel package, built
	# by syslibs.sh) below /boot/system.
	local packages="$BOOTSTRAP/objects/haiku/mips/packaging"
	rm -rf "$HOSTSYSROOT"
	mkdir -p "$HOSTSYSROOT/boot/system"
	for package in "$packages/packages/haiku_cross_devel_sysroot_stage1_mips.hpkg" \
			"$packages"/repositories/HaikuPortsCross-build/packages/gcc_syslibs-*.hpkg \
			"$packages"/repositories/HaikuPortsCross-build/packages/gcc_syslibs_devel-*.hpkg; do
		"$PACKAGE_TOOL" extract -C "$HOSTSYSROOT/boot/system" "$package"
	done
	# libroot holds the math, pthread and dl functions; see syslibs.sh.
	local lib="$HOSTSYSROOT/boot/system/develop/lib"
	for name in libm libpthread libdl; do
		printf 'INPUT(libroot.so)\n' >"$lib/$name.so"
	done
	printf 'INPUT(-lgcc)\n' >"$lib/libgcc_s.so"
	ls "$lib"
}

build_binutils() {
	rm -rf "$ROOT/build/native-binutils"
	mkdir -p "$ROOT/build/native-binutils"
	cd "$ROOT/build/native-binutils"
	CFLAGS="-O2" CXXFLAGS="-O2" "$ROOT/src/binutils/configure" \
		--build="$BUILD_MACHINE" --host=$TARGET --target=$TARGET \
		--prefix=/boot/system --disable-nls --disable-shared \
		--disable-werror --disable-gdb --disable-gdbserver --disable-sim \
		--disable-gprofng --disable-maintainer-mode --disable-plugins \
		--with-sysroot=/boot/system --enable-multilib
	make -j"$JOBS" MAKEINFO=true
	make install DESTDIR="$STAGE" MAKEINFO=true
}

build_gcc() {
	cd "$ROOT/src/gcc"
	for lib in gmp mpfr mpc isl; do
		[ -e "$lib" ] || { ./contrib/download_prerequisites --no-verify; break; }
	done
	rm -rf "$ROOT/build/native-gcc"
	mkdir -p "$ROOT/build/native-gcc"
	cd "$ROOT/build/native-gcc"
	CFLAGS="-O2" CXXFLAGS="-O2" "$ROOT/src/gcc/configure" \
		--build="$BUILD_MACHINE" --host=$TARGET --target=$TARGET \
		--prefix=/boot/system --disable-nls --disable-shared \
		--enable-languages=c,c++ --enable-lto --enable-frame-pointer \
		--enable-__cxa-atexit --enable-threads=posix \
		--with-default-libstdcxx-abi=gcc4-compatible \
		--with-sysroot=/ --with-build-sysroot="$HOSTSYSROOT" \
		--with-native-system-header-dir=/boot/system/develop/headers \
		--disable-libgomp --disable-libatomic --disable-libsanitizer \
		--disable-libquadmath --disable-libssp --disable-libvtv \
		--disable-libstdcxx-pch --disable-bootstrap \
		--enable-multilib --with-arch=mips3 --with-tune=r4600 \
		--with-abi=n32 --with-float=hard
	make -j"$JOBS" all-gcc MAKEINFO=true
	make install-gcc DESTDIR="$STAGE" MAKEINFO=true
}

case "$STEP" in
	sysroot) sysroot ;;
	binutils) build_binutils ;;
	gcc) build_gcc ;;
	all) sysroot; build_binutils; build_gcc ;;
	*) echo "usage: native.sh [sysroot|binutils|gcc|all]" >&2; exit 2 ;;
esac
