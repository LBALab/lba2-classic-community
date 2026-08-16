#!/usr/bin/env bash
# Camera zones come in two shapes, and the difference is a flag in the authored data.
#
# A zone carrying ZONE_OBLIGATOIRE re-aims the camera on every frame the hero is inside it, so it
# holds the view for as long as he stays. Without that flag the dispatch latches ZONE_ACTIVE after
# the first fire and does not fire again, so the zone re-aims the camera once on entry and then
# leaves it alone; leaving the box clears the latch, so walking back in re-aims it again. Both
# shapes are in the shipped data and the non-forced one is by far the more common, so a fixture
# that only ever saw a forced zone would be describing the exception.
#
# Constructed rather than found: `cube` moves to a scene known to hold each shape and `teleport`
# puts the hero in and out of the box, so neither case depends on a save being parked somewhere
# particular.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=camzone_model
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

# zone_frames <log> -- how many frames a camera zone re-aimed the camera on, and the longest
# unbroken run of them. A forced zone gives one long run; a latching one gives single frames.
zone_frames() {
    cam_col "$1" zone | awk '
        { if ($1 == 1) { total++; run++; if (run > best) best = run } else run = 0 }
        END { print total + 0, best + 0 }'
}

# --- forced: cube 97 zone 1, flags 0x0b, holds the view while the hero is inside -------------
ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
    --exec "cam_follow 1; camtrace 1" \
    --exec-at 20 "teleport 15000 1000 21000" \
    --tick 120 --exit > "$log" 2>&1 || fail "forced-zone run: exit $?"

read -r total best < <(zone_frames "$log")
[ "$total" -gt 0 ] || fail "forced camera zone never engaged: the teleport missed the box"
[ "$best" -ge 30 ] \
    || fail "forced camera zone held the view for only $best consecutive frames (expected it to hold throughout)"

forced_total="$total"

# --- latching: cube 90 zone 48, flags 0x03, fires once per entry ------------------------------
# Out of the box, back in, out and back in again: two entries, so a zone that re-arms on leaving
# is distinguishable from one that simply fired once and stopped.
ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
    --exec "cube 90" \
    --exec-at 15 "cam_follow 1; camtrace 1" \
    --exec-at 40 "teleport 18000 3900 8000" \
    --exec-at 70 "teleport 13000 4000 8000" \
    --exec-at 100 "teleport 18000 3900 8000" \
    --exec-at 130 "teleport 13000 4000 8000" \
    --tick 190 --exit > "$log" 2>&1 || fail "latching-zone run: exit $?"

read -r total best < <(zone_frames "$log")

[ "$total" -ge 2 ] \
    || fail "latching camera zone fired $total time(s) across two entries: leaving does not re-arm it"
[ "$best" -eq 1 ] \
    || fail "latching camera zone held the view for $best consecutive frames (expected one per entry)"

pass "forced zone held $forced_total frames; latching zone fired $total times, longest run $best"
