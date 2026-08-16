#!/usr/bin/env bash
# An orbit interrupted by something else taking the camera must not resume afterwards, and must
# not leave the next orbit thinking it is already under way.
#
# The Auto camera keeps per-frame state while the player drives it: the tail still to be spent
# after they let go, and the flag saying a gesture is in progress that tells the next nudge
# whether to realign first. A camera zone, a cutscene, an interior or switching the Auto camera
# off all take the camera away mid-gesture, and none of them run the code that maintains that
# state -- so left alone it does not end, it waits. The tail then resumes on a camera the player
# has since re-aimed, and the stale flag makes the next nudge skip its realign, which is the #450
# snap again at the worst possible moment: an authored shot is exactly where the held angle and
# the hero-relative one are furthest apart.
#
# Driven here by toggling the Auto camera off and back on, which is the one such discontinuity
# reachable without a scene that scripts a camera.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=followcam_interrupt
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

# Orbit, then pull the camera away while the follow-through is still being spent and hand it
# back. Turn the hero on the spot so the held angle and the hero-relative one diverge, then the
# smallest stick touch there is.
# The stale flag lasts exactly one frame: the frame the camera comes back is the one that still
# reads "a gesture is already under way", and the frame after it clears itself. So the touch has
# to land on that frame, which means issuing it in the same command buffer as the handback.
cam_run "$log" "" \
    --exec-at 30 "camnudge 20 0 20" \
    --exec-at 45 "cam_follow 0" \
    --exec-at 50 "input left 60" \
    --exec-at 130 "cam_follow 1; camnudge 1 0 1" \
    --tick 200

# Everything from here is after the camera came back. The interrupted tail must be gone: the
# camera holds until the hero's turn is done and the final touch arrives.
resumed="$(cam_tsv "$log" | awk '
    NR == 1 { next }
    $7 == 1 { orbit = 1 }                       # an orbit frame after the handback
    orbit == 1 && $5 != 0 { v = $5; if (v < 0) v = -v; if (v > m) m = v }
    END { print m + 0 }')"

# The final 1-unit touch: with the gesture state properly ended it realigns first and moves by
# what was asked. Carrying the stale flag across the interruption skips that and spends half the
# accumulated turn in one frame instead.
last_step="$(cam_tsv "$log" | awk '
    NR == 1 { next }
    $7 == 1 { v = $5; if (v < 0) v = -v; last = v }
    END { print last + 0 }')"

# How far the hero turned across the run, the shortest way round.
#
# The precondition is measured on the hero rather than on the camera's distance from its target,
# because a working engine leaves no trace of the latter: the realign happens in the input pass,
# before the frame's camera update logs anything, so the divergence is already absorbed by the
# time it could be read. Asserting on it would demand the fix fail to fix anything.
turned="$(cam_col "$log" heroBeta | awk '
    NR == 1 { first = $1 }
    END { d = $1 - first; if (d > 2048) d -= 4096; if (d < -2048) d += 4096; print (d < 0) ? -d : d }')"

[ "$turned" -gt 800 ] \
    || fail "hero turned only $turned units while the camera was away: nothing to snap through"

[ "$last_step" -le 20 ] \
    || fail "a 1-unit touch after the interruption moved the camera $last_step units (stale gesture state)"

[ "$resumed" -le 20 ] \
    || fail "interrupted follow-through resumed after the camera came back ($resumed units in a frame)"

pass "gesture ended with the interruption: 1-unit touch moved $last_step units after $turned units of hero turn"
