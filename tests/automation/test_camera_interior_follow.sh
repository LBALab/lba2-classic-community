#!/usr/bin/env bash
# The interior follow keeps the hero in the middle of the isometric view, the switch is what does it,
# and a camera zone still owns the view.
#
# An interior's camera IS the tile it is centred on (`StartXCube`/`StartZCube`, INTEXT.CPP's
# CameraCenter), so "the camera follows the player" there is not an angle and cannot be read off the
# trace's angle columns: they are inert in an isometric scene. What moves is two integers, and the
# trace reports them beside the hero's own tile (`tx`/`tz` against `hx`/`hz`, FOLLOWCAM.CPP), which is
# what makes this assertable at all.
#
# Four things, and the second and third are why the first means anything:
#
#   1  WITH THE SWITCH ON, the view sits on his tile: `tx == hx` and `tz == hz` for most of the walk.
#   2  THE SAME WALK WITH IT OFF does not: the classic camera recentres only once he reaches the screen
#      margin, so it is behind him for the whole run. An arm that only ran the first would pass on a
#      build where something else kept him centred.
#   3  IT NEVER STEPS PAST HIM. The classic recentre moves by `want + (want - now) / 2`, which
#      overshoots and comes back -- invisible in a one-shot recentre of a hero nobody is looking at,
#      and a visible wobble in a camera that runs on consecutive frames. The follow's own step
#      (FOLLOWCAM_MATH.H's `FollowCamIsoStep`) is monotone, so the SIGN of the gap between the view's
#      tile and his never changes. This is the assertion the classic formula would fail, which is the
#      point: it distinguishes the two rules and not merely "something moved".
#   4  THE CROSSING IS A PAN AND NOT A CUT. A tile is 24 pixels, so a view that only ever sits on one
#      jumps that much every time he crosses a brick -- which is what the classic arm's eye does. With
#      the switch on the eye sits between tiles (`512*tx + lagx` is where it actually is), so the
#      trace's `lag*` columns are non-zero across a crossing and the eye never moves a whole brick in
#      a frame. `bricksteps=0` then says the cut is gone, and `lagframes` that it was replaced by
#      something rather than nothing.
#   5  AND THAT PAN IS MADE OF ONE FRAME'S WALK AT A TIME, which is the property a pan can still miss:
#      an eye eased toward his TILE is eased toward something that does not move while he walks a
#      brick, so it holds the picture still for the length of the brick and then covers the whole of
#      it in a handful of frames -- measured below at fifteen still frames and then 256+128+64+32+16
#      units in five, which reads as a jump however smoothly the catch-up is shaped. `stillrun` is the
#      longest stretch of frames in which he moved and the eye did not: 15 for that rule, at most one
#      for this one. It is asserted on the tail, so the placement a scene load makes is not what it
#      measures.
#   5b AND THE EYE IS ON HIM AT THE END OF EVERY FRAME, not one of his steps behind. An eye that EASES
#      toward him (which is what this had, and what "smooth" sounds like it wants) is still up to 39
#      units behind the position it was placed from -- a pixel of picture -- for as long as he walks,
#      and then closes that when he stops: the room slides the last pixel under a hero who is not
#      moving, which reads as the screen jumping by itself, and is worst after a tap. `eyelag` is the
#      distance between the eye the glide placed and the position he was at when it placed it: 39 for
#      the eased rule, zero for one that places it on him.
#   6  A CAMERA ZONE STILL WINS, in an interior and with the switch on. Every interior camera that is
#      not this option's is a zone's (`SetZoneCamera` re-places `Start*Cube` every frame while the hero
#      is in the box), and a following camera that overrode one would be a room whose authored shot
#      snapped back the frame after the script set it. The third arm is a room where a zone holds the
#      view for the whole run: the camera's tile must not move at all, wherever the hero walks.
#
# Every number below is measured on this tree against the corpus save (both arms of the same walk,
# cube 143, and the zoned room 154), not chosen: 93 of 95 traced frames on his tile with the switch on
# against 0 of 95 with it off, a single camera tile for the whole zoned run, at most one frame of his
# walk with the picture standing still (15 for an eye eased toward the tile instead), and no frame in
# which the eye is anywhere but on him (39 units behind for an eye eased toward his position instead).
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
# Tier: small
TESTNAME=camera_interior_follow
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

# A room with no camera zone in it (checked with `cam_interior 1; camtrace 1`: ext=0, zone=0 for the
# whole run), and one where a zone holds the view (zone=1). Both are interiors of the same island.
INTERIOR_CUBE=143
ZONED_CUBE=154
# The walk. `input up` inside one of these rooms carries him about a brick, which is all the follow
# needs to be seen: a camera that never leaves the tile he started on proves nothing. Longer walks
# leave the room through a door, and the scene change is not what this fixture is about.
WALK=45
WALK_TICKS=140
ZONED_TICKS=180
# Traced rows dropped before the tile comparison: the scene load places the view on him, so the first
# rows of BOTH arms agree by construction and would flatter the control.
WARMUP_ROWS=30

# arm <cube> <cam_interior 0|1> <ticks> <log> -- the same walk, with the switch in the given state.
arm() {
    local cube="$1" on="$2" ticks="$3" log="$4"

    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cube $cube" \
        --exec-at 15 "cam_interior $on; camtrace 1" \
        --exec-at 30 "input up $WALK" \
        --tick "$ticks" --exit > "$log" 2>&1 \
        || fail "run in cube $cube with cam_interior $on failed: exit $? (see $log)"
}

# stats <log> -- what the fixture reads, one `key=value` per line.
stats() {
    cam_tsv "$1" | python3 -c '
import sys

warm = int(sys.argv[1])
hdr = None
rows = []
for line in sys.stdin:
    f = line.rstrip("\n").split("\t")
    if hdr is None:
        hdr = f
        continue
    if len(f) != len(hdr):
        continue
    rows.append(dict(zip(hdr, f)))

if not rows:
    print("rows=0")
    raise SystemExit

tail = rows[warm:]
on_tile = sum(1 for r in tail if r["tx"] == r["hx"] and r["tz"] == r["hz"])

# A flip is the gap to the hero changing sign on one axis: the view stepped past him. Frames where
# either side is ON his tile are not flips -- that is arrival.
flips = 0
prev = (0, 0)
for r in rows:
    dx = int(r["tx"]) - int(r["hx"])
    dz = int(r["tz"]) - int(r["hz"])
    now = ((dx > 0) - (dx < 0), (dz > 0) - (dz < 0))
    for axis in (0, 1):
        if prev[axis] and now[axis] and now[axis] != prev[axis]:
            flips += 1
    prev = now

# THE PAN. `lagx`/`lagz` are where the eye sits relative to the tile the view is centred on, so
# `512*tx + lagx` is where it actually is: the frame it moves a whole brick there is the cut this
# option exists to remove, and the classic camera makes exactly that move (its eye IS the tile).
# Counted on the tail, like the tile comparison above, so the placement a scene load makes -- a
# deliberate cut on both arms -- is not what is being measured.
eye = [(512 * int(r["tx"]) + int(r["lagx"]), 512 * int(r["tz"]) + int(r["lagz"])) for r in rows]
lag_frames = 0
brick_steps = 0
for i in range(1, len(rows)):
    is_tail = i >= warm
    if is_tail and (int(rows[i]["lagx"]) or int(rows[i]["lagz"])):
        lag_frames += 1
    step_x = abs(int(rows[i]["tx"]) - int(rows[i - 1]["tx"]))
    step_z = abs(int(rows[i]["tz"]) - int(rows[i - 1]["tz"]))
    # One tile at a time on an axis: a walk, not a teleport or a scene change.
    if is_tail and ((step_x <= 1 and abs(eye[i][0] - eye[i - 1][0]) >= 512)
                    or (step_z <= 1 and abs(eye[i][1] - eye[i - 1][1]) >= 512)):
        brick_steps += 1

print("rows=%d" % len(rows))
print("iso=%s" % ",".join(sorted(set(r["iso"] for r in rows))))
print("ext=%s" % ",".join(sorted(set(r["ext"] for r in rows))))
print("zone=%s" % ",".join(sorted(set(r["zone"] for r in rows))))
print("walked=%d" % sum(1 for r in rows if r["moving"] == "1"))
print("tail=%d" % len(tail))
print("ontile=%d" % on_tile)
print("sxdistinct=%d" % len(set(int(r["tx"]) for r in rows)))
print("szdistinct=%d" % len(set(int(r["tz"]) for r in rows)))
print("flips=%d" % flips)
# THE MOTION ITSELF. `stillrun` is the longest run of frames in which the hero moved and the eye did
# not: the shape of a camera that catches up a tile at a time, rather than one that moves with him.
# A frame in which he moved and the eye did not is the picture standing still under a walking hero,
# which is what a follow camera is for not doing; one or two of them is a rounding, fifteen is a cut
# the eye then flies. Counted on the tail, like the rest.
still_run = 0
still_max = 0
for i in range(1, len(rows)):
    stepped = abs(eye[i][0] - eye[i - 1][0]) or abs(eye[i][1] - eye[i - 1][1])
    if i >= warm and rows[i]["moving"] == "1" and not stepped:
        still_run += 1
        if still_run > still_max:
            still_max = still_run
    else:
        still_run = 0

print("lagframes=%d" % lag_frames)
print("bricksteps=%d" % brick_steps)
print("stillrun=%d" % still_max)

# THE EYE IS ON HIM, not one of his steps behind. The trace is emitted BEFORE the glide of its own
# frame, so an eye placed on the hero reads exactly the position he held on the PREVIOUS row; an eye
# that eases toward him reads that plus whatever lag it still has. Both are compared on the tail, like
# the rest, so the placement a scene load makes is not what this reads.
eyelag = 0
for i in range(1, len(rows)):
    if i < warm:
        continue
    dx = int(rows[i]["vxw"]) - int(rows[i - 1]["hxw"])
    dz = int(rows[i]["vzw"]) - int(rows[i - 1]["hzw"])
    eyelag = max(eyelag, abs(dx), abs(dz))

print("eyelag=%d" % eyelag)
' "$WARMUP_ROWS"
}

# get <stats> <key>
get() {
    printf '%s\n' "$1" | awk -F= -v k="$2" '$1 == k { print $2 }'
}

follow_log="$(mktemp)"
classic_log="$(mktemp)"
zoned_log="$(mktemp)"
trap 'rm -f "$follow_log" "$classic_log" "$zoned_log"' EXIT

arm "$INTERIOR_CUBE" 1 "$WALK_TICKS" "$follow_log"
follow="$(stats "$follow_log")"

rows="$(get "$follow" rows)"
[ "${rows:-0}" -gt 0 ] || fail "no camera trace in the follow arm (is cube $INTERIOR_CUBE an interior?)"
[ "$(get "$follow" ext)" = "0" ] \
    || fail "cube $INTERIOR_CUBE did not stay an interior (ext saw: $(get "$follow" ext)): the exterior camera's own fixtures are the ones above this one"
[ "$(get "$follow" iso)" = "1" ] \
    || fail "cam_interior 1 was not in force ($(get "$follow" iso)): the run proves nothing"
walked="$(get "$follow" walked)"
[ "$walked" -gt 20 ] \
    || fail "the hero barely moved ($walked frames): nothing for the camera to have followed"
tail="$(get "$follow" tail)"
[ "$tail" -gt 60 ] \
    || fail "only $tail traced frames after the warm-up: too few to compare a camera over"
[ "$(get "$follow" sxdistinct)" -ge 2 ] \
    || fail "the view never changed tile with the follow on: it did not follow him"
on_tile="$(get "$follow" ontile)"
[ "$on_tile" -gt $((tail / 2)) ] \
    || fail "the view sat on the hero's tile for only $on_tile of $tail frames with the follow on"
[ "$(get "$follow" flips)" -eq 0 ] \
    || fail "the view stepped past the hero $(get "$follow" flips) times: the step rule is not monotone"
# 4  THE CROSSING IS A PAN, not a 24-pixel cut: the eye spends frames between two tiles, and never
#    moves a whole brick in one of them. The classic arm below is what says these come from the
#    switch -- with it off the eye is always exactly on its tile, which is the cut.
[ "$(get "$follow" lagframes)" -ge 6 ] \
    || fail "the eye sat on its tile for all but $(get "$follow" lagframes) frames: a brick crossing is still a cut"
[ "$(get "$follow" bricksteps)" -eq 0 ] \
    || fail "the eye jumped a whole brick $(get "$follow" bricksteps) times: the sub-tile pan is not reaching the camera"
# 5  ... and the pan is continuous: no stretch of his walk leaves the picture standing still.
[ "$(get "$follow" stillrun)" -le 3 ] \
    || fail "the eye held still for $(get "$follow" stillrun) frames while he walked: it is easing toward his tile, not toward him"
# 5b ... and it is ON him: an eye that eases trails by up to 77 units and slides that back when he
#    stops, which is a jump under a stationary hero.
[ "$(get "$follow" eyelag)" -eq 0 ] \
    || fail "the eye trailed the hero by $(get "$follow" eyelag) units: it is easing toward him instead of sitting on him"

# The control: the same walk and the same room with the switch off.
arm "$INTERIOR_CUBE" 0 "$WALK_TICKS" "$classic_log"
classic="$(stats "$classic_log")"

[ "$(get "$classic" iso)" = "0" ] \
    || fail "cam_interior 0 was not in force in the control ($(get "$classic" iso))"
[ "$(get "$classic" lagframes)" -eq 0 ] \
    || fail "the classic camera left its tile by $(get "$classic" lagframes) frames: the pan comes from something other than the switch"
classic_tail="$(get "$classic" tail)"
[ "$classic_tail" -gt 60 ] \
    || fail "only $classic_tail traced frames in the control: the two arms are not comparable"
[ "$(get "$classic" ext)" = "0" ] \
    || fail "the control left the interior (ext saw: $(get "$classic" ext))"
classic_tile="$(get "$classic" ontile)"
[ "$classic_tile" -le $((classic_tail / 3)) ] \
    || fail "the classic camera was on his tile for $classic_tile of $classic_tail frames: the follow arm's result is not the switch's"

# And the room whose view an authored shot owns.
arm "$ZONED_CUBE" 1 "$ZONED_TICKS" "$zoned_log"
zoned="$(stats "$zoned_log")"

[ "$(get "$zoned" rows)" -gt 0 ] || fail "no camera trace in the zoned arm"
[ "$(get "$zoned" ext)" = "0" ] \
    || fail "cube $ZONED_CUBE is not an interior any more (ext saw: $(get "$zoned" ext))"
[ "$(get "$zoned" zone)" = "1" ] \
    || fail "cube $ZONED_CUBE held no camera zone (zone saw: $(get "$zoned" zone)): the arm does not test what it says"
[ "$(get "$zoned" iso)" = "1" ] \
    || fail "the interior follow was not in force in the zoned arm: it proves nothing about the zone"
[ "$(get "$zoned" walked)" -gt 20 ] \
    || fail "the hero barely moved in the zoned arm ($(get "$zoned" walked) frames): a held view proves nothing"
[ "$(get "$zoned" sxdistinct)" = "1" ] && [ "$(get "$zoned" szdistinct)" = "1" ] \
    || fail "the view moved inside a camera zone (sx $(get "$zoned" sxdistinct), sz $(get "$zoned" szdistinct) tiles): the follow overrode an authored shot"

pass "interior follow: view on the hero's tile $on_tile/$tail frames with no overshoot against $classic_tile/$classic_tail classic, the crossing panned over $(get "$follow" lagframes) frames with no whole-brick step, never more than $(get "$follow" stillrun) frame(s) of it standing still and the eye $(get "$follow" eyelag) units off the hero, and a camera zone still holds its room"
