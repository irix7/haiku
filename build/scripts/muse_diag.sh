#!/bin/sh
# Reports the state of the guest on the serial console through muse_log, and
# in /boot/home/muse-diag.txt. Started by the launch job muse_diag.
log=/boot/home/muse_log
out=/boot/home/muse-diag.txt
$log "muse_diag.sh started"
echo "started" > $out
sleep 20
step() {
	name=$1
	shift
	$log "== $name"
	"$@" 2>&1 | tee -a $out | $log
}
step date date
step uname uname -a
step bin ls -ld /bin /bin/python3 /bin/sh /boot/system/bin/python3 /boot/system/bin/bash
step python3 /boot/system/bin/python3 --version
step gcc /boot/system/bin/gcc --version
step ifconfig ifconfig
step route route
step ps ps
step launch ls /boot/system/data/launch
step interfaces cat /boot/system/settings/network/interfaces
$log "muse_diag.sh done"
