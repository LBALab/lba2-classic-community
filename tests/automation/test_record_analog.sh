#!/usr/bin/env bash
# The analog camera survives a recording: mouse and right stick both round-trip.
#
# The recorder carries values that never reach TabKeys -- the mouse delta pair and
# Click, the right stick, and the pad's first-pressed scancode -- because none of them
# has a scancode to be carried as. Everything else in a recording rides the key table,
# so those twenty bytes are the only part of the format that no keyboard session
# exercises, and until the two devices could be driven from the console no session
# exercised them at all. A block that is never filled is a block that is never checked.
#
# Two properties per device, because each catches what the other cannot:
#
#   The file carries the input, at the poll it happened. A recording samples device
#   state at the input poll, which sits after the frame's events are pumped and before
#   ManageMouse drains them, so the mouse pair to sample is the motion still pending and
#   not the motion delivered last time. Sampling the delivered one records every
#   movement a poll late and replays the orbit a frame behind the session.
#
#   The file beats a live device. A replay run whose own mouse or stick is moving has to
#   reproduce the recording, not the room. Without this the test cannot tell a replayed
#   sample from the recorded `mouse` or `stick` command being re-executed: both put the
#   same values in the same place, so a replay that dropped the analog block entirely
#   would still pass. For the stick the live one lets go early, which is what isolates
#   the pad-presence half: the analog camera asks whether a pad is there before it reads
#   the axes, and the polls after the live stick centres are the only ones where that
#   answer can come from nowhere but the file.
#
# Runs in an exterior, where the Auto camera and its analog orbit live. The default
# config wants the right button held for the drag orbit, which is why the injected
# button mask matters as much as the motion.
TESTNAME="record_analog"
. "$(dirname "$0")/lib.sh"
precheck

# The camera fixtures' save: island 6 cube 97, an exterior.
SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$SAVE" ] || skip "fixture missing: $SAVE"
case "$(ctl_headless --exec 'help mouse' --tick 2 --exit 2>/dev/null)" in
*"Unknown help topic"*) skip "binary predates the \`mouse\` console command" ;;
esac

rec="$(user_dir)/analog.rec"

# The Auto camera off a run of its own, not off the recording's --exec. `cam_follow` is
# a cvar, so setting it writes it back: a recording run that switched it on would boot
# with it off and the replay would boot with it on, and the two disagree from tick 0
# about a value the digest covers. Settling it first leaves both runs booting the same.
ctl --fixed-dt 16 --load "$SAVE" --exec "cam_follow 1" --tick 2 --exit >/dev/null 2>&1 ||
    fail "could not switch the Auto camera on ($?)"

record() { # record <label> <driving command>
    rm -f "$rec" "$rec".lba "$rec".end.lba
    ctl --fixed-dt 16 --load "$SAVE" --record "$rec" \
        --exec "skipmodals 1" --exec-at 40 "$2" --tick 200 --exit \
        >/dev/null 2>&1 || fail "$1: recording run exited non-zero ($?): hang or crash"
    [ -s "$rec" ] || fail "$1: no recording written to $rec"
}

# The dump prints its summary line first whatever mode it is asked for, so take the
# per-poll lines by their own prefix rather than by position.
CARRIED=""
carried() { # carried <label> <expected tail of the first poll line>
    local analog
    analog="$(python3 "$REPO/scripts/dev/dump_recording.py" "$rec" analog | grep '^poll ')" ||
        fail "$1: the recording carries no analog polls at all"
    CARRIED="$(printf '%s\n' "$analog" | grep -c . || true)"
    [ "$CARRIED" -ge 50 ] ||
        fail "$1: the recording carries $CARRIED analog polls; 60 polls of input went in"
    case "$(printf '%s\n' "$analog" | head -1)" in
    *"$2") ;;
    *) fail "$1: first analog poll is '$(printf '%s\n' "$analog" | head -1)'; want it to end '$2'" ;;
    esac
}

# Reports through CHECKED rather than stdout, and is called directly rather than in a
# command substitution: `fail` ends the shell it runs in, which inside $( ) is only the
# subshell, and the caller carries on and prints over the top of the failure.
CHECKED=""
replay() { # replay <label> [extra replay args...]
    local label="$1" out summary
    shift
    out="$(ctl --fixed-dt 16 --load "$SAVE" --replay "$rec" --tick 300 "$@" --exit 2>&1)" ||
        fail "$label: replay run exited non-zero ($?): hang or crash"
    case "$out" in
    *"consistency failure"*)
        fail "$label: $(printf '%s\n' "$out" | grep -m1 'consistency failure')" ;;
    esac
    summary="$(printf '%s\n' "$out" | grep -m1 'replay ended')" ||
        fail "$label: replay printed no summary; it cannot be said to have matched"
    case "$summary" in
    *"first hash mismatch -1"*) ;;
    *) fail "$label: $summary" ;;
    esac
    CHECKED="$(printf '%s\n' "$summary" | sed -n 's/.*: \([0-9]*\) ticks checked.*/\1/p')"
    [ -n "$CHECKED" ] && [ "$CHECKED" -gt 100 ] ||
        fail "$label: only ${CHECKED:-0} ticks checked, so the oracle barely ran"
}

# --- the mouse ---------------------------------------------------------------------
# 20 pixels a poll for 60 polls with the right button down: past the 2px dead zone by
# enough that the orbit is unmistakable, and short of anything that would wrap. A poll
# late reads mdx 0, which is what the first line is checked for.
record "mouse" "mouse 20 0 60"
carried "mouse" "mdx 20 mdy 0 click 2"
mousepolls="$CARRIED"
replay "mouse"
replay "mouse contested" --exec-at 40 "mouse -60 40 60"

# --- the right stick ---------------------------------------------------------------
# Well past JoystickDeadzone, and a live one that lets go after 20 of the file's 60
# polls: from there on the only thing that can report a pad present is the replay.
record "stick" "stick 24000 0 60"
carried "stick" "rsx 24000 rsy 0 padfirst 0 mdx 0 mdy 0 click 0"
stickpolls="$CARRIED"
replay "stick"
replay "stick contested" --exec-at 40 "stick -30000 12000 20"

pass "mouse $mousepolls polls, stick $stickpolls polls, $CHECKED ticks matched each, and the file beat a live device"
