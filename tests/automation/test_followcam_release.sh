#!/usr/bin/env bash
# Letting go of the stick should ease the camera to a stop, not halt it from full speed.
#
# The lerp ramps the camera up to the stick's speed over several frames but has nothing to ramp
# it back down, so releasing used to take it from full speed to nothing between two frames. The
# assertion is on the size of that first drop, which is what a velocity discontinuity looks like
# in a trace.
#
# The fixture checks its own instrument: the same measurement is taken with the follow-through
# turned off (cam_glide 0), where the drop must be total. A metric that cannot see the halt it
# was written to catch would otherwise pass for the wrong reason.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=followcam_release
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

on="$(mktemp)"; off="$(mktemp)"
trap 'rm -f "$on" "$off"' EXIT

SPEED=20 # units/frame of orbit, held long enough to reach steady state

# drop_pct <log> -- the fall in camera speed, as a percentage of the steady orbit speed, on the
# frame after the last driven one. 100 means it stopped dead.
drop_pct() {
    cam_col "$1" step | awk -v s="$SPEED" '
        { v = ($1 < 0) ? -$1 : $1; row[NR] = v; if (v >= s) last = NR }
        END {
            if (!last) { print "no-steady"; exit }
            printf "%d\n", (row[last] - row[last + 1]) * 100 / s
        }'
}

for mode in on off; do
    glide=$([ "$mode" = on ] && echo "" || echo "cam_glide 0;")
    eval "cam_run \"\$$mode\" \"$glide\" --exec-at 40 \"camnudge $SPEED 0 30\" --tick 120"
done

d_on="$(drop_pct "$on")"
d_off="$(drop_pct "$off")"

case "$d_on$d_off" in *no-steady*) fail "orbit never reached $SPEED units/frame: nothing to measure" ;; esac

# Instrument check first: with the follow-through off this must read as a full stop, or the
# measurement is not sensitive to the thing being asserted.
[ "$d_off" -ge 90 ] \
    || fail "instrument check failed: cam_glide 0 should halt the camera, measured a ${d_off}% drop"

# A tail that is still 50% of the orbit speed on its first frame is not a stop being eased.
[ "$d_on" -lt 50 ] \
    || fail "camera lost ${d_on}% of its speed in one frame on release (halt, not an ease-out)"

# The tail has to end: a follow-through that keeps creeping is its own bug.
tail_frames="$(cam_col "$on" step | awk '
    { v = ($1 < 0) ? -$1 : $1; if (v >= 20) last = NR; if (v > 0) moving = NR }
    END { print moving - last }')"
[ "$tail_frames" -le 25 ] \
    || fail "camera still drifting $tail_frames frames after release (tail does not settle)"

pass "release eased out: ${d_on}% first-frame drop over $tail_frames frames (halt measures ${d_off}%)"
