#!/usr/bin/env bash
# Stands in for the engine under `run.sh --profile`: runs LBA2_PROFILE_BIN with the same
# arguments and exit code, and appends one line per run to LBA2_PROFILE_LOG:
#
#   milliseconds <tab> exit code <tab> arguments
#
# A run killed from outside (a fixture's `timeout -s KILL`) takes this script with it and
# leaves no line, so a fixture's wall time can exceed the engine time it shows.
set -u
now_ms() {
    if [ -n "${EPOCHREALTIME:-}" ]; then
        local us="${EPOCHREALTIME/[.,]/}"
        echo $((10#$us / 1000))
    else
        echo $(($(date +%s) * 1000))
    fi
}
t0="$(now_ms)"
"$LBA2_PROFILE_BIN" "$@"
rc=$?
if [ -n "${LBA2_PROFILE_LOG:-}" ]; then
    printf '%s\t%s\t%s\n' "$(($(now_ms) - t0))" "$rc" "$*" >>"$LBA2_PROFILE_LOG"
fi
exit "$rc"
