#!/usr/bin/env bash
# The console's cube command refuses a cube the game data does not have.
#
# SCENE.HQR's last offset is the end of the file, not a scene. `cube 222` on the
# full game used to read a header past the end and die with SIGBUS in
# HQF_LoadClose; a negative or huge number, or text, went through unchecked.
# The command now reads the scene count from the data, and the test takes the
# last cube from the command's own reply rather than hard-coding 221.
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
# Tier: small
TESTNAME=console_cube_range
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

run() { # run <cube-arg>: the command, then where the hero is 20 ticks later
    rm -rf "$tmp/u"
    mkdir -p "$tmp/u"
    ctl --user-dir "$tmp/u" --no-audio --fixed-dt 16 --exec "cube $1" \
        --exec-at 20 "status" --tick 30 --exit 2>&1
}

cube_now() { # the cube `status` reports
    printf '%s\n' "$1" | grep -aoE 'Cube: [0-9]+' | head -1 | sed 's/Cube: //'
}

# --- a number past the end is refused, and names the range -------------------
out=$(run 9999) || fail "cube 9999 exited non-zero"
last=$(printf '%s\n' "$out" | grep -aoE 'the scenes are 0 to [0-9]+' | sed 's/.* //')
[ -n "$last" ] || fail "cube 9999 was not refused with the range: $(printf '%s\n' "$out" | grep -a 'control\]' | head -3)"
[ "$(cube_now "$out")" = "0" ] || fail "cube 9999 moved the hero to cube $(cube_now "$out")"

# --- one past the last cube is refused, not loaded (the SIGBUS) ---------------
past=$((last + 1))
out=$(run "$past") || fail "cube $past exited non-zero (it crashed before the range check)"
printf '%s\n' "$out" | grep -aq "no cube $past" || fail "cube $past was not refused"
[ "$(cube_now "$out")" = "0" ] || fail "cube $past moved the hero to cube $(cube_now "$out")"

# --- negative and non-numbers are refused --------------------------------------
out=$(run -1) || fail "cube -1 exited non-zero"
printf '%s\n' "$out" | grep -aq "no cube -1" || fail "cube -1 was not refused"
out=$(run 12x) || fail "cube 12x exited non-zero"
printf '%s\n' "$out" | grep -aq "'12x' is not a number" || fail "cube 12x was not refused"

# --- the last cube still loads --------------------------------------------------
out=$(run "$last") || fail "cube $last exited non-zero"
[ "$(cube_now "$out")" = "$last" ] || fail "cube $last left the hero on cube $(cube_now "$out")"

pass "cube refuses numbers outside 0 to $last and still loads cube $last"
