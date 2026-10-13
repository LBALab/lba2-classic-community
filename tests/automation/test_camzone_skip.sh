#!/usr/bin/env bash
# With cam_zone_skip on, a camera zone that only latches is left unfired while the player steers under the Auto camera; forced zones and the classic camera are unaffected.
#
# A latching zone cuts the camera to an authored shot once as the hero walks in. Under the
# classic camera that is the only way the view ever suits the place. Under the Auto camera the
# player already has a view of his own, and the cut takes it from him. cam_zone_skip (a
# prototype, off by default) has the dispatch pass such a zone over while the Auto camera has the
# view outdoors and the hero is under manual control.
#
# What must not change with it on: a forced zone holds the view for as long as the hero is
# inside, and the classic camera gets every shot it got before. Each is its own run here, since
# a setting that switched camera zones off altogether would pass the first check alone.
#
# Not covered: a hero a script is moving still gets the shot. Nothing the console offers puts
# him under a script's control, so that half rests on the condition as written.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=camzone_skip
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

# zone_frames <log> -- how many frames a camera zone re-aimed the camera on, and the longest
# unbroken run of them.
zone_frames() {
    cam_col "$1" zone | awk '
        { if ($1 == 1) { total++; run++; if (run > best) best = run } else run = 0 }
        END { print total + 0, best + 0 }'
}

# Cube 90 zone 48 latches (flags 0x03). The hero is put out of its box, in, out and in again,
# as test_camzone_model.sh does, so there are two entries to fire on. The trace starts after
# the scene has loaded, which keeps the shot the zone takes on arrival out of the count.
latching_run() { # <cam_follow> <cam_zone_skip>
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cube 90" \
        --exec-at 15 "cam_follow $1; cam_hold_angle 1; cam_zone_skip $2; camtrace 1" \
        --exec-at 40 "teleport 18000 3900 8000" \
        --exec-at 70 "teleport 13000 4000 8000" \
        --exec-at 100 "teleport 18000 3900 8000" \
        --exec-at 130 "teleport 13000 4000 8000" \
        --tick 190 --exit > "$log" 2>&1 || fail "latching-zone run (cam_follow $1, cam_zone_skip $2): exit $?"
    grep -q "^\[INFO\] \[cam\]" "$log" || fail "no camera trace in the latching-zone run"
}

latching_run 1 0
read -r off_total _ < <(zone_frames "$log")
[ "$off_total" -ge 2 ] \
    || fail "with the setting off the latching zone fired $off_total time(s) across two entries: nothing for the setting to pass over"

latching_run 1 1
read -r on_total _ < <(zone_frames "$log")
[ "$on_total" -eq 0 ] \
    || fail "with cam_zone_skip on the latching zone still took the view $on_total time(s) from a steering player"

latching_run 0 1
read -r classic_total _ < <(zone_frames "$log")
[ "$classic_total" -ge 2 ] \
    || fail "with cam_zone_skip on the classic camera got the shot $classic_total time(s) across two entries: the setting is the Auto camera's alone"

# Cube 97 zone 1 is forced (flags 0x0b): it holds the view while the hero is inside, whoever is
# steering.
ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
    --exec "cam_follow 1; cam_hold_angle 1; cam_zone_skip 1; camtrace 1" \
    --exec-at 20 "teleport 15000 1000 21000" \
    --tick 120 --exit > "$log" 2>&1 || fail "forced-zone run: exit $?"

read -r forced_total forced_best < <(zone_frames "$log")
[ "$forced_best" -ge 30 ] \
    || fail "with cam_zone_skip on the forced zone held the view for only $forced_best consecutive frames ($forced_total in all)"

pass "latching zone fired $off_total times with the setting off and $on_total with it on; classic camera still got $classic_total; forced zone held $forced_best frames"
