#!/usr/bin/env bash
# The fixed-timestep throttle must not drop the hero's melee press EDGE (#407 case 1). In aggressive
# mode a melee attack (GEN_ANIM_COUP_1/2/3) fires on the I_ACTION_M press edge, gated by an anti-
# repeat on LastInput (OBJECT.CPP MOVE_MANUAL, C_AGRESSIF). LastInput advances every rendered frame
# but the object loop that reads the edge runs only on stepped frames, so a press landing on a
# throttle-skipped frame is never seen and the attack never starts. #407's force-step
# (I_ACTION_EDGE, PERSO.CPP) fixes it -- but the melee case was only "covered by construction" and
# never auto-tested. This is that test.
#
# It uses the frame-precise injector (docs/INPUT_SIM_PLAN.md): `input fseq` places the action press
# on an exact rendered frame. At --fixed-dt 8 --fixed-timestep 24 the sim steps every 3 frames, so
# frame 30 is a step and frame 31 a skip. The melee must fire on BOTH -- the skip-frame case is the
# regression -- observed as a GEN_ANIM_COUP (17/18/19) in the per-sim-tick input trace.
#
# Local-only: needs retail data + a build + the tracked steam_classic_2023 corpus; skips otherwise.
TESTNAME=melee_throttle
. "$(dirname "$0")/lib.sh"
precheck

SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Desert Island.LBA"
[ -f "$SAVE" ] || skip "no Desert Island corpus save"

# Count GEN_ANIM_COUP_* (17/18/19 = a melee attack) frames in the input trace when the action press
# starts on rendered frame $1. Aggressive mode is set by injecting I_AGRESSIF (0x10000); 0x1 = up
# (walk), 0x80 = I_ACTION_M (action). An isolated XDG_DATA_HOME keeps the trace log to this run.
coup_frames() { # action-start-frame
    local f="$1" sv home log n
    sv="$(mktemp --suffix=.LBA)"
    home="$(mktemp -d)"
    cp "$SAVE" "$sv"
    XDG_DATA_HOME="$home" ctl_headless --load "$sv" --fixed-dt 8 --fixed-timestep 24 --log-level debug \
        --exec "input trace 1; input fseq 0:0x10000:6/0:up:120/${f}:up+action:6" --tick 60 --exit >/dev/null 2>&1
    log="$home/Twinsen/LBA2/adeline.log"
    n=$(grep "\[input\]" "$log" 2>/dev/null | grep -cE "gen_anim=(17|18|19)")
    rm -rf "$sv" "$home"
    echo "$n"
}

on_step="$(coup_frames 30)" # press lands on a stepped frame
on_skip="$(coup_frames 31)" # press lands on a throttle-skipped frame -- the #407 case

[ "$on_step" -gt 0 ] || fail "control: melee did not fire on a STEP-frame press (COUP=$on_step); fixture broken"
[ "$on_skip" -gt 0 ] || fail "throttle dropped the melee press edge on a skip frame (COUP=$on_skip): #407 regression"
pass "melee press survives the throttle (COUP frames: step=$on_step skip=$on_skip)"
