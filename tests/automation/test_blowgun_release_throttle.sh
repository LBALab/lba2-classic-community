#!/usr/bin/env bash
# #407 regression: the fixed-timestep throttle must not drop a hero input EDGE.
#
# The throttle (issue #358) skips the whole simulation on frames that render faster than one
# step. Hero weapon fire is level-triggered on I_THROW and *stops* on a release edge:
# `!(Input & I_THROW) && (LastInput & I_THROW)` (OBJECT.CPP, the weapon block's else branch).
# LastInput advances every RENDERED frame (PERSO.CPP), but the object loop that reads the edge
# only runs on SIMULATED frames. So when the release lands on a skipped frame, the edge is gone
# by the next simulated frame and the ANIM_REPEAT blowgun animation never resets -- the hero
# keeps firing on its own (issue #407 case 3). Same one-frame-signal-drop class as the item-use
# (#392) and scene-flip (#411) throttle regressions; the fix forces a sim step on any frame whose
# hero action/fire input changed (Timer_ForceStepIfPending input-edge case, PERSO.CPP).
#
# Test: fire the blowgun for a few frames then release, over the SAME window, with the throttle
# off (per-frame sim, the ground truth) and on (forcing frame-skips). Toggling only the throttle
# must not change the settled hero animation. --fixed-dt pins the cadence so the skip pattern is
# deterministic. Before the fix the throttled run stays stuck on the firing anim (SARBACANE 35 /
# SARBATRON 45) where the ground truth has returned to idle (RIEN 0).
#
# Local-only: needs retail data + a build + the tracked steam_classic_2023 corpus; skips cleanly
# otherwise. The corpus saves are git-tracked, so each run works on a temp copy (a --load run
# autosaves and must not mutate the fixture).
TESTNAME=blowgun_release_throttle
. "$(dirname "$0")/lib.sh"
precheck

CORPUS="$REPO/tests/savegame/corpus/saves/steam_classic_2023"
# Saves with the blowgun (weapon 23) selected and the hero idle at load, so a fire+release is
# the only thing that moves the animation off RIEN. Verified: idle (gen_anim 0) at load under
# both throttle states, so the load state is not a confound.
SAVES=("Bu" "DOS2" "Spaceship")

is_firing_anim() { [ "$1" = "35" ] || [ "$1" = "45" ]; } # GEN_ANIM_SARBACANE / GEN_ANIM_SARBATRON

# Fire the blowgun (hold I_THROW) for 6 SIM ticks, release, settle, report hero gen_anim. The
# `input` hold is metered in sim ticks (docs/INPUT_SIM_PLAN.md), so at --fixed-timestep 100 with
# --fixed-dt 8 one sim step is ~12 rendered frames: --tick must be large enough for the 6 fire
# ticks plus a settle window to elapse (else the throttled run ends mid-fire and looks "stuck").
gen_anim_after_fire() { # save-src fixedtimestep-ms
    local src="$1" fts="$2" sv dump ga
    sv="$(mktemp --suffix=.LBA)"; dump="$(mktemp)"
    cp "$src" "$sv"
    ctl_headless --load "$sv" --fixed-dt 8 --fixed-timestep "$fts" \
        --exec "input throw 6" --tick 300 --dump-state "$dump" --exit >/dev/null 2>&1
    ga="$(jget "$dump" "d['hero']['gen_anim']" 2>/dev/null)"
    rm -f "$sv" "$dump"
    echo "$ga"
}

tested=0
fails=""
for name in "${SAVES[@]}"; do
    src="$CORPUS/$name.LBA"
    [ -f "$src" ] || continue
    off="$(gen_anim_after_fire "$src" 0)"   # ground truth: every frame simulates, no skips
    on="$(gen_anim_after_fire "$src" 100)"  # throttle forcing frame-skips (the regression path)
    [ -n "$off" ] && [ -n "$on" ] || { fails="$fails
  $name: dump failed (off='$off' on='$on')"; continue; }
    # Sanity: the ground-truth run must have stopped firing, else the fixture/observable is wrong.
    if is_firing_anim "$off"; then
        fails="$fails
  $name: ground-truth (throttle off) is still firing (gen_anim=$off) -- bad fixture"
        continue
    fi
    tested=$((tested + 1))
    if [ "$on" != "$off" ]; then
        fails="$fails
  $name: throttle-off gen_anim=$off, throttle-on gen_anim=$on (blowgun stuck firing)"
    fi
done

[ "$tested" -gt 0 ] || skip "no usable blowgun corpus save (steam_classic_2023 not present?)"
[ -z "$fails" ] || fail "throttle changed the post-release hero animation (#407 input-edge drop):$fails"
pass "blowgun stops firing on release with and without the throttle ($tested save(s))"
