#!/usr/bin/env bash
# Auto camera: touching the stick must not snap the view through rotation the hero accumulated
# while turning in place (#450).
#
# In hold-angle mode the camera keeps its angle while the hero turns on the spot, so BetaCam and the
# hero-relative target drift apart. Nothing discharges that difference until the player orbits --
# and orbiting also selects the tight manual divisor, so the first frame used to eat half of it. A
# 1-unit stick touch moved the camera 859 units (75 degrees) in one frame.
#
# Drives the whole thing headlessly with `camnudge` (stick stand-in) and reads `camtrace`. Asserts
# both halves: the hold still accumulates a large delta (otherwise the test proves nothing because
# the setup never happened), and the nudge that discharges it moves the camera by what was asked
# for rather than by half the backlog.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=autocam_orbit_snap
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

# 30: orbit away from behind the hero and release. 100: turn him in place for 60 ticks -- the camera
# holds, so the target drifts out from under it. 170: the smallest possible stick touch.
#
# `cam_follow` and `cam_hold_angle` are cvars, so this run writes them back to its settings folder
# on exit; lib.sh hands every test a folder of its own, which is what keeps that off everyone else's
# camera (a stray FollowCamera=1 moves the projection corpus).
ctl_headless --load "$FIX" --fixed-dt 16 \
    --exec "cam_follow 1; cam_hold_angle 1; camtrace 1" \
    --exec-at 30 "camnudge 40 0 10" \
    --exec-at 100 "input left 60" \
    --exec-at 170 "camnudge 1 0 1" \
    --tick 200 --exit > "$log" 2>&1 || fail "run: exit $?"

# The console echo and the log fan-out are separate streams that interleave in the capture, so read
# only the '[INFO] [cam]' lines -- those are in frame order.
read -r delta step < <(awk '
    /^\[INFO\] \[cam\]/ {
        d = $10 - $8                                    # target - BetaCam, shortest way round
        if (d >  2048) d -= 4096
        if (d < -2048) d += 4096
        if (d < 0) d = -d
        if (d > maxd) maxd = d
        if ($16 == "1") { s = $12 + 0; if (s < 0) s = -s; last = s }   # step on each orbit frame
    }
    END { print maxd + 0, last + 0 }
' "$log")

[ "${delta:-0}" -gt 1000 ] \
    || fail "setup did not build a stale angle: hold accumulated only $delta units (wanted > 1000)"

# A 1-unit nudge should move the camera ~1 unit. Allow slack for the lerp's minimum step; the
# regression was 859.
[ "$step" -le 20 ] \
    || fail "a 1-unit stick touch moved the camera $step units in one frame (#450 snap)"

pass "1-unit nudge moved the camera $step units with $delta units of held rotation pending"
