#!/usr/bin/env bash
# A 3D icon puts back the camera the frame was drawn with, so actors stay on the scenery on frames that run no simulation step.
#
# The holomap globe, the inventory and the behaviour menu borrow the 3D camera and hand it back
# through Init3DExtView. A rendered frame that runs no simulation step does not redraw the
# terrain, so the actors drawn on it use whatever camera that call left. Handed back from the
# globals, that is the end view of a shot blend (cam_shot_blend_ms) and leaves out cam_obstruct's
# extra pitch, and the actors jump off the scenery until the next step.
#
# The route is the Dinofly nest on Citadel Island (cube 49) from the corpus save Anon1, with the
# three game variables the scene reads set to just before Zoe's line about the healing wizard
# (text 951). Walking in fires the nest's shot, which blends in; partway through, she says the
# line and her script shows the holomap icon while the blend finishes. --fixed-dt 2 with
# --fixed-timestep 16 renders eight frames per simulation step, as a display above 400 fps does.
#
# The check is on two frames: the last one with a step, and the one after it, which has none.
# Nothing moved between them, so apart from the spinning icon and a few pixels of animation they
# are the same picture. Before the fix the actors on the second one stand where the shot's end
# view puts them, some ten thousand pixels away.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
# Tier: medium
TESTNAME=icon_blend_camera
. "$(dirname "$0")/lib.sh"
precheck

SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
[ -f "$SAVE" ] || skip "fixture missing: $SAVE"
case "$(ctl_headless --exec 'varlist' --tick 2 --exit 2>/dev/null)" in
    *cam_shot_blend_ms*) ;;
    *) skip "binary has no cam_shot_blend_ms setting" ;;
esac

work="$(mktemp -d -t lba2-icon-blend-XXXXXX)"
trap 'rm -rf "$work"' EXIT

# nest_run <tick>: the route, ending on that rendered frame with a screenshot of it.
nest_run() {
    LBA2_USER_DIR="$work/user-$1"
    export LBA2_USER_DIR
    mkdir -p "$LBA2_USER_DIR"
    ctl_headless --verbose --load "$SAVE" --fixed-dt 2 --fixed-timestep 16 \
        --exec "vargame 253 2; vargame 56 5; vargame 62 0; settings_before cam_; cam_follow 1; cam_shot_blend_ms 1500; skipmodals 1" \
        --exec-at 16 "cube 49" \
        --exec-at 80 "teleport 14600 2000 14300 3363" \
        --exec-at 160 "camtrace 1; input up 30" \
        --tick "$1" --screenshot "$work/f$1.png" --exit > "$work/run-$1.log" 2>&1 \
        || fail "run to tick $1 failed: exit $?"
}

# steps <tick>: the simulation steps that run recorded, one camera trace line each.
steps() { grep -c '\[cam\]' "$work/user-$1/adeline.log"; }

# The first tick from 440 on that ends on a step: the step count goes up by one.
nest_run 439
prev="$(steps 439)"
grep -q 'Dial(text=951' "$work/run-439.log" \
    || fail "Zoe's line never opened: the route missed the nest, so the icon never showed"
step=""
for t in $(seq 440 448); do
    nest_run "$t"
    rows="$(steps "$t")"
    if [ "$rows" -gt "$prev" ]; then step="$t"; break; fi
    prev="$rows"
done
[ -n "$step" ] || fail "no simulation step between ticks 440 and 448: the throttle is not rendering 8 frames a step"

next=$((step + 1))
nest_run "$next"
[ "$(steps "$next")" = "$rows" ] || fail "tick $next ran a step too: no frame without one to check"

blend="$(grep '\[cam\]' "$work/user-$next/adeline.log" | tail -1 | sed -n 's/.* blend *\([0-9]*\) .*/\1/p')"
[ "${blend:-0}" -gt 0 ] || fail "the blend had ended by tick $next: nothing for the frame to get wrong"

# Pixels that differ between the two frames, leaving out the icon's corner.
moved="$(python3 - "$work/f$step.png" "$work/f$next.png" "$REPO/scripts/dev" <<'PY'
import sys
sys.path.insert(0, sys.argv[3])
import png_hash
w, h, c, a = png_hash.decode(sys.argv[1])
w2, h2, c2, b = png_hash.decode(sys.argv[2])
assert (w, h, c) == (w2, h2, c2)
n = 0
for i in range(w * h):
    if (i % w) < 96 and (i // w) < 96:
        continue
    if a[i * c:(i + 1) * c] != b[i * c:(i + 1) * c]:
        n += 1
print(n)
PY
)" || fail "could not compare the two frames"

# The fix leaves a few hundred pixels of animation; drawn with the shot's end view the actors
# move ten thousand or more.
[ "$moved" -le 3000 ] \
    || fail "$moved pixels moved between the step at tick $step and the frame after it with the icon up: the actors were drawn with a camera other than the scenery's"

pass "with the holomap icon up during a shot blend, the frame after a step (tick $next) differs from it by $moved pixels outside the icon"
