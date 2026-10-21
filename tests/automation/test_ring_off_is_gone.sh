#!/usr/bin/env bash
# With the Auto camera off, none of its settings changes anything: the game, the camera or the picture.
#
# The Original ring's contract for the Auto camera, from the rings Decision
# (docs/knowledge/decisions/a-camera-setting-is-watched-not-played.md): a player who never
# switches it on plays the original camera, whatever its settings hold. Each scene is
# recorded once with the classic camera and replayed once per Auto camera setting, flipped
# on the console at the start. Everything must match: the world hash, the view hash, the
# view drawn on every step (the camera trace) and the last frame, pixel for pixel.
#
# The picture is checked because original drawing code now calls into the Auto camera:
# Init3DExtView hands back the view the last frame was drawn with (#717), falling back to
# the classic camera's globals when no Auto camera view is live. A second check covers
# that path: the holomap icon up during the Dinofly nest shot (cube 49, the route of
# test_icon_blend_camera.sh), one frame compared per flip.
#
# Two controls keep it honest. Two unflipped replays match in every respect, so a
# difference is the flip's. And the same recording replayed with the Auto camera on moves
# the view, so the checks can see a camera change at all.
#
# Local-only (needs retail data + the tracked corpus saves). Not in host_quick CI.
# Tier: big
TESTNAME=ring_off_is_gone
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/ringlib.sh"
precheck
[ -f "$RING_SAVE" ] || skip "fixture missing: $RING_SAVE"
NEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
[ -f "$NEST_SAVE" ] || skip "fixture missing: $NEST_SAVE"
case "$(ctl_headless --exec 'help settings_before' --tick 2 --exit 2>&1)" in
    *"Unknown help topic"*) skip "binary predates the settings table's console commands" ;;
esac
ring_check_rows

work="$(mktemp -d -t lba2-ring-off-XXXXXX)"
trap 'rm -rf "$work"' EXIT

# Every scene's replays are started before any is read, so the runs of one scene fill the
# slots the last runs of the one before leave idle; the holomap icon's runs join them.
flips=0
for scene in ${RING_SCENES_ONLY:-$RING_SCENES}; do
    ring_record "$work" "$scene" 0
    ring_defaults "$work" "$scene" 0

    ring_launch "$work" "$scene" 0 none ""
    ring_launch "$work" "$scene" 0 again ""
    ring_launch "$work" "$scene" 1 on ""
    for flip in "${RING_FLIPS[@]}"; do
        name="${flip%%=*}" value="${flip#*=}"
        ring_is_default "$name" "$value" && continue
        ring_launch "$work" "$scene" 0 "$name" "$name $value"
    done
done

# The icon path. The nest route sets the three game variables the scene reads to just
# before Zoe's line about the healing wizard, which shows the holomap icon; tick 445 is a
# frame with the icon up. --fixed-dt 2 under --fixed-timestep 16 draws eight frames a step,
# as a display above 400 fps does, so most frames run no step and are drawn with the view
# Init3DExtView hands back.
NEST_TICK=445
nest_run() { # nest_run <tag> <console>
    local ud="$work/nest.$1.ud"
    mkdir -p "$ud"
    printf 'FollowCamera: 0\n' >"$ud/lba2.cfg"
    (
        LBA2_USER_DIR="$ud" timeout "$LBA2_TEST_TIMEOUT" "$LBA2_BIN" --headless --no-autosave \
            --language English --resolution 640x480 --vsync on --verbose --load "$NEST_SAVE" \
            --fixed-dt 2 --fixed-timestep 16 \
            --exec "vargame 253 2; vargame 56 5; vargame 62 0; skipmodals 1${2:+; $2}" \
            --exec-at 16 "cube 49" --exec-at 80 "teleport 14600 2000 14300 3363" \
            --exec-at 160 "input up 30" \
            --tick "$NEST_TICK" --screenshot "$work/nest.$1.png" --exit >"$work/nest.$1.out" 2>&1
        echo $? >"$work/nest.$1.rc"
        [ -f "$work/nest.$1.png" ] &&
            python3 "$REPO/scripts/dev/png_hash.py" "$work/nest.$1.png" >"$work/nest.$1.fhash"
    ) &
    ring_spawned
}
nest_frame() { # nest_frame <tag>: the frame's hash into NF
    [ "$(cat "$work/nest.$1.rc" 2>/dev/null)" = 0 ] || fail "nest/$1: run exited non-zero: hang or crash"
    NF="$(cat "$work/nest.$1.fhash" 2>/dev/null)"
    [ -n "$NF" ] || fail "nest/$1: no frame"
}

nest_run none ""
nest_run again ""
for flip in "${RING_FLIPS[@]}"; do
    nest_run "${flip%%=*}" "${flip%%=*} ${flip#*=}"
done
ring_wait

for scene in ${RING_SCENES_ONLY:-$RING_SCENES}; do
    ring_use_defaults "$work" "$scene"
    ring_read "$work" "$scene" none
    [ "$RW" = -1 ] && [ "$RV" = -1 ] ||
        fail "$scene: the unflipped replay mismatched (world $RW, view $RV): nothing a flip shows can be told from that"
    [ "$RN" -ge "$SCENE_TICKS" ] || fail "$scene: the camera trace logged $RN steps of $SCENE_TICKS"
    drawn="$RS" frame="$RF"
    ring_read "$work" "$scene" again
    [ "$RS" = "$drawn" ] && [ "$RF" = "$frame" ] ||
        fail "$scene: two unflipped replays drew different views or frames: a flip could not be told from that"
    ring_read "$work" "$scene" on
    [ "$RV" -ge 0 ] || fail "$scene: the Auto camera switched on left the view hash matching: the view side cannot see the camera"

    for flip in "${RING_FLIPS[@]}"; do
        name="${flip%%=*}" value="${flip#*=}"
        ring_is_default "$name" "$value" && continue
        ring_read "$work" "$scene" "$name"
        ring_flip_took "$work" "$scene" "$name" "$name" "$value"
        [ "$RW" = -1 ] || fail "$scene: $name $value changed the game with the Auto camera off, first at tick $RW"
        [ "$RV" = -1 ] || fail "$scene: $name $value moved the camera with the Auto camera off, first at tick $RV"
        [ "$RS" = "$drawn" ] || fail "$scene: $name $value changed the view drawn with the Auto camera off"
        [ "$RF" = "$frame" ] || fail "$scene: $name $value changed the last frame with the Auto camera off"
        flips=$((flips + 1))
    done
done

grep -q 'Dial(text=951' "$work/nest.none.out" ||
    fail "nest: Zoe's line never opened, so the icon was never up: the route missed the nest"
nest_frame none; base="$NF"
nest_frame again
[ "$NF" = "$base" ] || fail "nest: two unflipped runs drew different frames at tick $NEST_TICK"
for flip in "${RING_FLIPS[@]}"; do
    nest_frame "${flip%%=*}"
    grep -q "^\[control\] *${flip%%=*} = ${flip#*=}\$" "$work/nest.${flip%%=*}.out" ||
        fail "nest: the console never confirmed ${flip%%=*} = ${flip#*=}, so the flip may not have happened"
    [ "$NF" = "$base" ] || fail "nest: ${flip%%=*} ${flip#*=} changed the frame with the icon up and the Auto camera off"
done

# shellcheck disable=SC2086 # the scene list splits on spaces on purpose
n_scenes=$(printf '%s\n' ${RING_SCENES_ONLY:-$RING_SCENES} | wc -l | tr -d ' ')
pass "$flips flips over $n_scenes scenes and ${#RING_FLIPS[@]} with the holomap icon up changed nothing with the Auto camera off"
