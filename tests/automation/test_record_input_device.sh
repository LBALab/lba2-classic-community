#!/usr/bin/env bash
# A recording carries which device the player was last on, so a replay saves under the
# name the session typed.
#
# ChoosePlayerName (SOURCES/GAMEMENU.CPP) offers a text field when the last input came
# from a physical keyboard and an "<island> - <datetime>" stem otherwise. That fact lived
# only in LastInputWasKeyboard, set by a real SDL key-down and written into no recording,
# so a replay of a session that typed a save name took the other branch and wrote a
# different file. Nothing reported it: a save slot is in none of the categories the
# per-tick digest hashes, so the run diverged and still exited 0. Every arm below runs to
# completion on a broken engine too -- what changes is the name in the save folder, which
# is why each arm reads the folder rather than the exit status.
#
# The format carries it in two halves and both are load-bearing. The header's
# `input.keyboard=` is the value the session started on; a 0x52 record marks each later
# change. The header alone freezes a session that flipped part-way; the records alone
# leave a session that never flipped -- a player who used the keyboard from boot to quit,
# which is most of them -- carrying nothing at all.
TESTNAME=record_input_device
. "$(dirname "$0")/lib.sh"
precheck
need_save
command -v python3 >/dev/null 2>&1 || skip "no python3 (needed to read a recording back)"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# Sets LBA2_USER_DIR rather than echoing one: an export inside a command substitution
# lands in the subshell and dies there. Every arm wants an empty save folder, and that is
# not something to arrange by deleting a folder somebody else named.
arm_dir() { # arm_dir <name>
    LBA2_USER_DIR="$tmp/$1"
    export LBA2_USER_DIR
    save_dir="$LBA2_USER_DIR/save"
    mkdir -p "$save_dir"
}

# What the save folder holds, minus the screenshot folder the engine makes for itself.
# A glob rather than `ls`, so a name with a space in it stays one entry -- which the
# datetime branch produces on every run that takes it.
saves() {
    local f out=""
    for f in "$save_dir"/*; do
        [ -e "$f" ] || continue
        f="${f##*/}"
        [ "$f" = "shoot" ] && continue
        out="$out$f "
    done
    printf '%s' "$out"
}

# Scancodes, because `key` names no letters: a=4, b=5, c=6, and F2 (59) is I_SAVE's first
# binding. Enter twice -- once to take the "New Game" slot, once to commit the name --
# with a poll's gap between the three letters. The delays are input polls, which is what
# makes them stable under a pinned step.
TYPE_ABC='key 59 2; key enter 6 60; key 4 6 400; key 5 6 500; key 6 6 600; key enter 6 800'

# --- Arm 1: the writer puts both halves in the file, and adds no drift. ---------------
# Recording from boot, so the session starts before the cvar is set and the flag flips
# during it: the header must hold the starting 0 and the stream a record for the flip.
#
# Four flips rather than one, spread far enough apart to fall either side of a sync
# marker (one every 64 polls). A record between a marker and the poll behind it is read
# through a different path from one between two polls -- the dispatch runs before the
# marker is consumed and the step-over chain runs after -- and one flip exercises
# neither reliably.
arm_dir writer
rec="$tmp/writer.rec"
ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --record "$rec" \
    --exec "input_keyboard 1" --exec-at 20 "input_keyboard 0" \
    --exec-at 60 "input_keyboard 1" --exec-at 120 "input_keyboard 0" \
    --exec-at 160 "$TYPE_ABC" --tick 320 --exit >/dev/null 2>&1 ||
    fail "writer: recording run exited non-zero ($?)"

start="$(head -c 4096 "$rec" | tr -d '\0' | sed -n 's/^input\.keyboard=//p')"
[ "$start" = "0" ] ||
    fail "writer: header says input.keyboard=${start:-absent}, the session started on 0"

counts="$(python3 "$REPO/scripts/dev/dump_recording.py" "$rec" | grep -m1 '^polls=')" ||
    fail "writer: could not read the recording back"
devices="$(printf '%s\n' "$counts" | sed 's/.*devices=\([0-9]*\).*/\1/')"
[ "${devices:-0}" -ge 4 ] ||
    fail "writer: the stream carries ${devices:-0} device records, the session flipped 4 times ($counts)"

# And the records cost the stream nothing. A poll is a unit of the recording, so anything
# that shifts how many polls fall between two ticks moves every actor's animation anchor
# -- which is how the last change to this path broke every recording that had a menu in
# it, without adding a poll at all. These records sit beside polls rather than among
# them, and a clean replay of a stream full of them is what says so.
arm_dir writer_replay
wout="$(ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$rec" --tick 600 --exit 2>&1)" ||
    fail "writer: replay run exited non-zero ($?) — hang or crash"
case "$wout" in
*"first hash mismatch -1"*) ;;
*) fail "writer: $(printf '%s\n' "$wout" | grep -m1 'replay ended') — a stream carrying device records did not replay clean" ;;
esac

# --- Arm 2: the reader takes it from the file and from nothing else. ------------------
# The cvar is set BEFORE the recording starts, so the console command that set it is not
# in the stream: `rec start` opens the file after it has already run. The header field is
# then the only thing in the recording that says a keyboard was in use, and the replay
# has to reach the text branch from that alone.
arm_dir header_record
rec2="$tmp/header.rec"
ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" \
    --exec "input_keyboard 1" --exec-at 10 "rec start $rec2" \
    --exec-at 30 "$TYPE_ABC" --tick 260 --exit >/dev/null 2>&1 ||
    fail "header: recording run exited non-zero ($?)"
[ -s "$rec2" ] || fail "header: no recording written to $rec2"
[ "$(saves)" = "abc.LBA " ] ||
    fail "header: the live session did not write the typed name; it wrote: $(saves)"

# Nothing but the header may carry the fact, or this arm proves nothing about it.
case "$(python3 "$REPO/scripts/dev/dump_recording.py" "$rec2" cmds)" in
*input_keyboard*)
    fail "header: the stream carries an input_keyboard command, so a replay could reach the text branch without the header field"
    ;;
esac

arm_dir header_replay
ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$rec2" --tick 400 --exit >/dev/null 2>&1 ||
    fail "header: replay run exited non-zero ($?)"
[ "$(saves)" = "abc.LBA " ] ||
    fail "header: the replay saved as '$(saves)' rather than abc.LBA — it took the datetime branch, so the device did not come out of the recording"

# --- Arm 3: the control, and the operator does not get a vote. ------------------------
# Arm 2 passing means little on its own: a run that reached the name screen and typed
# would write abc.LBA whatever the flag did, if the flag were being ignored and the text
# branch taken always. This is the same recipe with the cvar left alone, and it must come
# out the other way -- which is also what the defect looked like before the field existed.
#
# The replay side then sets the cvar to 1, which is the second thing this arm is for. A
# recording says 0 and the live run says 1, and the recording has to win: the flag is
# maintained from SDL events that keep arriving during playback, so a value applied once
# would be whoever touched the keyboard last. That is the whole defect in its original
# form -- a replay that saved under a different name depending on who was in the room --
# and it is the only arm here where the two sources disagree.
arm_dir control_record
rec3="$tmp/control.rec"
ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" \
    --exec-at 10 "rec start $rec3" --exec-at 30 "$TYPE_ABC" --tick 260 --exit >/dev/null 2>&1 ||
    fail "control: recording run exited non-zero ($?)"
[ -s "$rec3" ] || fail "control: no recording written to $rec3"

arm_dir control_replay
cout="$(ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$rec3" \
    --exec "input_keyboard 1" --exec-at 380 "input_keyboard" --tick 400 --exit 2>&1)" ||
    fail "control: replay run exited non-zero ($?)"

# And the borrowed slot is given back. The recording is spent long before tick 380, so
# by then the flag should hold the 1 this run set at boot rather than the 0 the file
# ended on -- a replay installs the device the way it installs the binding tables, and
# owes it back the same way.
#
# Read by toggling, because a bool cvar with no argument does not print the value, it
# inverts it and prints the result. So `= 0` here means the flag was 1, which is the
# pass. Written out because reading it as the value is the obvious mistake and it
# reverses the verdict.
back="$(printf '%s\n' "$cout" | grep -oE 'input_keyboard = [01]' | tail -1)"
[ "$back" = "input_keyboard = 0" ] ||
    fail "control: after the replay the flag read '${back:-nothing}' — inverted, that is not the 1 this run set before the replay borrowed it"

case "$(saves)" in
abc.LBA*)
    fail "control: the replay saved as abc.LBA — either arm 2 is not measuring the device flag, or the live run's keyboard beat the recording's"
    ;;
"")
    fail "control: the replay saved nothing; it never reached the name screen, so neither arm means anything"
    ;;
esac

# --- Arm 4: a recording made before the field existed still replays as it did. --------
# recordings/legacy-v13.rec was captured by the engine of the commit before this, and a
# same-binary round trip cannot see a change that breaks the writer and the reader
# together. It reaches the name screen by setting the cvar from the console, so the
# command record in the stream is what carries the keyboard there. A replay that forced
# the flag to a default because the file did not name one would stamp on that command and
# write the datetime name: this arm is the one that says it does not.
legacy="$REPO/tests/automation/recordings/legacy-v13.rec"
[ -f "$legacy" ] || fail "the legacy recording is missing from $REPO/tests/automation/recordings"

arm_dir legacy
lout="$(ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$legacy" --tick 300 --exit 2>&1)" ||
    fail "legacy: replay run exited non-zero ($?) — hang or crash"
case "$lout" in
*"is format"*) fail "legacy: $(printf '%s\n' "$lout" | grep -m1 'is format')" ;;
esac
case "$lout" in
*"first hash mismatch -1"*) ;;
*) fail "legacy: $(printf '%s\n' "$lout" | grep -m1 'replay ended'); $(printf '%s\n' "$lout" |
        grep 'mode differs' | tr '\n' ';' || echo 'no mode line differed, so this is the format path')" ;;
esac
[ "$(saves)" = "abc.LBA " ] ||
    fail "legacy: the replay saved as '$(saves)' rather than abc.LBA — a recording that predates the field has stopped doing what it did"

pass "device carried in the header and in the stream; older recordings unchanged"
