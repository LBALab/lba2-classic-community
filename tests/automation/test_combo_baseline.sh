#!/usr/bin/env bash
# The input baseline: what the engine did before the input work started moving it.
#
# The eight combo fixtures each assert the hero's animation or position at a chosen tick.
# That is what a fixture is for and it is also its limit: it only looks where someone thought
# to look. These two recordings carry a per-tick digest of scene, hero, camera, every other
# actor and all 336 script variables, so a change that moves something nobody asserted is
# caught here and nowhere else. Between them, the fixtures say what the rules are and the
# recordings say that nothing else moved.
#
# Two files, and the second is not a spare. `combo-set.rec` drives the input *combinations*;
# `combo-controls.rec` drives each of the same inputs on its own, over the same 661 ticks.
# Swapping I_LEFT and I_RIGHT in the turn block is caught by the control at tick 21 and by the
# combination only at tick 201, because a frame holding both directions runs the same branch
# either way. A combination without its control pins the one session where the rule under test
# happens not to matter.
#
# Neither is regenerated here. A baseline re-recorded against the build it is meant to be
# judging has stopped being one; recordings/README.md says when it is legitimate to remake them.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=combo_baseline
. "$(dirname "$0")/lib.sh"
precheck

SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$SAVE" ] || skip "fixture missing: $SAVE"

# Reports through CHECKED and is called directly, never as `x=$(replay ...)`: `fail` ends the
# shell it runs in, which inside $( ) is only the subshell, so the caller would carry on and
# print a pass over the top of the failure.
CHECKED=""
replay() { # replay <name>
    local rec="$REPO/tests/automation/recordings/$1.rec" out summary
    [ -f "$rec" ] || fail "$1: the baseline recording is missing from recordings/"
    out="$(ctl --fixed-dt 16 --load "$SAVE" --replay "$rec" --tick 800 --exit 2>&1)" ||
        fail "$1: replay run exited non-zero ($?): hang or crash"
    case "$out" in
    *"is format"*) fail "$1: $(printf '%s\n' "$out" | grep -m1 'is format')" ;;
    *"consistency failure"*)
        fail "$1: $(printf '%s\n' "$out" | grep -m1 'consistency failure')" ;;
    esac
    summary="$(printf '%s\n' "$out" | grep -m1 'replay ended')" ||
        fail "$1: the replay printed no summary; it cannot be said to have matched"
    case "$summary" in
    *"first hash mismatch -1"*) ;;
    *) fail "$1: $summary" ;;
    esac
    CHECKED="$(printf '%s\n' "$summary" | sed -n 's/.*: \([0-9]*\) ticks checked.*/\1/p')"
    # The file holds 661. Reading fewer means the reader lost the stream rather than that the
    # session was short, which is a different failure from a divergence and reads the same
    # without this.
    [ "${CHECKED:-0}" -ge 660 ] ||
        fail "$1: checked ${CHECKED:-0} ticks of the 661 the file holds"
}

replay combo-set;      set_ticks="$CHECKED"
replay combo-controls; ctl_ticks="$CHECKED"

pass "the input baseline still reproduces: $set_ticks ticks of combinations and $ctl_ticks of their controls"
