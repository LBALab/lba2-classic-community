#!/usr/bin/env bash
# The gamepad's binding table resolves to the same actions the keyboard's does.
#
# There are two tables, DefKeys and GamepadKeys, and every slot is looked up in both:
# InitInput folds the pair into one combined (key, mask) list, so by the time GetInput scans it
# nothing downstream knows which device a press came from. Nothing tested that, and the
# increments that rework the table are exactly the ones that could break it: naming the keys,
# splitting layout from preference, and putting the remaining hardcoded sources onto the layer.
#
# Driven through `key` with K_GAMEPAD_* scancodes, which land in the packed joystick tail of
# TabKeys the same way GetJoys writes them, so this needs no pad plugged in. That is the only
# reason pad coverage is affordable here at all.
#
# Three claims, each an identity rather than a direction, because "the hero turned left" would
# pass on a table that had quietly stopped distinguishing anything.
#
#   The pad's binding for an action lands the hero in exactly the state the keyboard's does.
#   The shipped defaults are not the same key, or the same kind of key: I_UP is the right
#   trigger on the pad and the arrow on the keyboard, and I_LEFT is a stick direction against
#   an arrow. Same slot, different scancodes, identical outcome.
#
#   The pad's own contradiction resolves the way the keyboard's does. Left and Right together
#   is Left, because the resolution is the if/else chain in OBJECT.CPP and it never sees a
#   device.
#
#   A contradiction spanning both devices resolves the same way, in both directions. The pad's
#   Left beats the keyboard's Right and the keyboard's Left beats the pad's Right, which says
#   the two tables merge into one Input before anything resolves them. A change that gave one
#   device precedence would pass the first two claims and fail this one.
#
# What decides these bindings is lba2.cfg, not the table compiled into INPUT_BINDINGS.CPP:
# ReadGamepadConfig reads `Gamepad<n>` per slot and the compiled entry is only the fallback for
# a key the file does not name. lib.sh hands every fixture a fresh settings folder, so the run
# writes the shipped layout and reads it back. Swapping `Gamepad2` and `Gamepad3` in that file
# turns the pad's left into a right turn while the keyboard still turns left, which is the first
# identity below failing, and is what a rebind regression would look like.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=pad_bindings
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

# Keyboard scancodes, then the shipped gamepad bindings for the same slots
# (INPUT_BINDINGS.CPP): I_UP is the right trigger, I_LEFT and I_RIGHT the left stick.
KB_UP=82; KB_LEFT=80; KB_RIGHT=79
PAD_UP=1048; PAD_LEFT=1041; PAD_RIGHT=1042

out=$(mktemp -d); trap 'rm -rf "$out"' EXIT

# Reports through STATE and is called directly, never as `x=$(state ...)`: `fail` ends the
# shell it runs in, which inside $( ) is only the subshell.
STATE=""
state() { # state <label> <key holds>
    ctl_headless --load "$FIX" --fixed-dt 16 --exec "skipmodals 1; $2" \
        --tick 120 --dump-state "$out/$1.json" --exit >/dev/null 2>&1 || fail "$1: engine exit $?"
    STATE=$(python3 -c "
import json
h = json.load(open('$out/$1.json'))['hero']
print(h['x'], h['y'], h['z'], h['beta'], h['gen_anim'])")
    [ -n "$STATE" ] || fail "$1: could not read the hero out of the dump"
}

# --- the same slot, either table -----------------------------------------------------
state kb_up  "key $KB_UP 90 10";  kb_up="$STATE"
state pad_up "key $PAD_UP 90 10"; pad_up="$STATE"
[ "$kb_up" = "$pad_up" ] ||
    fail "the pad's I_UP should land where the keyboard's does: '$pad_up' against '$kb_up'"

state kb_left  "key $KB_LEFT 90 10";  kb_left="$STATE"
state pad_left "key $PAD_LEFT 90 10"; pad_left="$STATE"
[ "$kb_left" = "$pad_left" ] ||
    fail "the pad's I_LEFT should land where the keyboard's does: '$pad_left' against '$kb_left'"

# Walking and turning have to be different states, or the two identities above are vacuous.
[ "$kb_up" != "$kb_left" ] ||
    fail "walking and turning gave the same state, so these identities prove nothing: '$kb_up'"

# --- the contradiction, on the pad ---------------------------------------------------
state pad_both "key $PAD_LEFT 90 10; key $PAD_RIGHT 90 10"
[ "$STATE" = "$kb_left" ] ||
    fail "pad Left and Right together should resolve as Left does: '$STATE' against '$kb_left'"

# --- the contradiction, spanning both devices ----------------------------------------
state kbl_padr "key $KB_LEFT 90 10; key $PAD_RIGHT 90 10"
[ "$STATE" = "$kb_left" ] ||
    fail "keyboard Left against pad Right should resolve as Left: '$STATE' against '$kb_left'"

state padl_kbr "key $PAD_LEFT 90 10; key $KB_RIGHT 90 10"
[ "$STATE" = "$kb_left" ] ||
    fail "pad Left against keyboard Right should resolve as Left: '$STATE' against '$kb_left'"

pass "both binding tables resolve to the same actions, and a contradiction resolves by code order whichever device supplies which half"
