#!/usr/bin/env bash
# Resolution confirmation: timeout reverts; held D-pad navigation cannot activate
# jetpack or advance gameplay time after confirmation.
#
# Verifies the 15 s timeout path — without any Y/N input the dialog must time out and
# fire the orchestrator a second time to restore the previous resolution.
# Confirming with D-pad Down held must not activate jetpack on return or
# charge the direction-release wait to gameplay time.
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
navuser="$(mktemp -d)"
trap 'rm -f "$out" "$json"; rm -rf "$navuser"' EXIT

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

# Pin the spell binding rather than inheriting the retail install's config.
printf 'WinMode: 1\nGamepad33: 1036\nGamepad33_2: 0\n' > "$navuser/lba2.cfg"
for arm in control held; do
    keys="key 28 3 100" # Y: keep without a navigation gesture.
    if [ "$arm" = held ]; then
        # D-pad Down selects Keep and remains held while Enter confirms.
        keys="key 1036 200 60; key enter 3 100"
    fi
    ctl_headless --user-dir "$navuser" --load "$LBA2_TEST_SAVE" \
        --exec-at 10 "vargame 12 1; behaviour 0; $keys; resolution 768x480" \
        --fixed-dt 16 --tick 16 --dump-state "$json" --exit >"$out" 2>&1 \
        || fail "$arm: direction-release run hung or crashed"
    grep -q "Res_Switch: revert dialog -> KEEP" "$out" \
        || fail "$arm: confirmation did not keep the new resolution"
    behaviour=$(jget "$json" "d['hero']['behaviour']")
    [ "$behaviour" = "0" ] \
        || fail "$arm: dialog navigation activated the jetpack (behaviour=$behaviour)"
    if [ "$arm" = control ]; then
        control_time=$(jget "$json" "d['timer_ref_hr']")
    else
        held_time=$(jget "$json" "d['timer_ref_hr']")
        [ "$held_time" = "$control_time" ] \
            || fail "navigation release advanced gameplay time ($control_time -> $held_time)"
    fi
done

pass "timeout reverts; confirmation drains navigation without activating jetpack or advancing gameplay time"
