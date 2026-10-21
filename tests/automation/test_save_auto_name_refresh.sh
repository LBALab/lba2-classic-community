#!/usr/bin/env bash
# Overwriting a save with an automatic name gives it a fresh one.
#
# Without a keyboard, ChoosePlayerName names a new save "<island> - <datetime>". The name
# says where and when the save was made, so overwriting that save later names it afresh
# and removes the old file. A name the player typed is theirs and stays. With a keyboard,
# Enter on a slot's automatic name, untouched, does the same; before, the name editor
# refused it (the shape is reserved) and typing was dropped (the name fills the field),
# so the save could not be overwritten without retyping. The first letter typed now
# replaces the automatic name, as in a selected field.
#
# The automatic name carries the wall-clock second, so the arms sleep between sessions.
# Tier: big
TESTNAME=save_auto_name_refresh
. "$(dirname "$0")/lib.sh"
precheck
need_save

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

arm_dir() { # arm_dir <name>
    LBA2_USER_DIR="$tmp/$1"
    export LBA2_USER_DIR
    save_dir="$LBA2_USER_DIR/save"
    mkdir -p "$save_dir"
}

# The save files, one per line, minus the screenshot folder.
saves() {
    local f
    for f in "$save_dir"/*; do
        [ -e "$f" ] || continue
        f="${f##*/}"
        [ "$f" = "shoot" ] || printf '%s\n' "$f"
    done
}

# The name stored inside a save: after a version byte and a 4-byte cube number.
name_in() { tail -c +6 "$1" | LC_ALL=C tr '\0' '\n' | head -1; }

# F2 (scancode 59) opens the save list; the cursor starts on New when the loaded save is
# not in the list, so Up selects the one save there. Delays are input polls.
NEW='key 59 2; key enter 6 60'
OVERWRITE='key 59 2; key up 6 60; key enter 6 120; key enter 6 220'
TYPE_ABC='key 59 2; key enter 6 60; key 4 6 400; key 5 6 500; key 6 6 600; key enter 6 800'
OVERTYPE_ABC='key 59 2; key up 6 60; key enter 6 120; key 4 6 400; key 5 6 500; key 6 6 600; key enter 6 800'

session() { # session <input_keyboard> <keys>
    ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --exec "input_keyboard $1" \
        --exec-at 20 "$2" --tick 300 --exit >/dev/null 2>&1 ||
        fail "$arm: session exited non-zero ($?); a hang here is the name editor refusing Enter"
}

# Overwrites an automatically named save and checks it was named afresh.
refresh_arm() { # refresh_arm <arm> <input_keyboard for the overwrite>
    arm="$1"
    arm_dir "$arm"
    session 0 "$NEW"
    first="$(saves)"
    case "$first" in
    *" - "[0-9][0-9][0-9][0-9]-*.LBA) ;;
    *) fail "$arm: the first save is not automatically named: '$first'" ;;
    esac
    sleep 2
    session "$2" "$OVERWRITE"
    after="$(saves)"
    [ "$(printf '%s\n' "$after" | wc -l | tr -d ' ')" = 1 ] ||
        fail "$arm: expected one save after overwriting, found: $(printf '%s' "$after" | tr '\n' '|')"
    [ "$after" != "$first" ] ||
        fail "$arm: the overwritten save kept its old name '$first'"
    [ "$(name_in "$save_dir/$after").LBA" = "$after" ] ||
        fail "$arm: file '$after' holds the name '$(name_in "$save_dir/$after")'"
}

refresh_arm pad 0
refresh_arm keyboard 1

# Typing over an automatic name replaces it, and the old file goes.
arm=overtype
arm_dir overtype
session 0 "$NEW"
session 1 "$OVERTYPE_ABC"
[ "$(saves)" = "abc.LBA" ] ||
    fail "overtype: typing over an automatic name left: $(saves | tr '\n' '|')"

# Control: a typed name stays when the save is overwritten without a keyboard.
arm=typed
arm_dir typed
session 1 "$TYPE_ABC"
[ "$(saves)" = "abc.LBA" ] || fail "typed: the keyboard session saved '$(saves)', not abc.LBA"
before="$(stat -f %m "$save_dir/abc.LBA" 2>/dev/null || stat -c %Y "$save_dir/abc.LBA")"
sleep 2
session 0 "$OVERWRITE"
[ "$(saves)" = "abc.LBA" ] || fail "typed: a typed name changed on overwrite: '$(saves)'"
now="$(stat -f %m "$save_dir/abc.LBA" 2>/dev/null || stat -c %Y "$save_dir/abc.LBA")"
[ "$now" != "$before" ] || fail "typed: abc.LBA was not rewritten, so the overwrite never ran"

pass "automatic names refreshed with and without a keyboard, typed over; typed name kept"
