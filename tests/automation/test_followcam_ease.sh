#!/usr/bin/env bash
# The two eases the Auto camera paces by the game clock.
#
#   cam_ease_ms    the camera's turn behind the hero as a time constant in milliseconds, in
#                  place of the per-frame lerp. The turn takes the time the setting says, lands
#                  on its target, and takes that time whatever an update's length is.
#   cam_target_ms  the point the camera looks at trails the spot ahead of the hero and closes on
#                  it, where otherwise it is pinned to him.
#
# What "whatever an update's length is" needs to be tested against is not the frame rate. The
# simulation runs in fixed steps of FixedTimestep milliseconds (16 by default) and the camera's
# update is inside it, so a 33 ms frame is two updates of 16 and the per-frame lerp already takes
# the same time at 30 frames a second as at 60. It is with the fixed step switched off, where an
# update is a rendered frame long, that the per-frame lerp takes twice as long at 33 ms as at 16.
# That run is the control here: the setting has to hold where the lerp demonstrably does not.
#
# Every claim is checked against the same run with the setting off.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=followcam_ease
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

work="$(mktemp -d -t lba2-followcam-ease-XXXXXX)"
trap 'rm -rf "$work"' EXIT

# Where the corpus save puts the hero, and the heading he has there plus a quarter turn.
SPOT="8989 2701 22932"
TURNED=1198

# turn_run <log> <cam_ease_ms> <frame ms> <sim step ms> -- let the camera settle behind the
# hero, then turn him a quarter turn on the spot and let it come round.
turn_run() {
    ctl_headless --load "$CAM_SAVE" --fixed-dt "$3" --fixed-timestep "$4" \
        --exec "cam_follow 1; cam_hold_angle 1; $CAM_PLAIN; cam_ease_ms $2; camtrace 1" \
        --exec-at 260 "teleport $SPOT $TURNED" \
        --tick 560 --exit > "$1" 2>&1 || fail "turn run failed: exit $?"
    grep -q "\[INFO\] \[cam\]" "$1" || fail "no camera trace in the turn run"
}

# turn_shape <log> <ms per update> -- "<turn> <ms to within 5%> <resting lag> <zone frames>":
# how far the camera had to come, how long it took to get within a twentieth of that, how far
# from its target it was left, and whether a camera zone had the view at any point.
turn_shape() {
    cam_tsv "$1" | awk -v per="$2" '
        NR == 1 { for (i = 1; i <= NF; i++) col[$i] = i; next }
        { d = $col["target"] - $col["beta"]
          if (d > 2048) d -= 4096
          if (d < -2048) d += 4096
          if (d < 0) d = -d
          zones += $col["zone"] }
        !turned && NR > 2 && $col["heroBeta"] != hero { turned = 1; n = 0 }
        { hero = $col["heroBeta"] }
        turned {
            if (n < 3 && d > peak) peak = d
            if (n >= 3 && !within && d <= peak / 20) within = n
            n++
            rest = d
        }
        END { print peak + 0, within * per, rest + 0, zones + 0 }'
}

# near <a> <b> <percent> -- whether a is within that many percent of b.
near() {
    local d=$(( $1 - $2 ))
    [ "$d" -lt 0 ] && d=$(( -d ))
    [ $(( d * 100 )) -le $(( $2 * $3 )) ]
}

turn_run "$work/lerp.log" 0 16 16
turn_run "$work/ease.log" 300 16 16
turn_run "$work/pace.log" 600 16 16
turn_run "$work/lerp33.log" 0 33 0
turn_run "$work/ease33.log" 300 33 0
turn_run "$work/ease7.log" 300 7 16

read -r lerp_turn lerp_ms lerp_rest lerp_zone <<< "$(turn_shape "$work/lerp.log" 16)"
read -r ease_turn ease_ms ease_rest ease_zone <<< "$(turn_shape "$work/ease.log" 16)"
read -r pace_turn pace_ms pace_rest pace_zone <<< "$(turn_shape "$work/pace.log" 16)"
read -r lerp33_turn lerp33_ms lerp33_rest lerp33_zone <<< "$(turn_shape "$work/lerp33.log" 33)"
read -r ease33_turn ease33_ms ease33_rest ease33_zone <<< "$(turn_shape "$work/ease33.log" 33)"
read -r ease7_turn ease7_ms ease7_rest ease7_zone <<< "$(turn_shape "$work/ease7.log" 16)"

for t in "$lerp_turn" "$ease_turn" "$pace_turn" "$lerp33_turn" "$ease33_turn"; do
    [ "$t" -ge 900 ] && [ "$t" -le 1024 ] \
        || fail "the camera had $t units to come round, not a quarter turn: the hero is not where this fixture turns him"
done
[ $(( lerp_zone + ease_zone + pace_zone + lerp33_zone + ease33_zone + ease7_zone )) -eq 0 ] \
    || fail "a camera zone had the view during a turn: this spot no longer leaves the Auto camera alone"

# The control. The lerp takes about 1.7 s for the turn, stops short of its target, and with the
# fixed step off takes twice as long at 33 ms a frame. Without these there is nothing for the
# setting to change.
[ "$lerp_ms" -ge 1500 ] && [ "$lerp_ms" -le 1900 ] \
    || fail "the per-frame lerp brought the turn within 5% in $lerp_ms ms, not about 1700: the control has moved"
[ "$lerp_rest" -ge 1 ] && [ "$lerp_rest" -le 10 ] \
    || fail "the per-frame lerp came to rest $lerp_rest units from its target, not inside its snap threshold and short of it"
[ "$lerp33_ms" -ge $(( lerp_ms * 18 / 10 )) ] \
    || fail "with the fixed step off the per-frame lerp took $lerp33_ms ms at 33 ms a frame against $lerp_ms at 16: it no longer depends on the frame, so the next check proves nothing"

# The setting: the time it says, on the target, at either update length.
[ "$ease_ms" -ge 800 ] && [ "$ease_ms" -le 1000 ] \
    || fail "with cam_ease_ms 300 the turn came within 5% in $ease_ms ms, not the three time constants an ease takes"
[ "$ease_rest" -eq 0 ] \
    || fail "with cam_ease_ms 300 the camera came to rest $ease_rest units off its target"
near "$ease33_ms" "$ease_ms" 10 \
    || fail "with cam_ease_ms 300 the turn took $ease33_ms ms at 33 ms an update against $ease_ms at 16: it is still paced by the update, not the clock"
[ "$ease33_rest" -eq 0 ] \
    || fail "with cam_ease_ms 300 at 33 ms an update the camera came to rest $ease33_rest units off its target"

# Above 60 fps with the fixed step on, most rendered frames skip the simulation and so the
# update. Those frames are not a gap in the ease's clock: a row is still a 16 ms step. A gap
# there stalls the ease before the turn as well, so the camera is not settled behind him and the
# turn it has to make is not a quarter: that is the same fault, and it is reported as one.
[ "$ease7_turn" -ge 900 ] && [ "$ease7_turn" -le 1024 ] \
    || fail "with cam_ease_ms 300 at 7 ms frames the camera had $ease7_turn units to come round where the other runs had a quarter turn: a frame that skips the simulation stalls the ease"
near "$ease7_ms" "$ease_ms" 10 \
    || fail "with cam_ease_ms 300 at 7 ms frames the turn took $ease7_ms ms against $ease_ms at 16: a frame that skips the simulation stalls the ease"
[ "$ease7_rest" -eq 0 ] \
    || fail "with cam_ease_ms 300 at 7 ms frames the camera came to rest $ease7_rest units off its target"

# 600 is documented as the pace the lerp has at the default step.
near "$pace_ms" "$lerp_ms" 10 \
    || fail "with cam_ease_ms 600 the turn took $pace_ms ms against the lerp's $lerp_ms: the documented equivalent no longer is one"

# --- The look-at point ---

# walk_run <log> <cam_target_ms>
walk_run() {
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_follow 1; cam_hold_angle 1; $CAM_PLAIN; cam_target_ms $2; camtrace 1; godmode 1" \
        --exec-at 120 "input up 150" \
        --tick 420 --exit > "$1" 2>&1 || fail "walk run failed: exit $?"
}

# walk_shape <log> -- "<frames walked> <furthest trailed while walking> <trail at the end>"
walk_shape() {
    cam_tsv "$1" | awk '
        NR == 1 { for (i = 1; i <= NF; i++) col[$i] = i; next }
        $col["moving"] == 1 { walked++; if ($col["tlag"] > far) far = $col["tlag"] }
        { last = $col["tlag"] }
        END { print walked + 0, far + 0, last + 0 }'
}

walk_run "$work/pinned.log" 0
walk_run "$work/trail.log" 300

read -r pin_walked pin_far pin_last <<< "$(walk_shape "$work/pinned.log")"
read -r trail_walked trail_far trail_last <<< "$(walk_shape "$work/trail.log")"

[ "$pin_walked" -ge 100 ] && [ "$trail_walked" -ge 100 ] \
    || fail "the hero walked for $pin_walked and $trail_walked frames: not far enough to trail anything"
[ "$pin_far" -eq 0 ] && [ "$pin_last" -eq 0 ] \
    || fail "with cam_target_ms 0 the look-at point trailed by $pin_far units: it is no longer pinned to the hero"
[ "$trail_far" -ge 100 ] \
    || fail "with cam_target_ms 300 the look-at point trailed by at most $trail_far units while the hero walked"
[ "$trail_last" -eq 0 ] \
    || fail "with cam_target_ms 300 the look-at point was still $trail_last units behind long after the hero stopped"

# Walking across the seam between two outdoor scenes renumbers the world under the hero. What the
# look-at point trails by has to come through unchanged: kept as a place, it would be a scene's
# width away on the far side.
#
# cam_zone_skip is on for this run. A shot that latches is waiting on the far side of this seam
# and, fired, takes the view for a frame, which ends the trail for a reason that has nothing to
# do with the seam.
ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
    --exec "cam_follow 1; cam_hold_angle 1; $CAM_PLAIN; cam_zone_skip 1; cam_target_ms 300; camtrace 1; cubetrace 1; godmode 1" \
    --exec-at 20 "cube 42" \
    --exec-at 80 "teleport 5000 250 19200 3072" \
    --exec-at 120 "input up 400" \
    --tick 420 --exit > "$work/seam.log" 2>&1 || fail "seam run failed: exit $?"
grep -q "\[cube\] island [0-9]*: 42 -> 43" "$work/seam.log" || fail "the hero never walked across the seam from cube 42 to 43"

# The trail on the last update before the scene changed and on the first update after it, and
# whether a camera zone had the view on any frame from the one to the other. A frame the update
# did not run on traces a dt of 0.
read -r seam_before seam_after seam_seen seam_zone <<< "$(awk '
    function field(rec, name) {
        if (!match(rec, name " +[0-9]+")) return -1
        return substr(rec, RSTART + length(name), RLENGTH - length(name)) + 0
    }
    /\[cube\] island [0-9]*: 42 -> 43/ { crossed = 1 }
    { at = index($0, "[INFO] [cam]")
      if (!at || seen) next
      rec = substr($0, at)
      if (field(rec, " tlag") < 0) next
      if (!crossed) { before = field(rec, " tlag"); next }
      zone += field(rec, " zone")
      if (field(rec, " dt") > 0) { after = field(rec, " tlag"); seen = 1 } }
    END { print before + 0, after + 0, seen + 0, zone + 0 }' "$work/seam.log")"

[ "$seam_seen" -eq 1 ] || fail "the Auto camera never updated after the seam"
[ "$seam_zone" -eq 0 ] \
    || fail "a camera zone took the view at the seam: the crossing no longer leaves the Auto camera alone"
[ "$seam_before" -ge 100 ] \
    || fail "the look-at point trailed by only $seam_before units at the seam: nothing for the crossing to lose"
[ "$seam_after" -eq "$seam_before" ] \
    || fail "the look-at point trailed by $seam_before units before the seam and $seam_after after it"

pass "turn within 5%: lerp $lerp_ms ms resting $lerp_rest short ($lerp33_ms ms at 33 ms updates), cam_ease_ms 300 $ease_ms ms on target ($ease33_ms ms at 33 with the step off, $ease7_ms at 7 ms frames), 600 $pace_ms ms; look-at point trails $trail_far units walking and closes to 0, $seam_before kept across the seam"
