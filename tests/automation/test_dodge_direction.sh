#!/usr/bin/env bash
# The dodge is a modifier: it changes what a direction means rather than doing anything itself.
#
# Each of the four direction branches in OBJECT.CPP carries the same shape,
#
#     if (Input & I_ESQUIVE) {
#         if (ptrobj->FlagAnim != ANIM_ALL_THEN) InitAnim(GEN_ANIM_ESQUIVE_<dir>, ...);
#     } else
#         InitAnim(GEN_ANIM_<dir>, ANIM_REPEAT, numobj);
#
# so I_ESQUIVE alone selects nothing and a direction alone walks. The pair is what picks one of
# four animations, and this is the only combo in the set where the modifier is a separate bit
# rather than a second press of something already meaningful.
#
# Eight runs, four pairs:
#
#     up     GEN_ANIM_MARCHE   1   ->  GEN_ANIM_ESQUIVE_AVANT     42
#     down   GEN_ANIM_RECULE   2   ->  GEN_ANIM_ESQUIVE_ARRIERE   43
#     left   GEN_ANIM_GAUCHE   3   ->  GEN_ANIM_ESQUIVE_GAUCHE    41
#     right  GEN_ANIM_DROITE   4   ->  GEN_ANIM_ESQUIVE_DROITE    40
#
# The direction-alone runs are half the test, not scaffolding: without them a fixture cannot tell
# a modifier that stopped working from a direction that stopped working.
#
# Note which way round the last two are. The plain animations run gauche then droite and the dodges
# run droite then gauche, so left is 41 against right's 40. Nothing catches that pair being swapped
# except naming both, since either way the hero dodges and either way it is a dodge animation.
#
# Sampled at tick 30. A dodge is ANIM_ALL_THEN, so it plays once and stops: sampling late enough
# reads GEN_ANIM_RIEN and looks exactly like a dodge that never fired.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=dodge_direction
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

# SDL scancodes, so this does not depend on the `key` verb's short-name table growing.
# X is the shipped I_ESQUIVE binding (INPUT_BINDINGS.CPP).
DODGE=27; UP=82; DOWN=81; LEFT=80; RIGHT=79
NORMAL=0

out=$(mktemp -d); trap 'rm -rf "$out"' EXIT

# The hero's general animation after holding <keys> from poll 11, reported through ANIM.
#
# Through a global and called directly, never as `x=$(gen_anim ...)`: `fail` ends the shell it runs
# in, and inside $( ) that is only the subshell, so the caller carries on and prints a pass over
# the top of the failure.
ANIM=""
gen_anim() { # gen_anim <label> <key-holds>
    ctl_headless --load "$FIX" --fixed-dt 16 \
        --exec "skipmodals 1; behaviour $NORMAL; $2" \
        --tick 30 --dump-state "$out/$1.json" --exit >/dev/null 2>&1 || fail "$1: engine exit $?"
    ANIM=$(python3 -c "import json;print(json.load(open('$out/$1.json'))['hero']['gen_anim'])")
    [ -n "$ANIM" ] || fail "$1: could not read the hero's animation out of the dump"
}

DODGED=""
check() { # check <dir-name> <scancode> <plain> <dodged>
    local name="$1" sc="$2" plain="$3" dodged="$4"
    gen_anim "${name}_alone" "key $sc 60 10"
    [ "$ANIM" = "$plain" ] || fail "$name alone should walk ($plain), got $ANIM"
    gen_anim "${name}_dodge" "key $DODGE 60 10; key $sc 60 10"
    [ "$ANIM" = "$dodged" ] || fail "$name with the dodge should give $dodged, got $ANIM"
    DODGED="$ANIM"
}

check up    $UP    1 42; fwd="$DODGED"
check down  $DOWN  2 43; back="$DODGED"
check left  $LEFT  3 41; lft="$DODGED"
check right $RIGHT 4 40; rgt="$DODGED"

# Four directions, four dodges. A change that collapsed them to one, or that swapped the sideways
# pair, still leaves every run above dodging.
[ "$(printf '%s\n' "$fwd" "$back" "$lft" "$rgt" | sort -u | wc -l)" = 4 ] \
    || fail "the four dodges are not four distinct animations: $fwd $back $lft $rgt"

# The dodge on its own selects nothing: it is a modifier, and with no direction there is nothing
# to modify.
gen_anim dodge_alone "key $DODGE 60 10"
[ "$ANIM" = 0 ] || fail "the dodge with no direction should leave the hero idle (0), got $ANIM"

pass "the dodge redirects each direction: forward $fwd, back $back, left $lft, right $rgt, and selects nothing on its own"
