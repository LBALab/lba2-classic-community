#!/usr/bin/env bash
# The Auto camera must stay out of interiors, whatever the player has it set to.
#
# Interiors are isometric: the camera holds a fixed orientation and the scene is drawn from a
# tile-grid anchor, and the camera zones in an interior fire every frame to place it. The Auto
# camera is a third-person follow for exterior perspective scenes and is gated to them, so an
# interior with `cam_follow 1` has to look exactly like an interior with it off.
#
# The gate is a condition repeated at several call sites rather than a single switch, which is the
# kind of thing that gets extended in one place and not the others. The symptom would be an
# isometric scene whose camera slowly rotates behind the hero, which no golden or state dump would
# obviously flag.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=camera_interior
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

INTERIOR_CUBE=154

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

# Walk him about inside, with the Auto camera switched on. If its update runs at all, BetaCam
# lerps toward the angle behind him and the fixed isometric orientation moves.
ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
    --exec "cube $INTERIOR_CUBE" \
    --exec-at 15 "cam_follow 1; camtrace 1" \
    --exec-at 30 "input up 120" \
    --tick 180 --exit > "$log" 2>&1 || fail "run failed: exit $?"

ext="$(cam_col "$log" ext | sort -u | tr -d '\n')"
[ "$ext" = "0" ] \
    || fail "cube $INTERIOR_CUBE did not stay an interior for the whole run (ext saw: $ext)"

follow="$(cam_col "$log" follow | tail -1)"
[ "$follow" = "1" ] \
    || fail "the Auto camera was not actually enabled ($follow): the run proves nothing"

walked="$(cam_col "$log" moving | awk '$1 == 1 { n++ } END { print n + 0 }')"
[ "$walked" -gt 20 ] \
    || fail "the hero barely moved ($walked frames): nothing for a follow camera to have followed"

angles="$(cam_col "$log" beta | sort -u | wc -l)"
[ "$angles" -eq 1 ] \
    || fail "the camera angle moved across $angles values in an interior: the Auto camera is running there"

pass "interior held one camera angle across $walked frames of walking with the Auto camera on"
