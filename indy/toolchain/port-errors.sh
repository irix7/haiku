#!/usr/bin/env bash
# Prints what matters of a haikuporter log (the output of port.sh): the port
# that was being built when the log ends, the first error lines with the
# lines before them, and the end of the log.
#
# Usage: indy/toolchain/port-errors.sh LOG [MAX_ERRORS]
set -uo pipefail

LOG="${1:?usage: port-errors.sh LOG [MAX_ERRORS]}"
MAX="${2:-12}"

if [ ! -f "$LOG" ]; then
	echo "port-errors.sh: no log $LOG" >&2
	exit 2
fi

# haikuporter's scan of the recipe tree is noise here.
filtered="$(mktemp)"
trap 'rm -f "$filtered"' EXIT
grep -v 'cannot be built for architecture\|updating dependency infos\|is still broken\|^	removing \|^--20[0-9][0-9]-\|^Resolving \|^Connecting to \|^HTTP request sent\|^Length: \|^Saving to: \|^ *[0-9]*K \.' \
	"$LOG" >"$filtered"

# The ports are announced as "category::name-version" between rules.
echo "== ports in this log (the last one is the one that failed)"
grep -A1 '^=\{60,\}$' "$filtered" | grep '::' | awk '!seen[$0]++' | sed 's/^/   /'

echo "== first errors"
grep -n -m "$MAX" -B3 \
	'error:\|Error: \| Error [0-9]\|\*\*\* \|undefined reference\|No such file or directory\|not found\|cannot find\|FAILED\|Traceback\|sysExit\|^Error' \
	"$filtered" | cut -c1-300

echo "== end of the log"
tail -25 "$filtered" | cut -c1-300
