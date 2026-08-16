#!/usr/bin/env bash
# Hold-angle is the free-camera contract: once the player has set an angle, the camera keeps it
# while the hero turns underneath, and gives it up only on a scene change or Center camera.
#
# The failure this guards is the quiet one. A camera that re-anchors while the hero turns in place
# reads as the view sliding on its own, and it is invisible to any assertion about where the
# camera ends up, because it ends up somewhere perfectly reasonable: behind the hero. What says
# it went wrong is that it moved at all.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=followcam_hold
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

# Orbit off the hero's back and let go, then turn him on the spot for 60 ticks. The last 40 ticks
# are his turn plus the settle after it, which is the window the camera must sit still through.
cam_run "$log" "" \
    --exec-at 30 "camnudge 30 0 10" \
    --exec-at 100 "input left 60" \
    --tick 190

moved="$(cam_col "$log" step | tail -40 | awk '{ s = ($1 < 0) ? -$1 : $1; if (s > m) m = s } END { print m + 0 }')"
drift="$(cam_lag "$log" | tail -1)"
turned="$(cam_col "$log" heroBeta | awk 'NR == 1 { first = $1 } END { d = $1 - first; if (d < 0) d = -d; print d }')"

# The hero has to have actually turned, or the camera held still against nothing.
[ "$turned" -gt 800 ] \
    || fail "hero barely turned ($turned units): the fixture proved nothing"

[ "$moved" -eq 0 ] \
    || fail "camera moved $moved units while the hero turned in place (hold-angle is not holding)"

# And the hold is real rather than the camera having quietly followed: it should now be a long
# way from the angle an anchored camera would sit at.
[ "$drift" -gt 800 ] \
    || fail "camera ended only $drift units off the hero-relative angle: it tracked the turn"

pass "camera held its angle through $turned units of hero rotation ($drift units off anchor)"
