#!/usr/bin/env bash
# A behaviour press cancels the animation in progress, on the frame it arrives.
#
# This is the trick the speedrunning community builds on, and it is a side effect rather than a
# feature. SetComportement in OBJECT.CPP rebuilds the hero and clears where he was in his current
# move:
#
#     ptrobj->GenAnim = NO_ANIM;
#     ...
#     ptrobj->FlagAnim = 0;
#     InitAnim(GEN_ANIM_RIEN, ANIM_REPEAT, NUM_PERSO);
#
# so a press lands the hero back at rest whatever he was doing, and a jump's recovery is cut short.
#
# The behaviour pressed is the one the hero is already in. Nothing checks for that, so the whole
# path runs anyway, and using it keeps the test to one variable: the animation set does not change
# underneath, so anything that moves is the reset and not a different behaviour's timings.
#
# Two things are asserted, and the second is what makes this about the trick rather than about an
# animation number.
#
#   The jump animation ends on the press frame, exactly, at three press times against a natural end
#   with no press at all.
#
#   How far the hero got depends on when the press lands. A standing jump carries its forward
#   motion late in the animation, so a press early enough leaves him exactly where he started, and
#   later presses keep more of the distance. That ordering is the property a runner uses.
#
# A standing jump, not a running one: the running jump from this save clears the ledge and the hero
# falls to Y=0, so there is no landing to cancel.
#
# Frames and polls are the same clock here: --fixed-dt with nothing else polling means one input
# poll per rendered frame, and objtrace emits one line per simulated frame.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=behaviour_cancels_jump
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

# SDL scancodes. F6 is the shipped I_SPORTIF binding, which is the behaviour this runs in.
SPACE=44; F6=63; SPORTY=1
JUMP_ANIM=71   # the standing jump in Sporty

out=$(mktemp -d); trap 'rm -rf "$out"' EXIT

# Jump at poll 31, optionally press the behaviour key, and trace the hero.
#
# Captured from stderr alone, never through `2>&1 |`: while the console drives the run a log line
# reaches the stderr sink AND the console's stdout mirror, so a merged pipe collects every line
# twice and a per-frame trace reads like the clock running backwards (docs/CONTROL.md).
#
# Reports through ENDED and MOVED and is called directly, never as `x=$(jump ...)`: `fail` ends the
# shell it runs in, and inside $( ) that is only the subshell.
ENDED=""; MOVED=""
jump() { # jump <label> <extra-exec>
    local label="$1"
    ctl_headless --load "$FIX" --fixed-dt 16 \
        --exec "skipmodals 1; behaviour $SPORTY; objtrace 0; key $SPACE 12 30; $2" \
        --tick 200 --exit 2>"$out/$label.err" >/dev/null || fail "$label: engine exit $?"
    grep -q "\[obj\]" "$out/$label.err" || fail "$label: no object trace in the run"
    ENDED=$(awk -v want="anim=$JUMP_ANIM" '
        /\[obj\]/ { n++
            for (i = 1; i <= NF; i++) if ($i ~ /^anim=/) an = $i
            if (an == want) seen = 1
            else if (seen && !done) { print n; done = 1 } }' "$out/$label.err")
    [ -n "$ENDED" ] || fail "$label: the hero never jumped, or never stopped"
    # Distance from the start of the run, along the ground.
    MOVED=$(awk '/\[obj\]/ { for (i = 1; i <= NF; i++) if ($i ~ /^pos=/) p = $i
            if (!first) first = p; last = p }
        END { split(substr(first,5), a, ","); split(substr(last,5), b, ",")
              dx = b[1] - a[1]; dz = b[3] - a[3]
              printf "%d", sqrt(dx*dx + dz*dz) }' "$out/$label.err")
}

# `key`'s third argument is a delay, so a key asked for after N polls is first down on N+1. Named
# by the poll it is actually pressed on, because that is what the end frame is asserted against.
press_at() { jump "press$1" "key $F6 6 $(($1 - 1))"; }

jump none "";  natural="$ENDED"; far="$MOVED"
press_at 60;   e60="$ENDED";     d60="$MOVED"
press_at 100;  e100="$ENDED";    d100="$MOVED"
press_at 120;  e120="$ENDED";    d120="$MOVED"

# The press frame is the end frame, whichever frame it is.
[ "$e60" = 60 ]   || fail "pressing at poll 60 should end the jump at frame 60, got $e60"
[ "$e100" = 100 ] || fail "pressing at poll 100 should end the jump at frame 100, got $e100"
[ "$e120" = 120 ] || fail "pressing at poll 120 should end the jump at frame 120, got $e120"

# And that is early. Without the press the jump runs on well past all three.
[ "$natural" -gt "$e120" ] \
    || fail "the uncancelled jump ended at $natural, no later than the latest press at $e120"

# The later the press, the more of the jump the hero keeps. An early one costs all of it.
[ "$d60" -lt "$d100" ] && [ "$d100" -lt "$d120" ] && [ "$d120" -lt "$far" ] \
    || fail "distance did not grow with press time: $d60 $d100 $d120 against $far uncancelled"
[ "$d60" -lt 10 ] \
    || fail "a press at poll 60 should leave the hero where he started, but he moved $d60"

pass "a behaviour press ends the jump on its own frame ($e60, $e100, $e120 against $natural), and keeps $d60, $d100 then $d120 of the $far it travels uncancelled"
