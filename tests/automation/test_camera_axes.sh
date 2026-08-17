#!/usr/bin/env bash
# Elevation and zoom: the two Auto-camera axes that are not orbit.
#
# Orbit has had all the attention because that is where the bugs were, and the other two axes go
# through the same nudge with none of the coverage. Both have limits that matter: elevation past
# its range puts the camera under the floor or overhead, and zoom past its range puts it inside
# the hero or out where he is a speck. Both are also meant to leave the orbit alone, which is what
# lets a player tilt or zoom without the follow re-engaging behind them.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=camera_axes
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

ALPHA_MIN=150   # FOLLOW_CAM_ALPHA_MIN
ALPHA_MAX=600   # FOLLOW_CAM_ALPHA_MAX
DIST_MIN=7000   # FOLLOW_CAM_DIST_MIN, the boom's closest approach
DIST_MAX=20000  # FOLLOW_CAM_DIST_MAX
K_NUMPAD_DIV=84 # zoom in
K_NUMPAD_MUL=85 # zoom out

log="$(mktemp)"; state="$(mktemp)"
trap 'rm -f "$log" "$state"' EXIT

# --- elevation ------------------------------------------------------------------------------
# Driven hard in both directions, long enough to sit on each limit.
cam_run "$log" "" \
    --exec-at 20 "camnudge 0 40 40" \
    --exec-at 90 "camnudge 0 -40 60" \
    --tick 180

read -r lo hi < <(cam_col "$log" alpha | awk '
    NR == 1 { lo = hi = $1 }
    { if ($1 < lo) lo = $1; if ($1 > hi) hi = $1 }
    END { print lo, hi }')

[ "$hi" = "$ALPHA_MAX" ] \
    || fail "tilting up stopped at $hi rather than the $ALPHA_MAX ceiling"
[ "$lo" = "$ALPHA_MIN" ] \
    || fail "tilting down stopped at $lo rather than the $ALPHA_MIN floor"

# Elevation is not an orbit: it must not select the tight orbit lerp or re-arm the follow's
# re-engage window, or tilting the camera would quietly change how it tracks the hero.
orbited="$(cam_col "$log" orbit | awk '$1 == 1 { n++ } END { print n + 0 }')"
[ "$orbited" -eq 0 ] \
    || fail "tilting raised the manual-orbit flag on $orbited frame(s): elevation is being treated as an orbit"

armed="$(cam_col "$log" delay | awk '$1 != 0 { n++ } END { print n + 0 }')"
[ "$armed" -eq 0 ] \
    || fail "tilting re-armed the re-engage window on $armed frame(s)"

# --- zoom -----------------------------------------------------------------------------------
# Read from --dump-state rather than the trace: the trace carries VueDistance, which is the
# spring arm's eased length and overshoots slightly on its way to rest. FollowCamBaseDist is the
# value the limits are applied to.
zoom_to() { # <key> -> the resting boom length after holding it far longer than the range
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_follow 1" \
        --exec-at 10 "key $1 100" \
        --tick 130 --dump-state "$state" --exit > /dev/null 2>&1 || fail "zoom run: exit $?"
    python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['camera']['base_dist'])" "$state"
}

near="$(zoom_to $K_NUMPAD_DIV)"
far="$(zoom_to $K_NUMPAD_MUL)"

[ "$near" = "$DIST_MIN" ] \
    || fail "zooming in came to rest at $near rather than the $DIST_MIN floor"
[ "$far" = "$DIST_MAX" ] \
    || fail "zooming out came to rest at $far rather than the $DIST_MAX ceiling"

pass "elevation held $ALPHA_MIN..$ALPHA_MAX without touching the orbit; zoom held $near..$far"
