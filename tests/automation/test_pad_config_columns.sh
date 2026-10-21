#!/usr/bin/env bash
# The keyboard reaches both binding columns of the Game Controller screen.
#
# The screen filters keys before acting on them, and its list had no Left or
# Right, so the line that switches between the Key1 and Key2 columns only ever
# saw the pad's own navigation (#508). A player on this screen is often there
# because the pad is not working.
#
# Each run opens the screen with `ui config-pad`, drives it with `key` holds
# armed in the same command line (no command runs while the screen is up), binds
# a pad input to row 0, accepts, and reads the binding back from the run's
# lba2.cfg. The holds are counted in input polls, which the screen's own loop
# keeps taking; 30 polls between presses leaves room for its wait-for-release.
#
#   base   Enter, bind, accept: the bind lands in Key1. Without this the test
#          would pass on an engine where the presses quietly stopped arriving.
#   right  Right first: the bind lands in Key2 and Key1 keeps its default.
#   tab    Tab is Right on this screen, as on the keyboard screen.
#   back   Right then Left: back in Key1.
#
# The input bound is the left stick pushed up, which no default binding uses.
# A duplicate is refused and silently restored (the D-pad, for one, is bound to
# the spell rows), which would read as the bind never landing.
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
# Tier: medium
TESTNAME=pad_config_columns
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

FREE_PAD=1039      # K_GAMEPAD_LSTICK_UP
ROW0_DEFAULT=1048  # K_GAMEPAD_RTRIGGER, row 0 (I_UP) Key1 by default

value_of() { # value_of <cfg> <key>
    grep -aE "^[[:space:]]*${2}[[:space:]]*:" "$1" 2>/dev/null | head -1 |
        sed -E 's/^[^:]*:[[:space:]]*//; s/[[:space:]]*$//' | tr -d '\r'
}

# drive <name> <keys...>: press each key in turn, 30 polls apart, then bind
# FREE_PAD, go to the Accept row and accept. Prints "<Key1> <Key2>" for row 0.
drive() {
    local name="$1" user="$tmp/$1" cmds="" delay=30 out
    shift
    mkdir -p "$user"
    for k in "$@" enter "$FREE_PAD" end enter; do
        cmds="${cmds}key $k 3 $delay; "
        delay=$((delay + 30))
    done
    out=$(ctl --language English --fixed-dt 16 --user-dir "$user" \
          --exec-at 5 "${cmds}ui config-pad" --tick 20 --exit 2>&1)
    # Runs inside $( ), where fail's message would land in the caller's variable.
    if ! printf '%s' "$out" | grep -aq 'ui config-pad done'; then
        echo "FAIL: $TESTNAME: $name: the screen never closed (a press did not land, or the verb is missing)" >&2
        return 1
    fi
    printf '%s %s\n' "$(value_of "$user/lba2.cfg" Gamepad0)" \
        "$(value_of "$user/lba2.cfg" Gamepad0_2)"
}

expect() { # expect <name> <got> <want> <what>
    [ "$2" = "$3" ] || fail "$1: $4: row 0 is Key1/Key2 '$2', want '$3'"
}

got=$(drive base) || exit 1
expect base "$got" "$FREE_PAD 0" "the bind did not land in Key1"

got=$(drive right right) || exit 1
expect right "$got" "$ROW0_DEFAULT $FREE_PAD" "Right did not move to Key2"

got=$(drive tab tab) || exit 1
expect tab "$got" "$ROW0_DEFAULT $FREE_PAD" "Tab did not move to Key2"

got=$(drive back right left) || exit 1
expect back "$got" "$FREE_PAD 0" "Left did not move back to Key1"

pass "Left, Right and Tab switch columns; the bind lands where the cursor is"
