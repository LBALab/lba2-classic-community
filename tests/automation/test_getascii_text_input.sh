#!/usr/bin/env bash
# Typed characters come from the engine's polled sample, so an injected key reaches
# the screens that read them.
#
# GetAscii (LIB386/SYSTEM/KEYBOARD.CPP) answers with the character behind the poll's
# first held key. Two screens in the shipping binary read it -- save-name entry
# (InputPlayerName, SOURCES/GAMEMENU.CPP) and cheat-code entry (GereCheatCode,
# SOURCES/CHEATCOD.CPP) -- and neither could be driven by anything that is not a
# physical keyboard, because the answer used to come from SDL rather than from the
# poll every other reader is on. A recording is one of those things: a replay
# restores the poll and never touches SDL's keyboard, so a replay that reached the
# name screen waited there for a key that could not arrive.
#
# Two properties, and the first is the one that makes the second worth having:
#
#   1. The name screen terminates at all. It ends on a keystroke, so a run that
#      cannot deliver one runs until something kills it. Every arm here is bounded
#      by the harness timeout, and an unfixed engine reaches it.
#   2. What was typed is what lands: the save is written under the typed name, and
#      a replay of the recorded session writes the same one.
#
# The keyboard gate is forced rather than earned. ChoosePlayerName offers typed
# entry only when the last input came from a physical keyboard, a fact set by a real
# SDL key event alone and carried in no recording; under --headless it is never set,
# so without `input_keyboard 1` this fixture would silently exercise the datetime
# branch instead. Forcing it tests the fix rather than the path a player walks to
# reach it -- which is the honest description of what an automated arm can do here.
#
# Local-only (needs retail data and a save fixture). Not in host_quick CI. The
# same function's contract is covered without an engine by tests/getascii.
TESTNAME=getascii_text_input
. "$(dirname "$0")/lib.sh"
precheck
need_save

save_dir="$(user_dir)/save"

# Scancodes rather than names, because `key` names no letters: a=4, b=5, c=6, and
# F2 (59) is I_SAVE's first binding (SOURCES/INPUT_BINDINGS.CPP). The delays are
# input polls, which is what makes them stable: the modals iterate once per present
# under a pinned step, so the same delay lands in the same place every run.
#
# Enter twice: once to take the "New Game" slot out of the list, once to commit the
# name. Between them, three letters with a poll's gap each.
TYPE_ABC='key 59 2; key enter 6 60; key 4 6 400; key 5 6 500; key 6 6 600; key enter 6 800'

rec="$(user_dir)/getascii.rec"

# --- Arm 1: a driven keyboard reaches the name screen, and the name lands. -------
rm -rf "$save_dir"
ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --record "$rec" \
    --exec "input_keyboard 1" --exec-at 20 "$TYPE_ABC" \
    --tick 200 --exit >/dev/null 2>&1 ||
    fail "recording run exited non-zero ($?) — the name screen never took a key"

[ -f "$save_dir/abc.LBA" ] ||
    fail "no save named for what was typed; the folder holds: $(ls "$save_dir" 2>/dev/null | tr '\n' ' ')"

# --- Arm 2: the recording replays, and writes the same name. ---------------------
# A harness-driven recording carries its input twice, as polls and as the console
# commands that scheduled them, so this arm says the replay reaches the same place
# rather than that the poll stream alone carried it. What it does cover, and what no
# other arm does, is that the screen appears in the stream at all: it produced no
# polls whatsoever before, so a replay had nothing to hand back however it was
# driven.
[ -s "$rec" ] || fail "no recording written to $rec"

rm -rf "$save_dir"
out="$(ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$rec" --tick 400 --exit 2>&1)" ||
    fail "replay run exited non-zero ($?) — the replay stalled on the name screen"

case "$out" in
*"consistency failure"*)
    fail "$(printf '%s\n' "$out" | grep -m1 'consistency failure')"
    ;;
esac

summary="$(printf '%s\n' "$out" | grep -m1 'replay ended')" ||
    fail "replay printed no summary; it cannot be said to have matched"
case "$summary" in
*"first hash mismatch -1"*) ;;
*) fail "$summary" ;;
esac

[ -f "$save_dir/abc.LBA" ] ||
    fail "the replay wrote a different name: $(ls "$save_dir" 2>/dev/null | tr '\n' ' ')"

# --- Arm 3: the other reader, the cheat codes. -----------------------------------
# GereCheatCode runs from the in-game menu, so F4 (I_OPTIONS) opens it and Esc closes
# it. b=5, o=18, x=27 spell the clover-box cheat, which raises ListVarGame[251]; the
# variable is zeroed first so the check cannot pass on whatever the save carried.
# Read with `vargame`, which prints; a bool cvar with no argument toggles instead of
# printing, which is a way to write this arm so that it reports the opposite.
rm -rf "$save_dir"
cheat="$(ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" \
    --exec "vargame 251 0" \
    --exec-at 20 "key 61 2; key 5 4 40; key 18 4 60; key 27 4 80; key esc 4 200" \
    --exec-at 40 "vargame 251" --tick 200 --exit 2>&1)" ||
    fail "cheat run exited non-zero ($?)"

after="$(printf '%s\n' "$cheat" | grep 'vargame\[251\]' | tail -1 |
    sed -n 's/.*= \([0-9][0-9]*\).*/\1/p')"
case "$after" in
'' | *[!0-9]*) fail "no readable vargame[251] line: $(printf '%s\n' "$cheat" | tail -3)" ;;
esac
[ "$after" -ne 0 ] || fail "typing the cheat in the menu changed nothing (vargame[251] still $after)"

pass "typed name lands live and on replay; cheat entry reaches vargame[251] = $after"
