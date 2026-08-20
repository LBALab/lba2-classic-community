#!/usr/bin/env bash
# A jump survives Up being held through it, and ends on the frame Up is re-pressed.
#
# The engine reads input *history* here, not input state, which is the class most exposed to a
# lost or duplicated edge. In OBJECT.CPP, inside the Up/Down branch:
#
#     if (Jumping & 1023 AND LastMyJoy & (I_UP | I_DOWN) AND !FlagClimbing) {
#         // skip pour ne pas interrompre un saut
#         // si le joueur n'a pas relacher Up et Down
#         // avant de reappuyer dessus
#         ...
#         break;
#     } else {
#         ...
#         if (!(Jumping & 1024)) Jumping = FALSE;
#     }
#
# So the bit being in LastMyJoy is what saves the jump. Hold Up and it is; release and press again
# and, on that one frame, it is not, and the jump is cancelled where it stands. The 1997 comment
# says exactly this and it is a trick the speedrunning community depends on.
#
# Four arms, because the interesting claim is not "the jump can be cancelled":
#
#   held, never released     the jump runs to its natural end
#   released, not re-pressed the jump runs to its natural end, the SAME frame as held
#   re-pressed at poll 101   the jump ends at frame 101
#   re-pressed at poll 121   the jump ends at frame 121
#
# The second arm is the control that makes this a statement about the re-press rather than about
# the release. The last two are asserted against the poll they were pressed on rather than against a
# recorded number, so a change that cancelled the jump a frame early or late fails even though the
# jump still ends.
#
# What this pins, and what it does not. The re-press is held to the `LastMyJoy` term above: drop it
# and the jump runs to its natural end instead of stopping on the press frame, which is the failure
# this exists to catch. That a release is harmless is asserted as behaviour and not as a mechanism,
# because it does not follow from that branch alone: entering it whether or not a direction is down
# leaves the released arm ending exactly where it does now. Whatever decides that sits further down
# the same function, and this fixture does not name it.
#
# Frames and polls are the same clock in this run: --fixed-dt with nothing else polling means one
# input poll per rendered frame, and objtrace emits one line per simulated frame.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=jump_hold_repress
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

UP=82; SPACE=44; SPORTY=1
JUMP_ANIM=73   # the running jump, GEN_ANIM_SAUTE's concrete animation in this behaviour

out=$(mktemp -d); trap 'rm -rf "$out"' EXIT

# Walk in Sporty from poll 21, jump at poll 80, and trace the hero.
#
# Captured from stderr alone, never through `2>&1 |`: while the console is driving the run a log
# line reaches the stderr sink AND the console's stdout mirror, so a merged pipe collects every
# line twice from two differently buffered streams. On a per-frame trace that reads exactly like
# the clock jumping backwards (docs/CONTROL.md).
jump_run() { # jump_run <label> <up-key-holds>
    local label="$1" holds="$2"
    ctl_headless --load "$FIX" --fixed-dt 16 \
        --exec "skipmodals 1; behaviour $SPORTY; objtrace 0; $holds; key $SPACE 12 80" \
        --tick 160 --exit 2>"$out/$label.err" >/dev/null || fail "$label: engine exit $?"
    grep -q "\[obj\]" "$out/$label.err" || fail "$label: no object trace in the run"
}

# The frame the jump animation stops, or empty if it never started or never stopped.
jump_ends() { # jump_ends <label>
    awk -v want="anim=$JUMP_ANIM" '
        /\[obj\]/ {
            n++
            for (i = 1; i <= NF; i++) if ($i ~ /^anim=/) an = $i
            if (an == want) seen = 1
            else if (seen && !done) { print n; done = 1 }
        }' "$out/$1.err"
}

jump_run held      "key $UP 900 20"
jump_run released  "key $UP 70 20"
jump_run repress101 "key $UP 70 20; key $UP 900 100"
jump_run repress121 "key $UP 90 20; key $UP 900 120"

held=$(jump_ends held)
released=$(jump_ends released)
r101=$(jump_ends repress101)
r121=$(jump_ends repress121)

[ -n "$held" ] || fail "the held run never jumped, or never stopped jumping"
for v in released r101 r121; do
    eval "[ -n \"\$$v\" ]" || fail "$v: the run never jumped, or never stopped jumping"
done

# Letting go is not what cancels it. Both of these run to the same natural end.
[ "$released" = "$held" ] ||
    fail "releasing Up moved the end of the jump from $held to $released; only a re-press should"

# A re-press ends the jump on its own frame, exactly.
[ "$r101" = "101" ] || fail "re-pressing Up at poll 101 should end the jump at frame 101, got $r101"
[ "$r121" = "121" ] || fail "re-pressing Up at poll 121 should end the jump at frame 121, got $r121"

# And that is early, rather than the natural end arriving to meet it.
[ "$r101" -lt "$held" ] && [ "$r121" -lt "$held" ] ||
    fail "the re-pressed jumps ended at $r101 and $r121, no earlier than the held one at $held"

pass "the jump survives a held Up to frame $held, survives a release to $released, and ends on the re-press frame: $r101 and $r121"
