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
trap 'rm -f "$out"' EXIT

# Switch to 768x480 via console; no input means the dialog times out and
# reverts to 640x480. Both the forward switch and the auto-revert must log.
ctl_headless --load "$LBA2_TEST_SAVE" \
    --exec "resolution 768x480" \
    --fixed-dt 16 --tick 1500 --exit >"$out" 2>&1 \
    || fail "exit code $? from forward+revert run (engine hung or crashed)"

# Forward switch.
grep -q "Res_Switch: 640x480 -> 768x480 OK" "$out" \
    || fail "forward switch did not fire (no 'Res_Switch: 640x480 -> 768x480 OK')"

# Dialog returned REVERT after the timeout.
grep -q "Res_Switch: revert dialog -> REVERT" "$out" \
    || fail "revert dialog did not time out (no 'revert dialog -> REVERT')"

# Auto-revert orchestrator fire restoring the previous mode.
grep -q "Res_Switch: 768x480 -> 640x480 OK" "$out" \
    || fail "auto-revert switch did not fire (no 'Res_Switch: 768x480 -> 640x480 OK')"

# Console verb still echoes success ("Resolution: ...") before the dialog
# fires — confirms cmd_resolution ran cleanly.
grep -q "^\[control\] Resolution: 768x480" "$out" \
    || fail "missing 'Resolution: 768x480' console echo"

pass "forward switch -> 15s timeout -> auto-revert chain works end-to-end"
