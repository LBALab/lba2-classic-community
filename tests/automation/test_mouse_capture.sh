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
# Chain lib.sh's cleanup so its temporary user folder goes too.
trap 'rm -f "$log" "${log2:-}"; _lib_cleanup' EXIT

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

# Above the step rate (VSync off, a fast display) most frames run no simulation step, and the
# camera asks from inside one. Relative mode must hold across those frames rather than end and
# come back once a step. --fixed-dt 2 gives eight frames a 16 ms step; a right-drag sampled on
# nine frames in a row must read relative on every one.
log2="$(mktemp)"
samples=()
for t in 100 101 102 103 104 105 106 107 108; do
    samples+=(--exec-at "$t" "mouse_capture")
done
ctl_headless --load "$CAM_SAVE" --fixed-dt 2 \
    --exec-at 15 "cam_follow 1; cam_mouse 1; cam_mouse_drag 1; mouse_confine 2" \
    --exec-at 60 "mouse 6 0 100" \
    "${samples[@]}" \
    --tick 120 --exit > "$log2" 2>&1 || fail "fast-frame run failed: exit $?"

fast="$(sed -n 's/^\[control\] mouse_capture: //p' "$log2" | tr '\n' ' ')"
set -- $fast
[ "$#" = 9 ] || fail "expected 9 samples of the fast-frame drag, got '$fast'"
case " $fast " in
*" confined "* | *" free "*) fail "a right-drag at eight frames a step left relative mode between steps: $fast" ;;
esac

pass "relative outdoors, confined indoors, free with mouse_confine 0, and held across frames without a step"
