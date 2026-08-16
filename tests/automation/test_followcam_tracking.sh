#!/usr/bin/env bash
# What the player asks the camera for is what the camera should do: hold the stick at a given
# orbit speed and, once the lerp has caught up, the camera turns at that speed.
#
# This is the axis every camera tuning change moves by accident. Anything touching the lerp
# divisor, the minimum-step clamp or the snap threshold can leave the camera running at a
# fraction of, or a multiple of, the speed asked for, and it will still look like a working
# camera until someone compares it against the input. Three speeds, because a divisor bug
# scales and a clamp bug shows only at the small end.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=followcam_tracking
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

# steady <log> -- the camera's settled orbit speed: the fastest step it sustains for several
# frames. Not the plain maximum, which a one-frame transient would win, and not the most common
# either: the camera spends its first frames after a load creeping toward the angle it wants at
# the minimum-step clamp, and on a short run there are more of those frames than orbit ones.
steady() {
    cam_col "$1" step | awk '
        { v = ($1 < 0) ? -$1 : $1; if (v > 0) n[v]++ }
        END { for (v in n) if (n[v] >= 5 && v + 0 > win + 0) win = v; print win + 0 }'
}

fails=""
for want in 8 20 40; do
    cam_run "$log" "" --exec-at 40 "camnudge $want 0 40" --tick 120
    got="$(steady "$log")"

    # One unit of slack: the lerp works in whole units and its minimum-step clamp can round a
    # frame either way without the motion reading as wrong.
    diff=$(( got - want ))
    [ "$diff" -lt 0 ] && diff=$(( -diff ))
    [ "$diff" -le 1 ] || fails="$fails asked=$want got=$got"
done

[ -z "$fails" ] \
    || fail "camera did not orbit at the speed asked for:$fails"

pass "camera tracked the stick at 8, 20 and 40 units/frame"
