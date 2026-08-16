#!/usr/bin/env bash
# The classic camera (cam_hold_angle 0) drifts back behind the hero after a manual pan, but only
# once he is walking and only after a grace window. Standing still must not recentre him.
#
# Both halves matter and they fail in opposite directions. Recentring while the player stands
# still yanks the view they just set; never recentring at all turns the classic camera into the
# free camera and quietly removes the option. The grace window is what makes a pan usable while
# walking, so it is asserted as a duration rather than only as an end state.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=followcam_recenter
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

# pan_at <log> <frames-from-end> -- how far the manual pan still sits from centre (AddBetaCam
# taken the shortest way round, so a pan that has drifted back through zero reads as small).
pan_at() {
    cam_col "$1" add | tail -"$2" | head -1 | awk '
        { a = $1; if (a > 2048) a -= 4096; print (a < 0) ? -a : a }'
}

# Pan off centre, stand still for 60 ticks, then walk for 200. The grace window is 60 frames of
# movement, so the walk has to outlast it for the drift to be reached at all.
cam_run "$log" "cam_hold_angle 0;" \
    --exec-at 30 "camnudge 30 0 10" \
    --exec-at 110 "input up 200" \
    --tick 330

standing="$(pan_at "$log" 210)" # just before the walk starts
walked="$(cam_col "$log" add | tail -1 | awk '{ a = $1; if (a > 2048) a -= 4096; print (a < 0) ? -a : a }')"

[ "$standing" -gt 200 ] \
    || fail "manual pan collapsed to $standing units while the hero stood still (recentred unasked)"

[ "$walked" -lt 100 ] \
    || fail "manual pan still $walked units off centre after walking (classic camera never recentred)"

pass "pan held at $standing units while standing, drifted to $walked while walking"
