#!/usr/bin/env bash
# Skipping a post-choice dialogue line must skip the line and nothing else.
#
# GameAskChoice speaks the option the player picked and spins until the line ends or
# a Menus/Esc press cuts it short. That press has to be consumed there: the main loop
# tests the same MyKey == K_ESC OR Input & I_MENUS a few hundred lines later, so a
# press left live skips the line AND opens the main menu (#451). Dial(), MyDial() and
# the found-object path all consume after their identical skip loops; this one did not.
#
# What the runs assert, per key:
#
#   linepolls  how long the chosen line's own wait ran. A press that lands cuts it short,
#              so it is far below the same run with no press. Without this the test would
#              pass on an engine where the injection quietly stopped arriving, having
#              proved only that a press nobody made was not left behind.
#
#              The line's wait, not the whole modal: most of a GameAskChoice is MyDial
#              speaking the QUESTION, so a ratio over the total would be asking whether
#              the answer outlasts the question in whichever bank the save's scene loads,
#              and would fail on correct code in a bank where it does not.
#   Input      must be 0 on return: the bit the skip loop exited on is gone.
#   MyKey      must be 0 on return: the Esc half of the main loop's test travels here,
#              and InitWaitNoInput does not touch it. Esc reproduces with Input clear,
#              so a test watching Input alone would miss that half of the fix.
#
# Three keys because they take three different routes into the same check: F10 is the
# default I_MENUS keyboard binding, Esc arrives through MyKey instead, and pad Start
# reaches Input through GetJoys and the combined binding table, never through MyKey.
#
# Local-only: needs retail data and the per-scene VOX banks, because the window this
# is about is a voice line playing. Not in host_quick CI.
TESTNAME=askchoice_menus_consume
. "$(dirname "$0")/lib.sh"
precheck
need_save
have_voice || skip "no per-scene VOX banks in $LBA2_GAME_DIR (voice is the window under test)"

# question and choice text ids in whichever bank the save's scene loads. Any ids do:
# the test reads how long the chosen line played, not what it said.
Q=1
C1=2
C2=3

# askchoice [key] -> prints the verb's "done" line, or nothing if the run failed.
askchoice() {
    local press="${1:-}" spec="ui askchoice"
    [ -n "$press" ] && spec="$spec --press $press"
    ctl_voice --load "$LBA2_TEST_SAVE" \
        --exec-at 40 "$spec $Q $C1 $C2" --tick 120 --exit 2>&1 |
        grep -F "ui askchoice done:"
}

field() { # field <line> <name> -> value
    printf '%s\n' "$1" | sed -n "s/.*$2=\([^ ]*\).*/\1/p"
}

# askchoice's own exit status is the grep's, so a dead run and a missing result line are
# the same failure; one check covers both.
base_line="$(askchoice)" \
    || fail "control run produced no result line (run died, or the verb is missing)"
base_polls="$(field "$base_line" linepolls)"

# A bank whose chosen line has no voice gives the press no window to land in, and
# every assertion below would then be vacuous. Skip rather than pass.
[ "${base_polls:-0}" -gt 5000 ] \
    || skip "chosen line is silent in this bank (its wait ran only ${base_polls:-0} polls)"

for key in menus esc pad-start; do
    line="$(askchoice "$key")" || fail "$key run produced no result line (run died?)"

    polls="$(field "$line" linepolls)"
    mykey="$(field "$line" MyKey)"
    input="$(field "$line" Input)"

    [ "$polls" -lt $((base_polls / 2)) ] \
        || fail "$key never cut the line short (line ran $polls polls vs $base_polls unpressed)"

    [ "$input" = "0x0" ] \
        || fail "$key left Input=$input live; the main loop reads it again and opens the menu"
    [ "$mykey" = "0" ] \
        || fail "$key left MyKey=$mykey live; the main loop tests MyKey == K_ESC too"
done

pass "Menus, Esc and pad Start each skip the line and are consumed (unpressed line $base_polls polls)"
