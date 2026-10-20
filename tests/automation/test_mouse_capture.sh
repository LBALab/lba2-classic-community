#!/usr/bin/env bash
# The mouse camera's relative mode must end when the camera stops asking for it.
#
# Relative mode used to be switched inside GereExtKeys, which returns early indoors and is not
# called at all in menus, camera zones or cutscenes. With free orbit (cam_mouse_drag 0) it was
# switched on outdoors and then left on after the hero went indoors: nothing ran to switch it off.
# Now the camera asks on each frame it orbits, and the present hook (MouseCapture_Update) ends it
# on the second frame that does not ask.
#
# The same run checks that mouse_confine reaches the window: with it off, an interior is free.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=mouse_capture
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

INTERIOR_CUBE=154

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
    --exec-at 15 "cam_follow 1; cam_mouse 1; cam_mouse_drag 0; mouse_confine 2; cubetrace 1" \
    --exec-at 40 "mouse_capture" \
    --exec-at 60 "cube $INTERIOR_CUBE" \
    --exec-at 90 "mouse_capture" \
    --exec-at 100 "mouse_confine 0" \
    --exec-at 110 "mouse_capture" \
    --tick 120 --exit > "$log" 2>&1 || fail "run failed: exit $?"

modes="$(sed -n 's/^\[control\] mouse_capture: //p' "$log" | tr '\n' ' ')"
grep -q "\[cube\] .*-> $INTERIOR_CUBE" "$log" \
    || fail "never reached interior $INTERIOR_CUBE: the run proves nothing"

set -- $modes
[ "${1:-}" = "relative" ] \
    || fail "free orbit outdoors was '${1:-none}', not relative: the camera never asked, so nothing below means anything"
[ "${2:-}" = "confined" ] \
    || fail "indoors after free orbit the mouse was '${2:-none}', want confined (mouse_confine 2): relative mode outlived the camera"
[ "${3:-}" = "free" ] \
    || fail "with mouse_confine 0 indoors the mouse was '${3:-none}', want free: the setting does not reach the window"

pass "relative outdoors, confined indoors, free with mouse_confine 0"
