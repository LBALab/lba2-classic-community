#!/usr/bin/env bash
# With the Auto camera on, flipping any of its settings changes how a run is seen, never what happens in it.
#
# The Modern ring's contract (docs/knowledge/decisions/a-camera-setting-is-watched-not-played.md).
# Each scene is recorded once with the Auto camera at its defaults and replayed once per camera
# setting, with that setting flipped on the console at the start. The replay's world hash (the
# game) must match on every tick; its view hash (the camera) may not.
#
# Three things keep it from passing by measuring nothing. The unflipped replay matches on both
# hashes, so a mismatch is the flip's. A script variable written mid-replay breaks the world
# hash, so the world side can disagree. And a flip must move the camera in at least one scene,
# its view hash or the view drawn on some step (the camera trace), unless RING_UNEXERCISED
# names it with the reason: a flip that moves nothing proves nothing,
# and a route that stops reaching a setting should say so rather than go on passing.
#
# The world hash is what the game holds, not what the player saw: a camera bug that draws the
# actors off the scenery (#717) passes here. test_ring_off_is_gone.sh checks the picture.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=ring_watch_not_play
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/ringlib.sh"
precheck
[ -f "$RING_SAVE" ] || skip "fixture missing: $RING_SAVE"
case "$(ctl_headless --exec 'help settings_before' --tick 2 --exit 2>&1)" in
    *"Unknown help topic"*) skip "binary predates the settings table's console commands" ;;
esac
ring_check_rows

# Settings no scene here moves, and why. Each passes the world check vacuously, so the list is
# the honest edge of what this fixture shows. All four act on the two adjustments the Quirk
# docs/knowledge/quirks/affgrilleext-sets-the-camera-from-the-globals.md shows never reach the
# frame: the HD recompose's pitch steepen and the ground lift, both applied around
# CameraCenter(3) and overwritten when the terrain is drawn. Should either become live, its
# rows move the camera here and the pass line names them, so the list can shrink.
RING_UNEXERCISED="
cam_hd_pitch      the recompose's pitch steepen, overwritten before the frame is drawn
cam_ground        the ground lift, overwritten the same way
cam_ground_clear  the ground lift's height
cam_manual_hold   how long a manual nudge holds off those two
"

work="$(mktemp -d -t lba2-ring-watch-XXXXXX)"
trap 'rm -rf "$work"' EXIT

moved=" "
flipped=" "
for scene in ${RING_SCENES_ONLY:-$RING_SCENES}; do
    ring_record "$work" "$scene" 1
    ring_defaults "$work" "$scene" 1

    ring_launch "$work" "$scene" 1 none ""
    ring_launch "$work" "$scene" 1 again ""
    ring_launch "$work" "$scene" 1 world "" --exec-at 50 "vargame 10 77"
    for flip in "${RING_FLIPS[@]}"; do
        name="${flip%%=*}" value="${flip#*=}"
        ring_is_default "$name" "$value" && continue
        ring_launch "$work" "$scene" 1 "$name" "$name $value"
    done
    ring_wait

    ring_read "$work" "$scene" none
    [ "$RW" = -1 ] && [ "$RV" = -1 ] ||
        fail "$scene: the unflipped replay mismatched (world $RW, view $RV): nothing a flip shows can be told from that"
    [ "$RN" -ge "$SCENE_TICKS" ] || fail "$scene: the camera trace logged $RN steps of $SCENE_TICKS"
    drawn="$RS"
    ring_read "$work" "$scene" again
    [ "$RS" = "$drawn" ] || fail "$scene: two unflipped replays drew different views: the drawn view cannot show a flip"
    ring_read "$work" "$scene" world
    [ "$RW" -ge 0 ] || fail "$scene: a script variable written at tick 50 left the world hash matching: the world side cannot see the game"

    for flip in "${RING_FLIPS[@]}"; do
        name="${flip%%=*}" value="${flip#*=}"
        ring_is_default "$name" "$value" && continue
        flipped="$flipped$name "
        ring_read "$work" "$scene" "$name"
        ring_flip_took "$work" "$scene" "$name" "$name" "$value"
        [ "$RW" = -1 ] ||
            fail "$scene: $name $value changed what happens, first at tick $RW (view first at $RV): a camera setting took part in the game"
        # Reached: the camera the game holds moved, or the view drawn from it did.
        if [ "$RV" -ge 0 ] || [ "$RS" != "$drawn" ]; then moved="$moved$name "; fi
    done
done

unflipped=""
for flip in "${RING_FLIPS[@]}"; do
    case "$flipped" in *" ${flip%%=*} "*) ;; *) unflipped="$unflipped ${flip%%=*}" ;; esac
done
[ -z "$unflipped" ] ||
    fail "RING_FLIPS sets these to the value they already hold in every scene, so they were never flipped:$unflipped"

never=""
also=""
for flip in "${RING_FLIPS[@]}"; do
    name="${flip%%=*}"
    case "$moved" in *" $name "*) listed=1 ;; *) listed=0 ;; esac
    if printf '%s\n' "$RING_UNEXERCISED" | grep -q "^$name "; then
        [ "$listed" = 1 ] && also="$also $name"
    else
        [ "$listed" = 1 ] || never="$never $name"
    fi
done
[ -z "$never" ] ||
    fail "flipped and never moved the camera in any scene:$never; a route lost it, or add it to RING_UNEXERCISED with the reason"

n_flips=${#RING_FLIPS[@]}
# shellcheck disable=SC2086 # the scene list splits on spaces on purpose
n_scenes=$(printf '%s\n' ${RING_SCENES_ONLY:-$RING_SCENES} | wc -l | tr -d ' ')
pass "$n_flips camera settings flipped in $n_scenes scenes: the world matched every time${also:+ (RING_UNEXERCISED can drop:$also)}"
