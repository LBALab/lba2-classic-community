#!/usr/bin/env bash
# Phase 1 harness regression (docs/INPUT_SIM_PLAN.md): a sustained `input` hold is metered in SIM
# ticks and OR'd into Input every rendered frame, so it is consumed on every sim step and drives the
# hero the same over equal game-time whatever the fixed-timestep throttle does. This is the harness
# gobble the old render-frame count produced: `input up 30` walked full distance at --fixed-timestep
# 16 and ZERO at 100 (the hold expired before a sparse sim step consumed it).
#
# Test: hold forward for ~30s of game-time (--fixed-dt 16 pins game-time to --tick, so --tick 1875 =
# 30s at any throttle) with the throttle mild (16) and coarse (100). The distance floated must match
# within tolerance; a dropped hold would collapse the coarse-throttle distance.
#
# Fixture: a high-altitude glide save (open air). Physics-sensitive walled scenes are deliberately
# avoided -- the coarse 10Hz sim can tunnel collision at --fixed-timestep 100 and diverge for reasons
# unrelated to input delivery (that is issue #412 territory, not this test's concern).
#
# Local-only: needs retail data + a build + the tracked steam_classic_2023 corpus; skips otherwise.
TESTNAME=input_hold_throttle
. "$(dirname "$0")/lib.sh"
precheck

SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/V Jump.LBA"
[ -f "$SAVE" ] || skip "no V Jump corpus save"

# Distance the hero floats holding forward for 30s of game-time at a given throttle.
dist_after_hold() { # fixedtimestep-ms
    local fts="$1" sv d0 d1
    sv="$(mktemp --suffix=.LBA)"; d0="$(mktemp)"; d1="$(mktemp)"; cp "$SAVE" "$sv"
    ctl_headless --load "$sv" --fixed-dt 16 --tick 3 --dump-state "$d0" --exit >/dev/null 2>&1
    ctl_headless --load "$sv" --fixed-dt 16 --fixed-timestep "$fts" \
        --exec "input up 100000" --tick 1875 --dump-state "$d1" --exit >/dev/null 2>&1
    python3 -c "import json,sys,math
a=json.load(open(sys.argv[1]))['hero']; b=json.load(open(sys.argv[2]))['hero']
print(int(math.dist((a['x'],a['y'],a['z']),(b['x'],b['y'],b['z']))))" "$d0" "$d1" 2>/dev/null
    rm -f "$sv" "$d0" "$d1"
}

mild="$(dist_after_hold 16)"
coarse="$(dist_after_hold 100)"
[ -n "$mild" ] && [ -n "$coarse" ] || fail "dump failed (mild='$mild' coarse='$coarse')"
# The hold must actually float the hero a meaningful distance, else the fixture is broken.
[ "$mild" -gt 2000 ] || fail "mild-throttle hold barely moved the hero ($mild); bad fixture"
# The coarse throttle must still float the hero a comparable distance -- the regression this guards
# is the old render-frame count collapsing the coarse run to ~0. A wide band (50%-200% of the mild
# run) keeps that guard sharp (a gobble is a total collapse, far below 50%) while tolerating the
# coarse 10Hz sim's own legitimate drift over a long run, so the test does not flake. Measured
# in-band with huge margin (mild~8600, coarse~8500).
lo=$((mild * 50 / 100))
hi=$((mild * 200 / 100))
if [ "$coarse" -lt "$lo" ] || [ "$coarse" -gt "$hi" ]; then
    fail "throttle collapsed the sustained-hold float: mild=$mild coarse=$coarse (want ${lo}-${hi})"
fi
pass "sustained hold floats the same across the throttle (mild=$mild coarse=$coarse)"
