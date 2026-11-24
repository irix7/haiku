#!/usr/bin/env bash
# Runs jam for the mips build, inside the devshell if there is one.
#
# Usage: indy/jam.sh TARGET...
#   for example: indy/jam.sh haiku-netboot-indy
#
# OUTPUT names another build directory than generated.mips, for example the
# bootstrap build. The output goes to jam.log in the build directory. The clone has no hrev tags, so
# the revision is set here.
set -uo pipefail

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="${JOBS:-$(nproc)}"
REVISION="${HAIKU_REVISION:-hrev59000}"
OUTPUT="${OUTPUT:-$TOP/generated.mips}"

quoted=""
for target in "$@"; do
	quoted="$quoted '$target'"
done

# Inside the devshell, or on a host without nix (CI), jam runs directly.
command="cd '$OUTPUT' \
	&& jam -q -j$JOBS -sHAIKU_REVISION=$REVISION ${JAM_ARGS:-} $quoted > jam.log 2>&1"
if [ -n "${IN_NIX_SHELL:-}" ] || ! command -v nix >/dev/null 2>&1; then
	bash -c "$command"
else
	(cd "$TOP" && nix develop -c bash -c "$command")
fi
status=$?

grep -n "rror:\|undefined reference\|don't know how to make\|\.\.\.failed" \
	"$OUTPUT/jam.log" | head -"${ERRORS:-20}" | cut -c1-240
tail -2 "$OUTPUT/jam.log" | cut -c1-200
echo "jam.sh: exit $status"
exit $status
