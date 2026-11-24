#!/usr/bin/env bash
# Builds bootstrap packages with haikuporter, one port after the other, the
# way the jam rule BootstrapRepositoryFetchPackage does (task M5.5 of
# indy-plan.md). jam builds the packages of the repository file in a fixed
# order; this script builds the named ports only.
#
# Usage: indy/toolchain/port.sh PORT...
#   for example: indy/toolchain/port.sh zlib_bootstrap
# Run inside the devshell (nix develop).
#
# Needs a build directory configured with --bootstrap, in which
#   indy/jam.sh @bootstrap-raw build haiku_cross_devel_sysroot_stage1_mips.hpkg
# has built the cross sysroot package, and whose HaikuPortsCross repository
# holds haikuports.conf (jam writes it with the first package it builds).
#
# Environment:
#   BOOTSTRAP_OUTPUT  the build directory
#                     (default /mnt/europa/haiku-toolchain/bootstrap)
#   PORTS             the directory with the checkouts of
#                     github.com/irix7/museporter and irix7/museports.cross,
#                     branch indy-mips
#                     (default /mnt/europa/haiku-toolchain/ports)
#   PORTER_ARGS       extra haikuporter options, for example "--debug"
set -euo pipefail

if [ $# -eq 0 ]; then
	echo "usage: port.sh PORT..." >&2
	exit 2
fi

ROOT="${HAIKU_TOOLCHAIN:-/mnt/europa/haiku-toolchain}"
OUTPUT="${BOOTSTRAP_OUTPUT:-$ROOT/bootstrap}"
PORTS="${PORTS:-$ROOT/ports}"
JOBS="${JOBS:-$(nproc)}"
REPOSITORY="$OUTPUT/objects/haiku/mips/packaging/repositories/HaikuPortsCross-build"
SYSROOT_PACKAGE="$OUTPUT/objects/haiku/mips/packaging/packages/haiku_cross_devel_sysroot_stage1_mips.hpkg"

for path in "$REPOSITORY/haikuports.conf" "$SYSROOT_PACKAGE" \
		"$PORTS/haikuporter/haikuporter"; do
	if [ ! -e "$path" ]; then
		echo "port.sh: missing $path" >&2
		exit 2
	fi
done

# The host tools of the Haiku build (package, mimeset) need its libraries.
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:+$LD_LIBRARY_PATH:}$OUTPUT/objects/linux/lib"
# Keeps the bash of a NixOS host from replacing PATH in haikuporter's build
# shells.
export NOSYSBASHRC=1

cd "$REPOSITORY"
for port in "$@"; do
	"$PORTS/haikuporter/haikuporter" -j"$JOBS" --all-dependencies \
		${PORTER_ARGS:-} --cross-devel-package "$SYSROOT_PACKAGE" "$port"
done

ls -l "$REPOSITORY/packages"
