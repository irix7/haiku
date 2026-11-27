#!/usr/bin/env bash
# Graphics gate. Boots an ECOFF image on an emulated SGI Indy with the Newport
# display, waits for a marker on the serial console and saves a screenshot.
#
# Usage: indy/gate-gfx.sh IMAGE MARKER SCREENSHOT [TIMEOUT_SECONDS]
#
# With a display the PROM uses the graphics console and prints nothing on the
# serial port, so the PROM is driven with keystrokes through the iris monitor
# port. The image under test writes its own output to the serial port.
#
# Environment: as indy/gate.sh. The default WORK is generated.mips/gate-gfx and
# the default MONITOR_PORT is 18932.
#   BOOT_ARGS       arguments for the kernel, for example "disk"
#   CONFIG_END      lines for the end of the machine file, for example
#                   $'[graphics]\nresolution = "1024x768"'
#   CONFIG_TOP, THEN  as indy/gate.sh; THEN runs after the screenshot
#   SETTLE          seconds to wait after the marker before the screenshot
#                   (default 2)
set -euo pipefail

USAGE="usage: gate-gfx.sh IMAGE MARKER SCREENSHOT [TIMEOUT_SECONDS]"
IMAGE="${1:?$USAGE}"
MARKER="${2:?$USAGE}"
SCREENSHOT="${3:?$USAGE}"
TIMEOUT="${4:-180}"

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IRIS="${IRIS:-/mnt/europa/iris/target/release/iris}"
IRIS_CI="${IRIS_CI:-/mnt/europa/iris/target/release/iris-ci}"
WORK="${WORK:-$TOP/generated.mips/gate-gfx}"
MONITOR_PORT="${MONITOR_PORT:-18932}"
RAM="${RAM:-64}"
# A bank holds at most 128 MB. RAM above that fills the next banks. The kernel
# uses the RAM that is contiguous from 0x08000000, which is 256 MB: two banks.
BANKS=$(python3 -c "
r = $RAM
b = []
while r > 0 and len(b) < 4:
    b.append(min(r, 128)); r -= b[-1]
print(', '.join(str(x) for x in b + [0] * (4 - len(b))))")
DISK="${DISK:-}"
SETTLE="${SETTLE:-2}"

for path in "$IMAGE" "$IRIS" "$IRIS_CI" ${DISK:+"$DISK"}; do
	if [ ! -e "$path" ]; then
		echo "gate-gfx: missing $path" >&2
		exit 2
	fi
done

IMAGE="$(realpath "$IMAGE")"
rm -rf "$WORK"
mkdir -p "$WORK/tftp" "$(dirname "$SCREENSHOT")"
SCREENSHOT="$(realpath "$SCREENSHOT")"
cp "$IMAGE" "$WORK/tftp/haiku"

{
	cat <<TOML
banks = [$BANKS]
no_audio = true
nvram = "$WORK/nvram.bin"
${CONFIG_TOP:-}

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
	if [ -n "${CONFIG_END:-}" ]; then
		printf '\n%s\n' "$CONFIG_END"
	fi
} >"$WORK/machine.toml"

# iris writes a lock file to its working directory.
(cd "$WORK" && IRIS_NO_IDLE=1 IRIS_TFTP_TRACE=1 exec "$IRIS" --config "$WORK/machine.toml" --ci \
	--ci-socket "$WORK/i.sock" --monitor-port "$MONITOR_PORT" \
	--serial-log "$WORK/serial.log" --noaudio ${IRIS_ARGS:-} \
	>"$WORK/iris.out" 2>&1) &
IRIS_PID=$!

cleanup() {
	local status=$?
	trap - EXIT
	if [ "$status" -ne 0 ]; then
		"$IRIS_CI" --socket "$WORK/i.sock" -q screenshot "$WORK/fail.png" \
			2>/dev/null || true
	fi
	kill "$IRIS_PID" 2>/dev/null || true
	wait "$IRIS_PID" 2>/dev/null || true
	if [ "$status" -ne 0 ]; then
		echo "gate-gfx: FAIL  marker \"$MARKER\" not seen; logs in $WORK" >&2
		echo "--- iris output (tail)" >&2
		tail -20 "$WORK/iris.out" >&2 || true
		echo "--- serial log (tail)" >&2
		tail -40 "$WORK/serial.log" >&2 || true
	fi
	exit "$status"
}
trap cleanup EXIT

ic() { "$IRIS_CI" --socket "$WORK/i.sock" -q "$@"; }
mon() { printf '%s\n' "$*" | nc -w 4 127.0.0.1 "$MONITOR_PORT" >/dev/null; }

ic status --wait-ready 30
ic start

# Esc stops the PROM's automatic boot and opens the maintenance menu. Entry 5
# is the command monitor.
sleep 12
mon "ps2 esc"
sleep 2
mon "ps2 esc"
mon "screen wait 1500 40000"
mon "ps2 type 5"
mon "screen wait 1500 20000"
mon "ps2 type boot -f bootp()haiku${BOOT_ARGS:+ $BOOT_ARGS}"
mon "ps2 enter"

ic serial-wait "$MARKER" --timeout "$TIMEOUT"
sleep "$SETTLE"
ic screenshot "$SCREENSHOT"

if [ -n "${THEN:-}" ]; then
	WORK="$WORK" bash -c "$THEN"
fi

echo "gate-gfx: PASS  marker \"$MARKER\" seen; screenshot $SCREENSHOT;" \
	"logs in $WORK"
