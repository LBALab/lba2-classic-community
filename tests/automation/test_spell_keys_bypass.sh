#!/usr/bin/env bash
# The spell keys reach the game without reaching Input, and this is what that costs.
#
# Four of the thirty-six binding slots are not read the way the other thirty-two are. The
# spells go through SpellKeyDown in PERSO.CPP,
#
#     return CheckKey(DefKeys[n].Key1) OR CheckKey(DefKeys[n].Key2)
#         OR CheckKey(GamepadKeys[n].Key1) OR CheckKey(GamepadKeys[n].Key2);
#
# which reads the binding table directly and never sets a bit in `Input`. It has to: slots 32
# to 35 were given action bits that collide with the movement ones, so I_PINGOUIN and I_UP are
# both (1 << 0) and a spell that reached `Input` would read as walking forward.
#
# That makes them the one part of the input surface where "what the player pressed" and "what
# the simulator was given" genuinely disagree, and it is the reason the two-funnel problem is a
# problem rather than a tidiness complaint. Increments 3 and 4 rework exactly this table, so
# what it does today is worth a number rather than a description.
#
# The number comes from the boundary-2 counters, which count TabKeys rises against Input rises:
#
#     a bound key      1 key rise, 1 action rise        the funnel agrees with itself
#     a spell key      1 key rise, 0 action rises       the key reached the game another way
#
# and "reached the game" is not inferred: the hero's behaviour changes on the same press.
#
# Also here because it is the same code and nothing else covers it: each spell carries its own
# hand-rolled edge latch (`WaitNoKey33` and siblings), so holding the key fires once and
# releasing and pressing again fires twice. That is the per-key suppression increment 2 replaces
# with a per-subsystem mask, and this pins what it does now.
#
# Runs against the corpus save as it stands. It already carries FLAG_PROTOPACK, so no quest
# state is set up here; the spell being reachable is a property of the save, and a save that
# lost the flag would fail the first assertion rather than pass a weaker test.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=spell_keys_bypass
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

# SDL scancodes. J is the shipped I_JETPACK binding, slot 33; Up is an ordinary bound action.
JETPACK=13; UP=82
C_SPORTIF=1; C_NORMAL=0; C_JETPACK=8

out=$(mktemp -d); trap 'rm -rf "$out"' EXIT

# Reports through BEHAVIOUR, KEYRISES and INPUTRISES, and is called directly, never as
# `x=$(run ...)`: `fail` ends the shell it runs in, which inside $( ) is only the subshell.
BEHAVIOUR=""; KEYRISES=""; INPUTRISES=""
run() { # run <label> <exec> [ticks]
    ctl_headless --load "$FIX" --fixed-dt 16 --exec "skipmodals 1; $2" \
        --tick "${3:-60}" --dump-state "$out/$1.json" --exit >/dev/null 2>&1 ||
        fail "$1: engine exit $?"
    read -r BEHAVIOUR KEYRISES INPUTRISES <<EOF
$(python3 -c "
import json
d = json.load(open('$out/$1.json')); f = d['input_flow']
print(d['hero']['behaviour'], f['tabkey_rises'], f['input_rises'])")
EOF
    [ -n "$INPUTRISES" ] || fail "$1: could not read the hero and the flow counters out of the dump"
}

# --- the control: a bound key crosses the boundary -----------------------------------
run bound "key $UP 40 10"
[ "$KEYRISES" = 1 ] && [ "$INPUTRISES" = 1 ] ||
    fail "a bound key should give one key rise and one action rise, got $KEYRISES and $INPUTRISES"
[ "$BEHAVIOUR" = "$C_SPORTIF" ] ||
    fail "walking should not change behaviour, got $BEHAVIOUR"

# --- the spell: it does not, and still reaches the game -------------------------------
run spell "key $JETPACK 30 10"
[ "$KEYRISES" = 1 ] ||
    fail "the spell key should still register as a key rise, got $KEYRISES"
[ "$INPUTRISES" = 0 ] ||
    fail "the spell key should produce no action bit, got $INPUTRISES"
[ "$BEHAVIOUR" = "$C_JETPACK" ] ||
    fail "the spell should have reached the game and set behaviour $C_JETPACK, got $BEHAVIOUR"

# --- the combo: a direction held does not shut the other route -----------------------
# Two key rises and one action bit: the direction's, because the spell has none to contribute.
run combo "key $UP 40 10; key $JETPACK 30 15"
[ "$KEYRISES" = 2 ] ||
    fail "a direction and a spell should be two key rises, got $KEYRISES"
[ "$INPUTRISES" = 1 ] ||
    fail "only the direction should reach Input, so one action rise, got $INPUTRISES"
[ "$BEHAVIOUR" = "$C_JETPACK" ] ||
    fail "the spell should fire with a direction held, got behaviour $BEHAVIOUR"

# --- the latch: held fires once, re-pressed fires twice ------------------------------
run held "key $JETPACK 120 10" 150
[ "$BEHAVIOUR" = "$C_JETPACK" ] ||
    fail "a held spell key should fire once and stay, got behaviour $BEHAVIOUR"

run tapped "key $JETPACK 20 10; key $JETPACK 20 60" 150
[ "$BEHAVIOUR" = "$C_NORMAL" ] ||
    fail "two presses should toggle twice and land on $C_NORMAL, got $BEHAVIOUR"
[ "$KEYRISES" = 2 ] ||
    fail "two presses should be two key rises, got $KEYRISES"

pass "the spell key reaches the game with no action bit (1 key rise, 0 action rises, behaviour $C_JETPACK), fires with a direction held, and its own latch makes a hold fire once against two for a re-press"
