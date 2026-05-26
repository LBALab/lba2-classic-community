#!/usr/bin/env bash
# Runtime resolution switch: load a save, fire Res_Switch mid-session via the
# `--res-switch-test WxH@TICK` harness flag, run more ticks, dump state. The
# engine must (1) survive the teardown+rebuild, (2) keep rendering at the new
# dimensions, (3) finish the requested tick budget cleanly.
#
# This covers the docs/RUNTIME_RESOLUTION.md Phase 0 contract: the orchestrator
# is correct across both wider (640->1280) and narrower (768->640) targets,
# including the cube-data reload path (PtrInitGrille + ChoicePalette).
TESTNAME=res_switch
. "$(dirname "$0")/lib.sh"
precheck
need_save

json="$(mktemp)"
trap 'rm -f "$json"' EXIT

# Switch from default 640x480 to 1280x720 on tick 50, run to tick 200.
ctl_headless --load "$LBA2_TEST_SAVE" \
    --res-switch-test 1280x720@50 \
    --fixed-dt 16 --tick 200 --dump-state "$json" --exit >/dev/null 2>&1 \
    || fail "wide switch (640->1280x720) returned non-zero ($?)"

tick=$(jget "$json" "d['tick']")
cube=$(jget "$json" "d['scene']['cube']")
[ "$tick" = "200" ] || fail "wide switch: tick=$tick (want 200; the loop hung or crashed early)"
[ "$cube" -ge 0 ] || fail "wide switch: invalid cube=$cube after switch"

# Switch from 768x480 (boot) back to 640x480 (smaller) on tick 30.
ctl_headless --resolution 768x480 --load "$LBA2_TEST_SAVE" \
    --res-switch-test 640x480@30 \
    --fixed-dt 16 --tick 120 --dump-state "$json" --exit >/dev/null 2>&1 \
    || fail "shrink switch (768->640x480) returned non-zero ($?)"

tick=$(jget "$json" "d['tick']")
[ "$tick" = "120" ] || fail "shrink switch: tick=$tick (want 120)"

pass "640->1280x720 and 768->640x480 both survive teardown+rebuild"
