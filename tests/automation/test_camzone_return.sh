#!/usr/bin/env bash
# A scripted shot borrows the Auto camera: with cam_zone_return on, the yaw and pitch it took are given back when the hero leaves its box.
#
# A camera zone re-aims the view to an authored shot. The Auto camera has always kept what the
# shot left behind: the yaw as an offset from the hero's facing, and the pitch as authored, until
# the player moves the camera or the scene changes. A player who never touches the camera has no
# reason to expect that, and on a touch screen has nothing to undo it with.
#
# Two prototype settings, both off by default, treat the shot as a loan instead:
#
#   cam_zone_return  when the hero leaves the box, ease yaw and pitch back to what the Auto camera
#                    had before the shot. An axis the player set by hand meanwhile is theirs.
#   cam_zone_anchor  inside the box of a zone that fires once, hold the shot's world yaw as the
#                    classic camera does, rather than carrying it round as an offset.
#
# What is owed outlives a visit to an interior, since a door leaves the pan and the pitch alone.
#
# Each claim is checked against the same run with the setting off, because every one of them
# could otherwise pass by its setup never happening: a shot that never fired leaves nothing to
# give back, and a camera that was never moved is trivially where it started.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=camzone_return
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

# Asked through varlist rather than by naming the setting: a bare name toggles a boolean cvar, and
# the toggle would be written to the cfg on exit.
case "$(ctl_headless --exec 'varlist' --tick 2 --exit 2>/dev/null)" in
    *cam_zone_return*) ;;
    *) skip "binary has no cam_zone_return setting" ;;
esac

work="$(mktemp -d -t lba2-camzone-return-XXXXXX)"
trap 'rm -rf "$work"' EXIT

# Cube 90. Zone 31 holds the view on every frame the hero is inside it (ZONE_OBLIGATOIRE); zone 21
# fires once on entry and latches. NEUTRAL is in no enabled camera zone.
NEUTRAL="18000 3900 8000"
FORCED="13000 3900 22000"
LATCH="19000 3900 4600"

# shot_run <name> <cvars> <inside-xyz> [extra engine args...]
#
# Settle behind the hero outside every zone, step into the box, turn about a quarter turn inside
# it, then leave and stand still. A settings folder per run: the cfg written at exit carries every
# cvar into the next boot, and cube 90 starts the hero inside a camera zone, so a run would
# otherwise begin with a loan the previous run's settings took out.
shot_run() {
    local name="$1" cvars="$2" inside="$3"
    shift 3
    LBA2_USER_DIR="$work/user-$name"
    export LBA2_USER_DIR
    mkdir -p "$LBA2_USER_DIR"
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cube 90" \
        --exec-at 15 "camtrace 1; cam_follow 1; cam_hold_angle 1; $cvars" \
        --exec-at 20 "teleport $NEUTRAL" \
        --exec-at 150 "teleport $inside" \
        --exec-at 200 "input left 40" \
        --exec-at 300 "teleport $NEUTRAL" \
        "$@" --tick 620 --exit > "$work/$name.log" 2>&1 || fail "run $name failed: exit $?"
    cam_tsv "$work/$name.log" > "$work/$name.tsv"
}

# field <name> <column> <which>: one value from a run's table.
#   before  the last frame before the shot fired     shot  the frame it fired
#   last    the final frame
field() {
    awk -v want="$2" -v which="$3" '
        NR == 1 { for (i = 1; i <= NF; i++) { if ($i == want) col = i; if ($i == "zone") zc = i }; next }
        { if (!fired && $zc == 1) { fired = 1; atshot = $col; before = prev } prev = $col }
        END { print (which == "before") ? before : (which == "shot") ? atshot : prev }' "$work/$1.tsv"
}

# ever <name> <column> <value>: how many frames a column held a value.
ever() {
    awk -v want="$2" -v val="$3" '
        NR == 1 { for (i = 1; i <= NF; i++) if ($i == want) col = i; next }
        $col == val { n++ } END { print n + 0 }' "$work/$1.tsv"
}

# --- A zone that holds the view, and what is left when the hero walks out of it ---------------
shot_run forced_off    "cam_zone_return 0; cam_zone_anchor 0" "$FORCED"
shot_run forced_return "cam_zone_return 1; cam_zone_anchor 0" "$FORCED"

[ "$(ever forced_off zone 1)" -gt 50 ] \
    || fail "the forced zone never held the view: the teleport missed the box"

pan0="$(field forced_off add before)"
pitch0="$(field forced_off alpha before)"
[ "$(field forced_off alpha shot)" != "$pitch0" ] \
    || fail "the shot authored the pitch the camera already had ($pitch0): nothing to give back"

# Off is the shipped behaviour, and it has to stay exactly that: the leftovers are kept.
[ "$(field forced_off add last)" != "$pan0" ] \
    || fail "with the setting off the yaw came back to $pan0 on its own: the control proves nothing"
[ "$(field forced_off alpha last)" != "$pitch0" ] \
    || fail "with the setting off the pitch came back to $pitch0 on its own: the control proves nothing"
[ "$(awk 'NR > 1 && $NF != 0' "$work/forced_off.tsv" | wc -l | tr -d ' ')" = 0 ] \
    || fail "a loan was taken out with both settings off"

[ "$(ever forced_return loan 7)" -ge 1 ] \
    || fail "leaving the box never started a return"
[ "$(field forced_return add last)" = "$pan0" ] \
    || fail "the yaw offset was not given back: $(field forced_return add last), lent at $pan0"
[ "$(field forced_return alpha last)" = "$pitch0" ] \
    || fail "the pitch was not given back: $(field forced_return alpha last), lent at $pitch0"
[ "$(field forced_return loan last)" = 0 ] \
    || fail "the loan was still open at the end of the run"

# --- What the player set by hand is theirs ----------------------------------------------------
# An orbit while the latching shot has the camera. The pitch, which they did not touch, still
# goes back.
shot_run took_yaw "cam_zone_return 1; cam_zone_anchor 0" "$LATCH" --exec-at 170 "camnudge 20 0 10"

[ "$(ever took_yaw zone 1)" -ge 1 ] || fail "the latching zone never fired"
[ "$(field took_yaw add last)" != "$(field took_yaw add before)" ] \
    || fail "the yaw the player set was taken back to $(field took_yaw add before)"
[ "$(field took_yaw alpha last)" = "$(field took_yaw alpha before)" ] \
    || fail "the pitch was not given back after an orbit: $(field took_yaw alpha last)"

# --- Inside a latching box, the shot's world angle -------------------------------------------
shot_run latch_off    "cam_zone_return 0; cam_zone_anchor 0" "$LATCH"
shot_run latch_anchor "cam_zone_return 1; cam_zone_anchor 1" "$LATCH"

# Frames, from the one the shot fired on and for as long as the hero stood in its box, on which
# the camera was not at the angle the shot authored. He turns under it in both runs. Counted from
# the shot because the scene starts him inside a different zone's box, which is not this visit.
off_shot() {
    awk -v shot="$(field "$1" beta shot)" '
        NR == 1 { for (i = 1; i <= NF; i++) { if ($i == "beta") b = i; if ($i == "inshot") s = i; if ($i == "zone") z = i }; next }
        $z == 1 { fired = 1 }
        fired && $s == 1 && $b != shot { n++ } END { print n + 0 }' "$work/$1.tsv"
}

[ "$(off_shot latch_off)" -gt 0 ] \
    || fail "the camera kept the shot's angle with the anchor off: the hero never turned, so the anchor run proves nothing"
[ "$(off_shot latch_anchor)" = 0 ] \
    || fail "the anchor let the camera leave the authored angle on $(off_shot latch_anchor) frame(s) inside the box"
[ "$(field latch_anchor add last)" = "$(field latch_anchor add before)" ] \
    || fail "after an anchored shot the yaw offset was not given back: $(field latch_anchor add last)"
[ "$(field latch_anchor alpha last)" = "$(field latch_anchor alpha before)" ] \
    || fail "after an anchored shot the pitch was not given back: $(field latch_anchor alpha last)"

# --- A visit to an interior does not end the loan ---------------------------------------------
# Cube 42: camera zone 6 is a latching shot over the door of interior cube 3. Take the shot, walk
# in through the door, come back out through it into the same box, then leave the box. A door
# changes scene through CameraCenter(0), which leaves the pan and the pitch as they were, so the
# shot's leftovers come back out with the hero. Forgetting what was owed on the way in made the
# zone lend afresh from its own leftovers on the way out, and the return then went nowhere.
door_run() { # <name> <cvars>
    LBA2_USER_DIR="$work/user-$1"
    export LBA2_USER_DIR
    mkdir -p "$LBA2_USER_DIR"
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cube 42" \
        --exec-at 15 "camtrace 1; cam_follow 1; cam_hold_angle 1; $2" \
        --exec-at 20 "teleport 17852 250 9500 2048" \
        --exec-at 150 "teleport 17852 250 7900 2048" \
        --exec-at 170 "input up 80" \
        --exec-at 320 "teleport 7100 1024 15600" \
        --exec-at 440 "teleport 17852 250 9500 2048" \
        --tick 720 --exit > "$work/$1.log" 2>&1 || fail "run $1 failed: exit $?"
    cam_tsv "$work/$1.log" > "$work/$1.tsv"
}

door_run door_off    "cam_zone_return 0; cam_zone_anchor 0"
door_run door_return "cam_zone_return 1; cam_zone_anchor 0"

[ "$(ever door_return ext 0)" -gt 50 ] \
    || fail "the hero never got indoors: the walk missed the door, so the visit proves nothing"
[ "$(ever door_return zone 1)" -ge 2 ] \
    || fail "the shot over the door fired $(ever door_return zone 1) time(s), not on the way in and again on the way out"
[ "$(field door_off add last)" != "$(field door_off add before)" ] \
    || fail "with the setting off the yaw was back at $(field door_off add before) after the visit: the control proves nothing"
[ "$(field door_return add last)" = "$(field door_return add before)" ] \
    || fail "after a visit indoors the yaw offset was not given back: $(field door_return add last), lent at $(field door_return add before)"
[ "$(field door_return alpha last)" = "$(field door_return alpha before)" ] \
    || fail "after a visit indoors the pitch was not given back: $(field door_return alpha last), lent at $(field door_return alpha before)"

# --- Coming out of a building, the yaw goes home ----------------------------------------------
# The same door, but the player orbits while the shot has the camera and then goes inside. The
# pan is an offset from the hero's facing and he comes out facing the other way, so given back as
# it stood it leaves the camera in front of him. Out of a building what is owed is "behind him".
# An orbit followed by a walk out of the box with no building in between keeps its angle, which
# is the took_yaw run above: the two differ by the visit and nothing else.
ctl_door_orbit() { # <name>
    LBA2_USER_DIR="$work/user-$1"
    export LBA2_USER_DIR
    mkdir -p "$LBA2_USER_DIR"
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cube 42" \
        --exec-at 15 "camtrace 1; cam_follow 1; cam_hold_angle 1; cam_zone_return 1; cam_zone_anchor 0" \
        --exec-at 20 "teleport 17852 250 9500 2048" \
        --exec-at 150 "teleport 17852 250 8150 2048" \
        --exec-at 170 "camnudge 30 0 20" \
        --exec-at 260 "input up 120" \
        --exec-at 440 "teleport 7100 1024 15600" \
        --exec-at 540 "teleport 17852 250 9500 2048" \
        --tick 860 --exit > "$work/$1.log" 2>&1 || fail "run $1 failed: exit $?"
    cam_tsv "$work/$1.log" > "$work/$1.tsv"
}

ctl_door_orbit door_orbit

[ "$(ever door_orbit orbit 1)" -ge 5 ] \
    || fail "the orbit during the shot never happened, so there was no pan of the player's to carry indoors"
[ "$(ever door_orbit loan 2)" -ge 1 ] \
    || fail "the orbit did not make the yaw the player's: the run is the plain door visit again"
[ "$(ever door_orbit ext 0)" -gt 50 ] \
    || fail "the hero never got indoors after orbiting"
[ "$(field door_orbit add last)" = "$(field door_orbit add before)" ] \
    || fail "out of the building the yaw went back to the orbit's angle, $(field door_orbit add last), not behind the hero at $(field door_orbit add before)"
[ "$(field door_orbit alpha last)" = "$(field door_orbit alpha before)" ] \
    || fail "out of the building the pitch was not given back: $(field door_orbit alpha last)"

pass "left to itself the shot kept yaw $(field forced_off add last) and pitch $(field forced_off alpha last); returned, both came back to $pan0 and $pitch0; the player's orbit stayed at $(field took_yaw add last); the anchor held $(field latch_anchor beta shot) for the whole visit; a trip indoors and back still returned $(field door_off add last) to $(field door_return add last), and after an orbit too"
