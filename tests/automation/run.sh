#!/usr/bin/env bash
# Run the CLI control-harness test suite. Local-only: needs the built binary, retail
# game data (LBA2_GAME_DIR), and a display. Tests skip cleanly when prerequisites are
# absent, so an empty environment reports SKIP, not FAIL.
#
#   LBA2_GAME_DIR=/path/to/data tests/automation/run.sh
#
# Exit 0 if nothing failed (passes and skips ok), 1 if any test failed.
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"

pass=0 skip=0 failn=0 failed=""
for t in "$DIR"/test_*.sh; do
    name="$(basename "$t")"
    bash "$t"
    rc=$?
    case "$rc" in
        0) pass=$((pass + 1)) ;;
        77) skip=$((skip + 1)) ;;
        *) failn=$((failn + 1)); failed="$failed $name" ;;
    esac
done

echo "----------------------------------------"
echo "control-harness tests: $pass passed, $skip skipped, $failn failed"
[ -n "$failed" ] && echo "failed:$failed"
[ "$failn" -eq 0 ]
