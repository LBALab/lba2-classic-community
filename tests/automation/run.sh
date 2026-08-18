#!/usr/bin/env bash
# Run the CLI control-harness test suite. Local-only: needs the built binary, retail
# game data (LBA2_GAME_DIR), and a display. Tests skip cleanly when prerequisites are
# absent, so an empty environment reports SKIP, not FAIL.
#
#   LBA2_GAME_DIR=/path/to/data tests/automation/run.sh
#
# Exit 0 if nothing failed (passes and skips ok), 1 if any test failed.
#
# Writes last-run.md, because no workflow can run this suite and an unrecorded result is
# indistinguishable from never having run it. Commit it, or paste it into a PR that touches
# engine behaviour: it is the only evidence available that these still pass. The commit it was
# run against is recorded so a reader can tell how stale the result is.
#
#   --no-record   run without touching last-run.md
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
RECORD="$DIR/last-run.md"
record=true
[ "${1:-}" = "--no-record" ] && record=false

pass=0 skip=0 failn=0 failed=""
results=""
for t in "$DIR"/test_*.sh; do
    name="$(basename "$t")"
    bash "$t"
    rc=$?
    case "$rc" in
        0) pass=$((pass + 1)); results="$results| $name | pass |"$'\n' ;;
        77) skip=$((skip + 1)); results="$results| $name | skip |"$'\n' ;;
        *) failn=$((failn + 1)); failed="$failed $name"
           results="$results| $name | **FAIL** |"$'\n' ;;
    esac
done

echo "----------------------------------------"
echo "control-harness tests: $pass passed, $skip skipped, $failn failed"
[ -n "$failed" ] && echo "failed:$failed"

if [ "$record" = true ]; then
    sha="$(git -C "$DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    dirty=""
    git -C "$DIR" diff --quiet 2>/dev/null || dirty=" (working tree dirty)"
    {
        echo "<!-- Written by tests/automation/run.sh. -->"
        echo
        echo "# Control-harness last run"
        echo
        echo "- when: $(date -u '+%Y-%m-%d %H:%M UTC')"
        echo "- commit: \`$sha\`$dirty"
        echo "- host: $(uname -s -m)"
        echo "- result: **$pass passed, $skip skipped, $failn failed**"
        echo
        if [ "$skip" -gt 0 ]; then
            echo "A skip means a prerequisite was missing, not that the fixture agreed with the"
            echo "engine. Only the passes are evidence."
            echo
        fi
        echo "| Fixture | Result |"
        echo "|---|---|"
        printf '%s' "$results"
    } > "$RECORD"
    echo "recorded: ${RECORD#"$DIR"/} ($sha)"
fi

[ "$failn" -eq 0 ]
