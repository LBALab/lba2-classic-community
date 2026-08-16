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

# pan_off_centre <log> <standing|walking> -- how far the manual pan sits from centre (AddBetaCam
# the shortest way round, so a pan that has drifted back through zero reads as small), sampled
# either on the last frame before the hero moves or on the final frame.
#
# The sample is found by the trace's own `moving` column rather than by counting rows back from
# the end: camtrace emits on dirty frames, not on ticks, so a row offset is not a point in time
# and any change to how long the camera settles moves it somewhere else entirely.
pan_off_centre() {
    cam_tsv "$1" | awk -v want="$2" '
        NR == 1 { next }
        { a = $2; if (a > 2048) a -= 4096; if (a < 0) a = -a
          # Wait for the pan to exist before looking for the walk: the hero is already
          # settling on the frames after a load, and taking the first moving frame of the
          # run would sample from before the pan was ever applied.
          if (a > 0) panned = 1
          if (panned && $8 != 0) walking = 1
          if (panned && !walking) standing = a
          final = a }
        END { print (want == "standing") ? standing + 0 : final + 0 }'
}

# Pan off centre, stand still for 60 ticks, then walk for 200. The grace window is 60 frames of
# movement, so the walk has to outlast it for the drift to be reached at all.
cam_run "$log" "cam_hold_angle 0;" \
    --exec-at 30 "camnudge 30 0 10" \
    --exec-at 110 "input up 200" \
    --tick 330

standing="$(pan_off_centre "$log" standing)"
walked="$(pan_off_centre "$log" walking)"

[ "$standing" -gt 200 ] \
    || fail "manual pan collapsed to $standing units while the hero stood still (recentred unasked)"

[ "$walked" -lt 100 ] \
    || fail "manual pan still $walked units off centre after walking (classic camera never recentred)"

pass "pan held at $standing units while standing, drifted to $walked while walking"
