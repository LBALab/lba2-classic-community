#!/usr/bin/env bash
# --tick: the keystone differential test. Proves the loop actually STEPS the simulation,
# not just counts a hook. Same load at two tick counts; assert that tick-sensitive state
# advanced (timer monotonic; hero idle animation frame changed).
TESTNAME=tick
. "$(dirname "$0")/lib.sh"
precheck
need_save

a="$(mktemp)"; b="$(mktemp)"
trap 'rm -f "$a" "$b"' EXIT

ctl --load "$LBA2_TEST_SAVE" --tick 1 --dump-state "$a" --exit >/dev/null 2>&1 \
    || fail "tick 1 run: non-zero exit ($?)"
ctl --load "$LBA2_TEST_SAVE" --tick 60 --dump-state "$b" --exit >/dev/null 2>&1 \
    || fail "tick 60 run: non-zero exit ($?)"

ta=$(jget "$a" "d['tick']"); tb=$(jget "$b" "d['tick']")
[ "$ta" = "1" ]  || fail "tick(a)=$ta (want 1)"
[ "$tb" = "60" ] || fail "tick(b)=$tb (want 60)"

# Hard: the game clock advanced because ManageTime (after the hook) ran 60x vs 1x.
tma=$(jget "$a" "d['timer_ref_hr']"); tmb=$(jget "$b" "d['timer_ref_hr']")
[ "$tmb" -gt "$tma" ] || fail "timer did not advance ($tma -> $tmb): loop body not stepping"

# Corroborating: the hero idle animation advanced frames over 60 ticks (soft — same pose
# could coincidentally land on the same frame, so only warn).
fa=$(jget "$a" "d['hero']['last_frame']"); fb=$(jget "$b" "d['hero']['last_frame']")
note=""
[ "$fa" != "$fb" ] || note=" (warn: hero last_frame unchanged $fa==$fb)"

pass "ticks 1/60, timer $tma->$tmb, anim frame $fa->$fb$note"
