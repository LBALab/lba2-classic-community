#!/usr/bin/env bash
# A camera zone's authored angle must survive the Auto camera being switched on.
#
# Most camera zones re-aim the camera once as the hero enters and then latch, leaving the angle
# where they put it. The classic camera shows exactly that. The Auto camera derives its own target
# from the hero's facing and is not told about the zone's write, so it used to read the authored
# angle as catch-up owed and lerp out of the shot over the following second: a cut to the shot,
# then a slide back to behind the hero.
#
# The assertion is that the two cameras agree on where the shot leaves the view. That states the
# property rather than a number, so it survives anyone retuning the lerp, and it cannot pass by
# the shot failing to happen, which is checked separately.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=camzone_hold
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

auto="$(mktemp)"; classic="$(mktemp)"; orbit="$(mktemp)"
trap 'rm -f "$auto" "$classic" "$orbit"' EXIT

# Cube 90's zone at (10752,3900,3072)-(16896,5644,15872) carries ZONE_ON without
# ZONE_OBLIGATOIRE, so it fires once on entry: step out of the box and back in to trigger it, then
# hold still long enough for a drift to show.
zone_run() { # <log> <extra first-tick cmds>
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cube 90" \
        --exec-at 15 "camtrace 1; $2" \
        --exec-at 40 "teleport 18000 3900 8000" \
        --exec-at 70 "teleport 13000 4000 8000" \
        --tick 160 --exit > "$1" 2>&1 || fail "run failed: exit $?"
}

zone_run "$auto" "cam_follow 1"
zone_run "$classic" "cam_follow 0"

fired="$(cam_col "$auto" zone | awk '$1 == 1 { n++ } END { print n + 0 }')"
[ "$fired" -ge 1 ] || fail "the camera zone never fired: the teleport missed the box"

auto_beta="$(cam_col "$auto" beta | tail -1)"
classic_beta="$(cam_col "$classic" beta | tail -1)"

[ "$auto_beta" = "$classic_beta" ] \
    || fail "authored angle survives the classic camera at $classic_beta but the Auto camera left it at $auto_beta"

# The shot holds, it is not nailed down: the player can still orbit away from it afterwards.
ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
    --exec "cube 90" \
    --exec-at 15 "camtrace 1; cam_follow 1" \
    --exec-at 40 "teleport 18000 3900 8000" \
    --exec-at 70 "teleport 13000 4000 8000" \
    --exec-at 100 "camnudge 20 0 10" \
    --tick 160 --exit > "$orbit" 2>&1 || fail "orbit run failed: exit $?"

moved="$(cam_col "$orbit" beta | tail -1)"
[ "$moved" != "$auto_beta" ] \
    || fail "the camera would not orbit away from the authored angle ($moved): the shot is stuck, not held"

pass "both cameras left the shot at $auto_beta, and the stick still moved it to $moved"
