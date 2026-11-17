#!/usr/bin/env bash
# Runs jam for the mips build inside the devshell.
#
# Usage: indy/jam.sh TARGET...
#   for example: indy/jam.sh haiku-netboot-indy
#
# The output goes to generated.mips/jam.log. The clone has no hrev tags, so
# the revision is set here.
set -uo pipefail

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="${JOBS:-$(nproc)}"
REVISION="${HAIKU_REVISION:-hrev59000}"

quoted=""
for target in "$@"; do
	quoted="$quoted '$target'"
done

cd "$TOP" && nix develop -c bash -c "cd '$TOP/generated.mips' \
	&& jam -q -j$JOBS -sHAIKU_REVISION=$REVISION $quoted > jam.log 2>&1"
status=$?

grep -n "rror:\|undefined reference\|don't know how to make\|\.\.\.failed" \
	"$TOP/generated.mips/jam.log" | head -"${ERRORS:-20}" | cut -c1-240
tail -2 "$TOP/generated.mips/jam.log" | cut -c1-200
echo "jam.sh: exit $status"
exit $status
