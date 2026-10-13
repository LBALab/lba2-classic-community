#!/usr/bin/env bash
# Arriving in an outdoor scene: with cam_scene_carry on, the Auto camera starts behind the hero with the pitch the player had, and with his pan unless he came out of a building.
#
# Two kinds of scene change put the hero outdoors without him walking there across the seam
# between two outdoor scenes, and each does something to the Auto camera that nobody chose. A
# change ordered by a script or the console recentres: the pan goes to zero and the pitch to
# the preset, whatever the player had set. A door out of an interior keeps the pan he had when
# he went in and leaves the yaw where the last outdoor scene left it, so the camera is found off
# to one side of the hero and swings round to a place that is not behind him either.
#
# cam_scene_carry (on by default) has the Auto camera take both arrivals. The
# pitch is left as the player had it. The pan is kept between two outdoor scenes and goes home
# when he comes out of an interior. The camera starts behind the hero at that pan, with nothing
# left to swing through.
#
# The Auto camera's other settings are off throughout (CAM_PLAIN), so that off here is the
# camera as it was before any of them, obstruction search and all.
#
# Each claim is checked against the same run with the setting off, because every one of them
# could otherwise pass by its setup never happening: a door that never fired leaves no arrival
# to judge, and a camera that was never moved is trivially where it started.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=followcam_scene_carry
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

door_off="$(mktemp)"; door_on="$(mktemp)"; warp_off="$(mktemp)"; warp_on="$(mktemp)"
trap 'rm -f "$door_off" "$door_on" "$warp_off" "$warp_on"' EXIT

# A visit to a building with the camera panned. Cube 131 is an outdoor scene whose zone 1 is a
# door into cube 129, and cube 129's zone 0 leads back out to a spot outside every enabled
# camera zone. The hero is warped to 131, the player pans and tilts, and he is put in the
# doorway. Indoors he is put in the way out facing another way, so he comes out with the camera
# nowhere near his back. A pan can only get into an interior by being walked in: an ordered
# change into one recentres.
door_run() { # <log> <cam_scene_carry>
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_follow 1; cam_hold_angle 1; $CAM_PLAIN; cam_scene_carry $2; camtrace 1; cubetrace 1" \
        --exec-at 20 "cube 131" \
        --exec-at 60 "camnudge 20 8 10" \
        --exec-at 110 "teleport 6400 5600 9472 0" \
        --exec-at 170 "teleport 8448 1280 9472 1024" \
        --tick 300 --exit > "$1" 2>&1 || fail "door run failed: exit $?"
    grep -q "\[cube\] island [0-9]*: 131 -> 129" "$1" || fail "the hero never went in by the door of cube 131"
    grep -q "\[cube\] island [0-9]*: 129 -> 131" "$1" || fail "the hero never came out by the door of cube 129"
}

# door_visit <log> -- "<pan indoors> <pitch indoors> <frames outdoors afterwards> <largest
# distance from the camera to its target on them> <camera zones that fired on them> <pan at
# the end> <pitch at the end>".
door_visit() {
    cam_tsv "$1" | awk '
        NR == 1 { for (i = 1; i <= NF; i++) col[$i] = i; next }
        $col["ext"] == 0 { indoors = 1; pan = $col["add"]; pitch = $col["alpha"]; next }
        indoors {
            d = $col["target"] - $col["beta"]
            if (d > 2048) d -= 4096
            if (d < -2048) d += 4096
            if (d < 0) d = -d
            if (d > lag) lag = d
            shots += $col["zone"]
            n++
            endpan = $col["add"]; endpitch = $col["alpha"]
        }
        END { print pan + 0, pitch + 0, n + 0, lag + 0, shots + 0, endpan + 0, endpitch + 0 }'
}

door_run "$door_off" 0
door_run "$door_on" 1

read -r off_pan off_pitch off_frames off_lag off_shots off_endpan off_endpitch <<< "$(door_visit "$door_off")"
read -r on_pan on_pitch on_frames on_lag on_shots on_endpan on_endpitch <<< "$(door_visit "$door_on")"

[ "$off_frames" -gt 50 ] && [ "$on_frames" -gt 50 ] \
    || fail "the hero was not outdoors for long after the building ($off_frames and $on_frames frames)"
[ "$off_shots" -eq 0 ] && [ "$on_shots" -eq 0 ] \
    || fail "a camera zone fired on arrival: this door no longer lands in the open, so the shot decides the view"
[ "$off_pan" -ne 0 ] && [ "$on_pan" -ne 0 ] \
    || fail "the hero went indoors with the pan at $off_pan and $on_pan: no pan for the door to deal with"
[ "$off_endpan" = "$off_pan" ] && [ "$off_lag" -ge 500 ] \
    || fail "with the setting off the hero came out with pan $off_endpan (in with $off_pan) and the camera $off_lag units off its target: nothing for the setting to change"
[ "$on_endpan" -eq 0 ] \
    || fail "with cam_scene_carry on the pan was $on_endpan after the building, where it should have gone home from $on_pan"
[ "$on_lag" -eq 0 ] \
    || fail "with cam_scene_carry on the camera arrived $on_lag units off its target and had to swing round"
[ "$on_endpitch" = "$on_pitch" ] \
    || fail "with cam_scene_carry on the pitch went from $on_pitch to $on_endpitch across the door"

# An ordered change between two outdoor scenes, after the player has panned and tilted. The
# console's `cube` takes the same branch of ChangeCube as a script's change-cube order, and
# cube 66's start point is outside every camera zone.
#
# With the setting off the pitch does not simply go back to the preset here: the recentre's
# obstruction search finds something behind the hero and leaves its own answer, 355 from this
# save. The tilt is chosen to end somewhere else, so that "kept" and "replaced" cannot agree.
warp_run() { # <log> <cam_scene_carry>
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_follow 1; cam_hold_angle 1; $CAM_PLAIN; cam_scene_carry $2; camtrace 1; cubetrace 1" \
        --exec-at 20 "camnudge 20 8 10" \
        --exec-at 80 "cube 66" \
        --tick 160 --exit > "$1" 2>&1 || fail "warp run failed: exit $?"
    grep -q "\[cube\] island [0-9]*: [0-9]* -> 66" "$1" || fail "the warp to cube 66 never happened"
}

# The nudge and its follow-through are over by frame 40 and the warp is at 80, so row 60 is
# the camera the player left; the last row is the camera in the new scene.
before() { cam_col "$1" "$2" | sed -n 60p; }
after() { cam_col "$1" "$2" | tail -1; }

warp_run "$warp_off" 0
warp_run "$warp_on" 1

pan="$(before "$warp_on" add)"
tilt="$(before "$warp_on" alpha)"

[ "$pan" -ne 0 ] || fail "the nudge never panned the camera: nothing to carry"
[ "$(before "$warp_off" add)" = "$pan" ] && [ "$(before "$warp_off" alpha)" = "$tilt" ] \
    || fail "the two runs differ before the warp, so they are not the same scenario"
[ "$(after "$warp_on" zone)" -eq 0 ] && [ "$(cam_col "$warp_on" zone | awk '{ s += $1 } END { print s + 0 }')" -eq 0 ] \
    || fail "a camera zone fired during the warp run: cube 66 no longer starts in the open"

[ "$(after "$warp_off" add)" -eq 0 ] && [ "$(after "$warp_off" alpha)" != "$tilt" ] \
    || fail "with the setting off the warp kept pan $(after "$warp_off" add) and pitch $(after "$warp_off" alpha): nothing for the setting to carry"
[ "$(after "$warp_on" add)" = "$pan" ] \
    || fail "with cam_scene_carry on the pan went from $pan to $(after "$warp_on" add) across the warp"
[ "$(after "$warp_on" alpha)" = "$tilt" ] \
    || fail "with cam_scene_carry on the pitch went from $tilt to $(after "$warp_on" alpha) across the warp"
[ "$(cam_lag "$warp_on" | tail -1)" -eq 0 ] \
    || fail "with cam_scene_carry on the camera was left $(cam_lag "$warp_on" | tail -1) units off its target after the warp"

pass "door: pan $off_pan kept and the camera $off_lag units off its target with the setting off, pan 0 and 0 units with it on, pitch $on_pitch kept; warp: pan $pan and pitch $tilt kept, reset to 0 and $(after "$warp_off" alpha) with it off"
