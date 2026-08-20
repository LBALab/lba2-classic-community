#!/usr/bin/env bash
# The analog camera survives a recording: mouse motion and buttons round-trip.
#
# The recorder carries three values that never reach TabKeys -- the mouse delta pair
# and Click, the right stick, and the pad's first-pressed scancode -- because none of
# them has a scancode to be carried as. Everything else in a recording rides the key
# table, so those twenty bytes are the only part of the format that no keyboard session
# exercises, and until the mouse could be driven from the console no session exercised
# them at all. A block that is never filled is a block that is never checked.
#
# Two properties, because each catches what the other cannot:
#
#   The file carries the motion, at the poll it happened. A recording samples device
#   state at the input poll, which sits after the frame's events are pumped and before
#   ManageMouse drains them, so the pair to sample is the motion still pending and not
#   the motion delivered last time. Sampling the delivered one records every movement a
#   poll late and replays the orbit a frame behind the session.
#
#   The file beats a live device. A replay run whose own mouse is moving the other way
#   has to reproduce the recording, not the room. Without this the test cannot tell a
#   replayed sample from the recorded `mouse` command being re-executed: both put the
#   same motion in the same place, so a replay that dropped the analog block entirely
#   would still pass.
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
rm -f "$rec" "$rec".lba "$rec".end.lba

# The Auto camera off a run of its own, not off the recording's --exec. `cam_follow` is
# a cvar, so setting it writes it back: a recording run that switched it on would boot
# with it off and the replay would boot with it on, and the two disagree from tick 0
# about a value the digest covers. Settling it first leaves both runs booting the same.
ctl --fixed-dt 16 --load "$SAVE" --exec "cam_follow 1" --tick 2 --exit >/dev/null 2>&1 ||
    fail "could not switch the Auto camera on ($?)"

# 20 pixels a poll for 60 polls with the right button down: past the 2px dead zone by
# enough that the orbit is unmistakable, and short of anything that would wrap.
ctl --fixed-dt 16 --load "$SAVE" --record "$rec" \
    --exec "skipmodals 1" --exec-at 40 "mouse 20 0 60" --tick 200 --exit \
    >/dev/null 2>&1 || fail "recording run exited non-zero ($?): hang or crash"
[ -s "$rec" ] || fail "no recording written to $rec"

# --- the file carries it -----------------------------------------------------------
# The dump prints its summary line first whatever mode it is asked for, so take the
# per-poll lines by their own prefix rather than by position.
analog="$(python3 "$REPO/scripts/dev/dump_recording.py" "$rec" analog | grep '^poll ')" ||
    fail "the recording carries no analog polls at all"
count="$(printf '%s\n' "$analog" | grep -c . || true)"
[ "$count" -ge 50 ] ||
    fail "the recording carries $count analog polls; 60 polls of mouse motion went in"

# The first one is the alignment check. It has to hold the motion, not a zero left over
# from reading the field ManageMouse had already drained.
first="$(printf '%s\n' "$analog" | head -1)"
case "$first" in
*"mdx 20 mdy 0 click 2"*) ;;
*) fail "first analog poll is '$first'; want mdx 20 mdy 0 click 2 (a poll late reads mdx 0)" ;;
esac

# --- it replays ---------------------------------------------------------------------
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
}

replay "plain"
checked="$CHECKED"
[ -n "$checked" ] && [ "$checked" -gt 100 ] ||
    fail "only ${checked:-0} ticks checked, so the oracle barely ran"

# The same recording against a mouse dragging the other way over the same polls. The
# recorded motion has to win, or the replay is reproducing the room.
replay "contested" --exec-at 40 "mouse -60 40 60"
[ "$CHECKED" = "$checked" ] ||
    fail "contested replay checked $CHECKED ticks, plain checked $checked"

pass "analog round-trip: $count polls of mouse motion, $checked ticks matched, and the file beat a live mouse"
