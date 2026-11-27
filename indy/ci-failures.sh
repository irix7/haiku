#!/usr/bin/env bash
# Shows why jobs of the bootstrap workflow failed (the workflow of
# github.com/irix7/museports.cross, see indy-plan.md section 1.5).
#
# Usage: indy/ci-failures.sh [RUN_ID]
#   without RUN_ID: the latest run of the workflow
#
# For every failed job it prints the job name, the failed step, and the file
# errors.txt of the job's artifact, which the workflow writes with
# indy/toolchain/port-errors.sh. The whole logs stay in DIR/ARTIFACT.
#
# Environment:
#   REPO  repository of the workflow (default irix7/museports.cross)
#   DIR   where the artifacts go (default generated.mips/ci-RUN_ID)
set -euo pipefail

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="${REPO:-irix7/museports.cross}"
RUN="${1:-$(gh run list -R "$REPO" --workflow bootstrap.yaml --limit 1 \
	--json databaseId -q '.[0].databaseId')}"
DIR="${DIR:-$TOP/generated.mips/ci-$RUN}"

gh run view "$RUN" -R "$REPO" --json status,conclusion,headSha,url \
	-q '"run \(.url)\nstatus \(.status) \(.conclusion // "") at \(.headSha[0:7])"'
echo "== jobs"
gh run view "$RUN" -R "$REPO" --json jobs \
	-q '.jobs[] | "\(.conclusion // .status)\t\(.name)\t\([.steps[] | select(.conclusion == "failure") | .name] | join(", "))"'

rm -rf "$DIR"
mkdir -p "$DIR"
gh run download "$RUN" -R "$REPO" --dir "$DIR" >/dev/null 2>&1 || true

found=0
for errors in "$DIR"/*/errors.txt; do
	[ -f "$errors" ] || continue
	found=1
	echo
	echo "######## $(basename "$(dirname "$errors")")"
	cat "$errors"
done
if [ "$found" = 0 ]; then
	echo "no errors.txt in the artifacts of this run; logs, if any, are in $DIR"
fi
