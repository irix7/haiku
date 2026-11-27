#!/usr/bin/env bash
# M5 gate. Builds the netboot kernel and the bootstrap image in the bootstrap
# build directory, then boots the kernel on iris with the image as the SCSI
# disk and waits for a marker on the serial console.
#
# Usage: indy/gate-m5.sh [MARKER] [TIMEOUT_SECONDS]
#
# Environment, besides that of gate.sh:
#   BOOTSTRAP   bootstrap build directory
#               (default /mnt/europa/haiku-toolchain/bootstrap)
#   IMAGE_SIZE  size of the image in MB (default 600)
#   IMAGE_JAM_ARGS  more jam arguments for the image, for example
#               -sHAIKU_LEAN_IMAGE=1
#   NO_BUILD    set to skip the two jam runs
set -euo pipefail

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MARKER="${1:-muse: m5 userland}"
TIMEOUT="${2:-180}"
BOOTSTRAP="${BOOTSTRAP:-/mnt/europa/haiku-toolchain/bootstrap}"
IMAGE_SIZE="${IMAGE_SIZE:-600}"

if [ -z "${NO_BUILD:-}" ]; then
	OUTPUT="$BOOTSTRAP" "$TOP/indy/jam.sh" @bootstrap-raw build \
		haiku-netboot-indy
	OUTPUT="$BOOTSTRAP" JAM_ARGS="-sHAIKU_IMAGE_SIZE=$IMAGE_SIZE ${IMAGE_JAM_ARGS:-}" \
		"$TOP/indy/jam.sh" @bootstrap-raw
fi

# The kernel writes to the boot volume, so the gate boots a copy.
mkdir -p "$TOP/generated.mips"
cp "$BOOTSTRAP/haiku-bootstrap.image" "$TOP/generated.mips/m5.image"

export RAM="${RAM:-128}"
export DISK="$TOP/generated.mips/m5.image"
export BOOT_ARGS="${BOOT_ARGS:-disk}"
"$TOP/indy/gate.sh" "$BOOTSTRAP/haiku-indy-netboot" "$MARKER" "$TIMEOUT"
