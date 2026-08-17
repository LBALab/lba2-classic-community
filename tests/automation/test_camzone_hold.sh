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

# How long the shot survives is cam_hold_angle's decision, and both of its answers have to keep
# working. On (the default) is the free camera: the angle is held until the player moves it, which
# is what the runs above cover. Off is the classic lazy drift, where the shot should be handed
# back as the hero walks on rather than kept. Adopting the angle is what makes the second possible
# at all: the drift acts on AddBetaCam, so a shot the Auto camera never adopted is a shot it has
# no way to give back gradually.
drift="$(mktemp)"
ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
    --exec "cube 90" \
    --exec-at 15 "camtrace 1; cam_follow 1; cam_hold_angle 0" \
    --exec-at 40 "teleport 18000 3900 8000" \
    --exec-at 70 "teleport 13000 4000 8000" \
    --exec-at 90 "input up 200" \
    --tick 300 --exit > "$drift" 2>&1 || fail "drift run failed: exit $?"

peak="$(cam_col "$drift" add | awk '{ a = $1; if (a > 2048) a -= 4096; if (a < 0) a = -a; if (a > m) m = a } END { print m + 0 }')"
left="$(cam_col "$drift" add | tail -1 | awk '{ a = $1; if (a > 2048) a -= 4096; print (a < 0) ? -a : a }')"
rm -f "$drift"

[ "$peak" -gt 200 ] \
    || fail "the shot never moved the camera off the hero-relative angle ($peak units): nothing to hand back"
[ "$left" -lt 100 ] \
    || fail "with cam_hold_angle off the shot was still $left units off centre after walking (never handed back)"

pass "both cameras left the shot at $auto_beta, the stick moved it to $moved, and drift mode handed $peak units back to $left"
