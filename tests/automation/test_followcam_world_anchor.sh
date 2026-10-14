#!/usr/bin/env bash
# cam_world_anchor: the Auto camera holds a direction in the world instead of an offset from the
# hero's facing.
#
#   - The hero turns, on the spot and while walking, and the camera's target does not move.
#   - The player still turns it: an orbit moves the target, which then holds the new direction.
#   - The drift back behind the hero (cam_hold_angle 0) stands down.
#   - A scripted shot that holds the view gives back the world direction the camera had before
#     it, not the offset, which after a turn inside the box is somewhere else.
#
# Each claim is checked against the same run with the setting off, where the target follows him.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=followcam_world_anchor
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

# Asked through varlist rather than by naming the setting: a bare name toggles a boolean cvar, and
# the toggle would be written to the cfg on exit.
case "$(ctl_headless --exec 'varlist' --tick 2 --exit 2>/dev/null)" in
    *cam_world_anchor*) ;;
    *) skip "binary has no cam_world_anchor setting" ;;
esac

work="$(mktemp -d -t lba2-followcam-world-anchor-XXXXXX)"
trap 'rm -rf "$work"' EXIT

# run <name> [engine args...] -- a settings folder per run, so no run boots with the last one's
# settings.
run() {
    local name="$1"
    shift
    LBA2_USER_DIR="$work/user-$name"
    export LBA2_USER_DIR
    mkdir -p "$LBA2_USER_DIR"
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 "$@" --exit > "$work/$name.log" 2>&1 \
        || fail "run $name failed: exit $?"
    cam_tsv "$work/$name.log" > "$work/$name.tsv"
}

# rows <name> <from> <to> <column...> -- those columns over a range of traced frames.
rows() {
    local name="$1" from="$2" to="$3"
    shift 3
    awk -v from="$from" -v to="$to" -v want="$*" '
        NR == 1 { n = split(want, w, " "); for (i = 1; i <= NF; i++) col[$i] = i; next }
        NR - 1 >= from && NR - 1 <= to {
            line = ""; for (i = 1; i <= n; i++) line = line (i > 1 ? " " : "") $col[w[i]]; print line }' \
        "$work/$name.tsv"
}

# distinct <name> <from> <to> <column> -- how many values a column took over a range.
distinct() { rows "$@" | sort -u | wc -l | tr -d ' '; }

# span <name> <from> <to> <column> -- how far a column moved over a range, the short way round.
span() {
    rows "$@" | awk 'NR == 1 { a = $1 } { b = $1 }
        END { d = b - a; if (d > 2048) d -= 4096; if (d < -2048) d += 4096; print (d < 0) ? -d : d }'
}

# travel <name> <from> <to> <column> -- how far a column moved in all over a range, every step
# taken the short way round: a hero who turns three quarters of a turn has travelled that far,
# though he ends a quarter turn from where he started.
travel() {
    rows "$@" | awk 'NR > 1 { d = $1 - p; if (d > 2048) d -= 4096; if (d < -2048) d += 4096; t += (d < 0) ? -d : d }
        { p = $1 } END { print t + 0 }'
}

# --- The hero turns under the camera -----------------------------------------------------------
# Settle behind him, turn him a quarter turn on the spot (a teleport with a new heading), walk
# turning left, orbit, then walk turning right. Rows are trace frames; the orbit lands at 560.
turn_run() { # <name> <cam_world_anchor>
    run "$1" --exec "cam_follow 1; cam_hold_angle 1; $CAM_PLAIN; cam_world_anchor $2; camtrace 1; godmode 1" \
        --exec-at 260 "teleport 8989 2701 22932 1198" \
        --exec-at 420 "input up+left 120" \
        --exec-at 560 "camnudge 20 0 10" \
        --exec-at 600 "input up+right 60" \
        --tick 760
}
turn_run turn_off 0
turn_run turn_on 1

[ "$(travel turn_on 200 550 heroBeta)" -ge 1500 ] \
    || fail "the hero turned only $(travel turn_on 200 550 heroBeta) units before the orbit: nothing for the anchor to hold against"
[ "$(span turn_off 200 400 target)" -ge 900 ] \
    || fail "with the setting off the target moved $(span turn_off 200 400 target) units over a quarter turn of the hero: the control no longer follows him"
[ "$(distinct turn_on 200 550 target)" -eq 1 ] \
    || fail "with cam_world_anchor 1 the target took $(distinct turn_on 200 550 target) values while the hero turned under it"
[ "$(span turn_on 550 590 target)" -ge 150 ] \
    || fail "with cam_world_anchor 1 an orbit moved the target only $(span turn_on 550 590 target) units: the player can no longer turn the camera"
[ "$(travel turn_on 600 750 heroBeta)" -ge 300 ] \
    || fail "the hero turned only $(travel turn_on 600 750 heroBeta) units after the orbit"
[ "$(distinct turn_on 600 750 target)" -eq 1 ] \
    || fail "with cam_world_anchor 1 the target moved after the orbit while the hero turned: the new direction is not held"

# --- The drift back behind the hero ------------------------------------------------------------
# cam_hold_angle 0 drifts a pan home while the hero walks. Pan, then walk straight on.
DRIFT=(--exec-at 120 "camnudge 20 0 10" --exec-at 160 "input up 200" --tick 400)
run drift_off --exec "cam_follow 1; cam_hold_angle 0; $CAM_PLAIN; cam_world_anchor 0; camtrace 1; godmode 1" "${DRIFT[@]}"
run drift_on --exec "cam_follow 1; cam_hold_angle 0; $CAM_PLAIN; cam_world_anchor 1; camtrace 1; godmode 1" "${DRIFT[@]}"

[ "$(distinct drift_on 140 390 heroBeta)" -eq 1 ] \
    || fail "the hero turned while walking straight: the drift run measures turning, not drift"
[ "$(span drift_off 140 390 target)" -ge 100 ] \
    || fail "with the setting off and cam_hold_angle 0 the pan drifted only $(span drift_off 140 390 target) units: the control no longer drifts"
[ "$(distinct drift_on 140 390 target)" -eq 1 ] \
    || fail "with cam_world_anchor 1 the target drifted while the hero walked straight on"

# --- A shot that holds the view, and what it gives back ----------------------------------------
# Cube 90, zone 31 holds the view while the hero is in its box. He turns inside it and walks out.
# The tilt is what makes the pitch on screen before the shot the camera's own (see
# test_camzone_return.sh, where this route comes from).
SHOT_CVARS="cam_follow 1; cam_hold_angle 1; camtrace 1; $CAM_PLAIN; cam_zone_return 1"
SHOT=(--exec "cube 90" --exec-at 20 "teleport 18000 3900 8000" --exec-at 100 "camnudge 0 5 2"
    --exec-at 150 "teleport 13000 3900 22000" --exec-at 200 "input left 40"
    --exec-at 300 "teleport 18000 3900 8000" --tick 620)
run shot_off --exec-at 15 "$SHOT_CVARS; cam_world_anchor 0" "${SHOT[@]}"
run shot_on --exec-at 15 "$SHOT_CVARS; cam_world_anchor 1" "${SHOT[@]}"

# The target on the last frame before the shot fired, and on the final frame.
shot_ends() {
    awk 'NR == 1 { for (i = 1; i <= NF; i++) col[$i] = i; next }
        !fired && $col["zone"] == 1 { fired = 1; before = prev }
        { prev = $col["target"] }
        END { print fired + 0, before + 0, prev + 0 }' "$work/$1.tsv"
}
read -r off_fired off_before off_last <<< "$(shot_ends shot_off)"
read -r on_fired on_before on_last <<< "$(shot_ends shot_on)"

[ "$off_fired" -eq 1 ] && [ "$on_fired" -eq 1 ] || fail "the shot never fired: the route no longer reaches zone 31"
[ "$off_last" != "$off_before" ] \
    || fail "with the setting off the camera came back to its world direction $off_before: the hero did not turn in the box, so the anchored run proves nothing"
[ "$on_last" = "$on_before" ] \
    || fail "with cam_world_anchor 1 the shot gave back $on_last where the camera had faced $on_before before it"

pass "target held through $(travel turn_on 200 550 heroBeta) units of hero turn (off: moved $(span turn_off 200 400 target)), an orbit moved it $(span turn_on 550 590 target) and that held too; no drift with cam_hold_angle 0 (off: $(span drift_off 140 390 target)); the held shot gave back $on_last, the direction before it (off: $off_last against $off_before)"
