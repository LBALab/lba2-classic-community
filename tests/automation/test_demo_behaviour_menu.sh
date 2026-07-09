#!/usr/bin/env bash
# #358 regression: the fixed-timestep throttle must not skip a full-scene-flip frame.
#
# In the attract demo a scripted behaviour change (LM_COMPORTEMENT_HERO) flashes the behaviour
# menu, suppressed during a scene flip (FirstTime == AFF_ALL_FLIP) so it does not pop over scene
# setup / a cutscene. That suppression reads FirstTime on the same frame AffScene consumes it. If
# the throttle skips the flip frame, AffScene still resets FirstTime, the deferred read a frame
# later sees the stale value, and the menu leaks over every scene start (the reported symptom).
# The fix forces a flip frame to simulate (Timer_ForceStepIfPending scene-flip case, PERSO.CPP).
#
# Test: run the SAME demo window (same --fixed-dt, same --tick) with the throttle off and with it
# on forcing frame-skips; toggling only the throttle must not change WHICH scenes show the menu.
# Before the fix the throttled run leaked the menu into many extra scene-start cubes.
#
# The [demo] line is a Log_Debug at the guard, so the run needs --log-level debug. Local-only:
# needs retail data + a build; skips cleanly otherwise.
TESTNAME=demo_behaviour_menu
. "$(dirname "$0")/lib.sh"
precheck

base="$(mktemp)"; thr="$(mktemp)"
trap 'rm -f "$base" "$thr"' EXIT

# Unique cubes where the behaviour menu was SHOWN over a fixed demo window. --fixed-dt makes it
# deterministic; the extra arg toggles only the throttle (FixedTimestep).
shown() {
    ctl_headless --log-level debug --exec "cube 193" --demo --fixed-dt 40 --tick 8000 "$@" --exit 2>&1 1>/dev/null \
        | sed -n 's/.*\[demo\] LM_COMPORTEMENT_HERO cube=\([0-9]*\).*menu=1/\1/p' | sort -un
}

shown --fixed-timestep 0   > "$base"   # no throttle: every frame simulates (ground truth)
shown --fixed-timestep 100 > "$thr"    # throttle forcing frame-skips (the regression path)

[ -s "$base" ] || fail "no behaviour-menu events in the no-throttle run: demo did not run or the trace is missing"

if ! diff -q "$base" "$thr" >/dev/null; then
    fail "throttle changed which scenes show the behaviour menu (leaks over scene setup, #358):
  no-throttle: $(tr '\n' ' ' < "$base")
  throttle   : $(tr '\n' ' ' < "$thr")"
fi

pass "behaviour-menu scenes identical with/without the throttle: $(tr '\n' ' ' < "$base")"
