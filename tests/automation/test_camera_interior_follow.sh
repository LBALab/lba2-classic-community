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
#   5c AND THE PAN IS WHOLE PIXELS: the camera itself stays on the tile's corner and the pan is a shift
#      of the projection centre that the brick lattice also adds (FOLLOWCAM.CPP's FollowCamIsoCentre).
#      A camera moved between tiles shifts the same picture on paper, but each projected point then
#      rounds its own fraction of a pixel together with the eye's while the lattice rounds the eye's
#      alone, so a door or sprite standing still drifts a pixel against the wall behind it as the view
#      pans (5 still objects in this walk, measured with a probe of each object's screen position
#      against the world origin's). `camt` is 1 on every traced frame the camera is on that corner.
#   7  A HAND-OVER IS A BLEND, NOT A CUT. When a camera zone takes the view or gives it back, the
#      view the frame shows would jump to the new owner's at once: 206 pixels (x plus y) in one
#      frame walking into the zone below, and about 100, 60 and 57 in three frames walking out (a
#      play test found 55 when a shot in Twinsen's house ended). The follow carries the picture
#      there over cam_shot_blend_ms, the exterior blend's setting: at most 13 pixels a frame on the
#      same walk. The cut arm
#      (cam_shot_blend_ms 0) is what says the blend is what removes the jump.
#      The same holds from one zone's shot to the next with nobody else owning the view between:
#      crossing from zone 9 into zone 7 of cube 3 is a 162-pixel cut in the classic camera and with
#      cam_shot_blend_ms 0, and at most 7 pixels a frame with the blend.
#   6  A CAMERA ZONE STILL WINS, in an interior and with the switch on. Every interior camera that is
#      not this option's is a zone's (`SetZoneCamera` re-places `Start*Cube` every frame while the hero
#      is in the box), and a following camera that overrode one would be a room whose authored shot
#      snapped back the frame after the script set it. The third arm is a room where a zone holds the
#      view for the whole run: the camera's tile must not move at all, wherever the hero walks.
#
# Every number below is measured on this tree against the corpus save (both arms of the same walk,
# cube 143, and the zoned room 154), not chosen: 93 of 95 traced frames on his tile with the switch on
# against 2 of 95 with it off, a single camera tile for the whole zoned run, at most one frame of his
# walk with the picture standing still (15 for an eye eased toward the tile instead), and no frame in
# which the eye is anywhere but on him (39 units behind for an eye eased toward his position instead).
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
# Tier: medium
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
# The hand-over walk: cube 3, back into a camera zone and forward out of it again.
HANDOVER_CUBE=3
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

# handover_arm <cam_shot_blend_ms> <log> -- into the zone and out again, the follow on.
handover_arm() {
    local ms="$1" log="$2"

    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_interior 1; cam_shot_blend_ms $ms; camtrace 1; cube $HANDOVER_CUBE" \
        --exec-at 20 "input down 110" \
        --exec-at 170 "input up 110" \
        --tick 330 --exit > "$log" 2>&1 \
        || fail "hand-over run in cube $HANDOVER_CUBE with cam_shot_blend_ms $ms failed: exit $? (see $log)"
}

# zone2zone_arm <cam_shot_blend_ms> <log> -- placed in zone 9 and walked across into zone 7.
zone2zone_arm() {
    local ms="$1" log="$2"

    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_interior 1; cam_shot_blend_ms $ms; camtrace 1; cube $HANDOVER_CUBE" \
        --exec-at 10 "teleport 7500 1024 5800 2600" \
        --exec-at 100 "input up 40" \
        --tick 200 --exit > "$log" 2>&1 \
        || fail "zone-to-zone run in cube $HANDOVER_CUBE with cam_shot_blend_ms $ms failed: exit $? (see $log)"
}

# handover_stats <log> -- zone changes seen, blend frames, and the largest move of the shown view in
# one frame from the first zone change on, in pixels (the lattice's own origin: tile and pan).
handover_stats() {
    cam_tsv "$1" | python3 -c '
import sys

hdr = None
rows = []
for line in sys.stdin:
    f = line.rstrip("\n").split("\t")
    if hdr is None:
        hdr = f
        continue
    if len(f) == len(hdr):
        r = {k: int(v) for k, v in zip(hdr, f)}
        if r["ext"] == 0:  # the save starts outdoors; the walk is the interior after the warp
            rows.append(r)

def view(r):
    return (-24 * (r["tx"] - r["tz"]) + r["panx"], -12 * (r["tx"] + r["tz"]) + 15 * r["ty"] + r["pany"])

def tile(r):
    return (r["tx"], r["ty"], r["tz"])

# A change is the zone flag flipping (01, 10), or one zone shot replacing another (11).
changes = []
first = len(rows)
first11 = len(rows)
for i in range(1, len(rows)):
    a, b = rows[i - 1], rows[i]
    if a["zone"] != b["zone"] or (a["zone"] and b["zone"] and tile(a) != tile(b)):
        changes.append("%d%d" % (a["zone"], b["zone"]))
        first = min(first, i)
        if a["zone"] and b["zone"]:
            first11 = min(first11, i)
jump = 0
for i in range(max(first, 1), len(rows)):
    a, b = view(rows[i - 1]), view(rows[i])
    jump = max(jump, abs(b[0] - a[0]) + abs(b[1] - a[1]))
print("changes=%s" % ",".join(changes))
print("rows=%d" % len(rows))
print("blendrows=%d" % sum(1 for r in rows if r["ibl"] > 0))
print("jump=%d" % jump)
# From the first zone-to-zone change on: the zone-to-zone arm is placed by a teleport, which moves the
# view itself and is not what that arm measures.
jump11 = 0
for i in range(max(first11, 1), len(rows)):
    a, b = view(rows[i - 1]), view(rows[i])
    jump11 = max(jump11, abs(b[0] - a[0]) + abs(b[1] - a[1]))
print("jump11=%d" % jump11)
'
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
print("camtoff=%d" % sum(1 for r in tail if r["camt"] != "1"))
' "$WARMUP_ROWS"
}

# get <stats> <key>
get() {
    printf '%s\n' "$1" | awk -F= -v k="$2" '$1 == k { print $2 }'
}

follow_log="$(mktemp)"
classic_log="$(mktemp)"
zoned_log="$(mktemp)"
blend_log="$(mktemp)"
cut_log="$(mktemp)"
trap 'rm -f "$follow_log" "$classic_log" "$zoned_log" "$blend_log" "$cut_log"' EXIT

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
# 5c ... and the camera is on its tile's corner, so the pan is whole pixels and nothing wobbles.
[ "$(get "$follow" camtoff)" -eq 0 ] \
    || fail "the camera was off its tile's corner on $(get "$follow" camtoff) frames: projected objects round differently from the bricks and wobble a pixel against them"

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

# 7  The hand-over: the same walk into a zone and out again, blended and cut.
handover_arm 600 "$blend_log"
blend="$(handover_stats "$blend_log")"
handover_arm 0 "$cut_log"
cut="$(handover_stats "$cut_log")"

[ "$(get "$blend" rows)" -gt 200 ] || fail "only $(get "$blend" rows) interior frames in the hand-over run: is cube $HANDOVER_CUBE still an interior?"
[ "$(get "$blend" changes)" = "01,10" ] && [ "$(get "$cut" changes)" = "01,10" ] \
    || fail "the hand-over walk did not enter and leave one zone (blend: $(get "$blend" changes), cut: $(get "$cut" changes)): the arm tests nothing"
[ "$(get "$cut" blendrows)" -eq 0 ] \
    || fail "a blend ran with cam_shot_blend_ms 0 ($(get "$cut" blendrows) frames)"
[ "$(get "$cut" jump)" -ge 60 ] \
    || fail "the cut arm moved the view only $(get "$cut" jump) px in a frame: the walk no longer has a jump for the blend to remove"
[ "$(get "$blend" blendrows)" -ge 20 ] \
    || fail "the hand-over blended over only $(get "$blend" blendrows) frames"
[ "$(get "$blend" jump)" -le 20 ] \
    || fail "the view jumped $(get "$blend" jump) px in a frame across a hand-over with the blend on (cut: $(get "$cut" jump))"

# ... and from one zone's shot to the next.
zone2zone_arm 600 "$blend_log"
zblend="$(handover_stats "$blend_log")"
zone2zone_arm 0 "$cut_log"
zcut="$(handover_stats "$cut_log")"

case ",$(get "$zblend" changes),$(get "$zcut" changes)," in
    *,11,*,11,*) ;;
    *) fail "the zone-to-zone walk did not cross from one shot to another (blend: $(get "$zblend" changes), cut: $(get "$zcut" changes))" ;;
esac
[ "$(get "$zcut" jump11)" -ge 60 ] \
    || fail "the zone-to-zone cut arm moved the view only $(get "$zcut" jump11) px in a frame: nothing for the blend to remove"
[ "$(get "$zblend" jump11)" -le 20 ] \
    || fail "the view jumped $(get "$zblend" jump11) px in a frame from one zone's shot to the next with the blend on (cut: $(get "$zcut" jump11))"

pass "interior follow: view on the hero's tile $on_tile/$tail frames with no overshoot against $classic_tile/$classic_tail classic, the crossing panned over $(get "$follow" lagframes) frames with no whole-brick step, never more than $(get "$follow" stillrun) frame(s) of it standing still and the eye $(get "$follow" eyelag) units off the hero, a camera zone still holds its room, and a hand-over moves the view at most $(get "$blend" jump) px a frame against $(get "$cut" jump) cut ($(get "$zblend" jump11) against $(get "$zcut" jump11) from zone to zone)"
