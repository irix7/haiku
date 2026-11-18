#!/usr/bin/env bash
# Serial gate. Boots an ECOFF image on an emulated SGI Indy through the PROM
# command "boot -f bootp()haiku" and waits for a marker on the serial console.
#
# Usage: indy/gate.sh IMAGE MARKER [TIMEOUT_SECONDS]
#
# The gate passes when MARKER appears within the timeout. Logs stay in WORK:
# serial.log is the console, iris.out is the emulator output with the TFTP
# trace.
#
# Environment:
#   IRIS, IRIS_CI   emulator binaries (default /mnt/europa/iris/target/release)
#   WORK            work directory (default generated.mips/gate). Keep the path
#                   short: a Unix socket path is limited to about 107 bytes.
#   MONITOR_PORT    iris monitor port (default 18931). Concurrent runs need
#                   different ports and work directories.
#   RAM             megabytes of RAM in bank 0: 8, 16, 32, 64 or 128
#                   (default 64)
#   DISK            raw disk image to attach as SCSI ID 1
#   BOOT_ARGS       words added to the PROM boot command, for example "disk"
#                   to boot from the SCSI disk and not from a remote disk
#   IRIS_ARGS       extra emulator options, for example "--cpu r5000"
set -euo pipefail

IMAGE="${1:?usage: gate.sh IMAGE MARKER [TIMEOUT_SECONDS]}"
MARKER="${2:?usage: gate.sh IMAGE MARKER [TIMEOUT_SECONDS]}"
TIMEOUT="${3:-120}"

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IRIS="${IRIS:-/mnt/europa/iris/target/release/iris}"
IRIS_CI="${IRIS_CI:-/mnt/europa/iris/target/release/iris-ci}"
WORK="${WORK:-$TOP/generated.mips/gate}"
MONITOR_PORT="${MONITOR_PORT:-18931}"
RAM="${RAM:-64}"
DISK="${DISK:-}"

for path in "$IMAGE" "$IRIS" "$IRIS_CI" ${DISK:+"$DISK"}; do
	if [ ! -e "$path" ]; then
		echo "gate: missing $path" >&2
		exit 2
	fi
done

IMAGE="$(realpath "$IMAGE")"
rm -rf "$WORK"
mkdir -p "$WORK/tftp"
cp "$IMAGE" "$WORK/tftp/haiku"

{
	cat <<TOML
banks = [$RAM, 0, 0, 0]
no_audio = true
headless = true
nvram = "$WORK/nvram.bin"

[machine]
profile = "indy_ip24"

[network]
mode = "nat"
tftp_dir = "$WORK/tftp"

[scsi]
TOML
	if [ -n "$DISK" ]; then
		printf '\n[scsi.1]\npath = "%s"\n' "$(realpath "$DISK")"
	fi
} >"$WORK/machine.toml"

# iris writes a lock file to its working directory.
(cd "$WORK" && IRIS_NO_IDLE=1 IRIS_TFTP_TRACE=1 exec "$IRIS" --config "$WORK/machine.toml" --ci \
	--ci-socket "$WORK/i.sock" --monitor-port "$MONITOR_PORT" \
	--serial-log "$WORK/serial.log" --headless --noaudio ${IRIS_ARGS:-} \
	>"$WORK/iris.out" 2>&1) &
IRIS_PID=$!

cleanup() {
	local status=$?
	trap - EXIT
	kill "$IRIS_PID" 2>/dev/null || true
	wait "$IRIS_PID" 2>/dev/null || true
	if [ "$status" -ne 0 ]; then
		echo "gate: FAIL  marker \"$MARKER\" not seen; logs in $WORK" >&2
		echo "--- iris output (tail)" >&2
		tail -20 "$WORK/iris.out" >&2 || true
		echo "--- serial log (tail)" >&2
		tail -40 "$WORK/serial.log" >&2 || true
	fi
	exit "$status"
}
trap cleanup EXIT

ic() { "$IRIS_CI" --socket "$WORK/i.sock" -q "$@"; }

ic status --wait-ready 30
ic start
# Headless, the PROM puts its maintenance menu on the serial console.
ic serial-wait 'Option?' --timeout 60
ic serial-send '5'
ic serial-wait '>>' --timeout 30
ic serial-send "boot -f bootp()haiku${BOOT_ARGS:+ $BOOT_ARGS}"
ic serial-wait 'entry: 0x' --timeout 60
ic serial-wait "$MARKER" --timeout "$TIMEOUT"

echo "gate: PASS  marker \"$MARKER\" seen; logs in $WORK"
