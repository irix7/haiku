#!/usr/bin/env bash
# GDB stub gate. Boots the netboot image on iris from a RemoteDisk server,
# waits until the boot volume is mounted, then runs indy/gdb-check.py against
# the kernel's GDB stub.
#
# Usage: indy/gate-gdb.sh serial|udp [IMAGE]
#
#   serial  through the serial console (a Unix socket of iris)
#   udp     through UDP port 2159 of the kernel (an iris port forward)
#
# Environment, besides that of gate.sh:
#   DISK_IMAGE  BFS image the RemoteDisk server offers
#               (default generated.mips/disk.raw)
#   GDB_PORT    host UDP port forwarded to the kernel (default 21590)
#   BREAK_AT    kernel function for the breakpoint (default
#               timer_interrupt)
set -euo pipefail

MODE="${1:?usage: gate-gdb.sh serial|udp [IMAGE]}"
TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$TOP/generated.mips"
IMAGE="${2:-$OUT/haiku-indy-netboot}"
DISK_IMAGE="${DISK_IMAGE:-$OUT/disk.raw}"
GDB_PORT="${GDB_PORT:-21590}"
BREAK_AT="${BREAK_AT:-timer_interrupt}"
export WORK="${WORK:-$OUT/gate-gdb}"

KERNEL="$OUT/objects/haiku/mips/release/system/kernel/kernel_mips"
TARGET_NM="${TARGET_NM:-/mnt/europa/haiku-toolchain/cross-tools-mips/bin/mips-unknown-haiku-nm}"
SERVER="$OUT/objects/linux/x86_64/release/tools/remote_disk_server/remote_disk_server"

for path in "$KERNEL" "$TARGET_NM" "$SERVER" "$DISK_IMAGE"; do
	if [ ! -e "$path" ]; then
		echo "gate-gdb: missing $path" >&2
		exit 2
	fi
done

ADDRESS="$("$TARGET_NM" -C "$KERNEL" | awk -v name="$BREAK_AT" \
	'($2 == "T" || $2 == "t") && ($3 == name || index($3, name "(") == 1) \
		{ print $1; exit }')"
if [ -z "$ADDRESS" ]; then
	echo "gate-gdb: no function $BREAK_AT in $KERNEL" >&2
	exit 2
fi

case "$MODE" in
	serial)
		export CONFIG_TOP="serial_socket_b = \"$WORK/serial.sock\""
		TRANSPORT="unix:$WORK/serial.sock"
		;;
	udp)
		export CONFIG_END="[[port_forward]]
proto = \"udp\"
host_port = $GDB_PORT
guest_port = 2159
bind = \"localhost\""
		TRANSPORT="udp:127.0.0.1:$GDB_PORT"
		;;
	*)
		echo "gate-gdb: unknown mode $MODE" >&2
		exit 2
		;;
esac

"$SERVER" "$DISK_IMAGE" >"$OUT/rds.log" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true' EXIT

export BOOT_ARGS="server=192.168.0.1"
export THEN="python3 '$TOP/indy/gdb-check.py' '$TRANSPORT' '$ADDRESS'"
"$TOP/indy/gate.sh" "$IMAGE" "Mounted boot partition" "${TIMEOUT:-120}"
