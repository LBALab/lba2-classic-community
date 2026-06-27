#!/usr/bin/env bash
# Revert dialog: the keep/revert modal that appears after every successful
# `resolution` console verb (per docs/RUNTIME_RESOLUTION.md). Verifies the
# 15 s timeout path — without any Y/N input the dialog must time out and
# fire the orchestrator a second time to restore the previous resolution.
#
# Under --fixed-dt the dialog's TimerRefHR-based countdown is pumped by
# Timer_FixedDtPump() once per modal iteration, so the 15 s timeout maps
# to ~938 modal iterations rather than 15 wall-clock seconds.
TESTNAME=res_switch_dialog
. "$(dirname "$0")/lib.sh"
precheck
need_save

out="$(mktemp)"
json="$(mktemp)"
trap 'rm -f "$out" "$json"' EXIT

# Switch to 768x480 via console; no input means the dialog times out and
# reverts to 640x480. Dump final state so we can confirm the resolution
# actually went back (the orchestrator's per-switch line is debug-only).
ctl_headless --load "$LBA2_TEST_SAVE" \
    --exec "resolution 768x480" \
    --fixed-dt 16 --tick 1500 --dump-state "$json" --exit >"$out" 2>&1 \
    || fail "exit code $? from forward+revert run (engine hung or crashed)"

# Forward switch fired: the console verb echoes "Resolution: 768x480" on success.
grep -q "^\[control\] Resolution: 768x480" "$out" \
    || fail "forward switch did not fire (no 'Resolution: 768x480' echo)"

# Dialog returned REVERT after the timeout.
grep -q "Res_Switch: revert dialog -> REVERT" "$out" \
    || fail "revert dialog did not time out (no 'revert dialog -> REVERT')"

# Auto-revert took effect: the final render resolution is back to 640x480.
rw=$(jget "$json" "d['scene']['render_w']")
rh=$(jget "$json" "d['scene']['render_h']")
[ "$rw" = "640" ] && [ "$rh" = "480" ] \
    || fail "auto-revert did not restore 640x480 (final ${rw}x${rh})"

pass "forward switch -> 15s timeout -> auto-revert chain works end-to-end"
