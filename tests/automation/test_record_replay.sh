#!/usr/bin/env bash
# Session recorder: a recording replays into the same simulation it captured.
#
# The recorder carries its own oracle. Every tick it stores an FNV-1a digest of scene,
# hero, camera, the other actors and all 336 script variables, and a replay recomputes
# that digest and says the first tick where the two stop matching. So this test does
# not need goldens or a dump to compare: it records a session, replays it, and asks the
# engine whether it reproduced.
#
# Two runs, because the interesting failures are asymmetric. `--tick` present is the
# harness-driven case every fixture uses; `--tick` absent is the case a player takes,
# and it has its own history: with no tick budget the run used to finalize on its first
# tick and disarm the hook that steps the fixed-dt clock, so a tick that presented
# nothing banked no time and the recording carried a stall its replay never reproduced.
# A suite that only ever ran the bounded form could not see that.
#
# --fixed-dt is not a convenience here. A recording made on a host-sampled clock does
# not replay exactly (docs/plan/RECORDING_RESEARCH.md), so the pinned step is part of
# what is under test rather than a way to make the test quicker.
#
# One arm is not a record-and-replay at all. recordings/legacy-v10.rec was captured by
# an older engine, in a format this build reads and does not write, and is replayed as it
# stands: a same-binary round trip cannot see a change that breaks the writer and the
# reader together.
TESTNAME="record_replay"
. "$(dirname "$0")/lib.sh"
precheck
need_save
# Three arms read the recording back with the reader that needs no engine. A box without
# an interpreter is not a recording defect, so skip rather than fail, as lib.sh does for
# its own python steps.
command -v python3 >/dev/null 2>&1 || skip "no python3 (needed to read a recording back)"

rec="$(user_dir)/session.rec"

# The replay must not be told anything the recording did not carry, so both runs get
# the same --load and the recording supplies the rest.
#
# Reports through CHECKED rather than stdout, and is called directly rather than in a
# command substitution, because `fail` ends the shell it runs in: inside $( ) that is
# the subshell, and the caller carries on to print PASS over the top of a failure.
CHECKED=""
record_and_replay() { # record_and_replay <label> [extra record args...]
    local label="$1"
    shift

    # The siblings too, which nothing writes: a recording is one file, and the check
    # below would otherwise report a leftover from another build as this run having
    # written one.
    rm -f "$rec" "$rec".lba "$rec".end.lba

    ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --record "$rec" \
        --exec-at 30 "key up 60 2" "$@" \
        >/dev/null 2>&1 ||
        fail "$label: recording run exited non-zero ($?) — hang or crash"

    [ -s "$rec" ] || fail "$label: no recording written to $rec"

    # One file, and it carries the savegames at both ends of the session. Both halves
    # are checked because either alone passes for the wrong reason: a recorder that
    # stopped writing snapshots altogether would leave no siblings either, and a
    # recorder that wrote them beside the file as well would still carry them inside.
    for sibling in "$rec".lba "$rec".end.lba; do
        if [ -e "$sibling" ]; then
            fail "$label: a recording is one file, but $sibling was written"
        fi
    done
    local saves
    saves="$(python3 "$REPO/scripts/dev/dump_recording.py" "$rec" |
        grep -m1 '^savegames:')" ||
        fail "$label: could not read the recording back"
    case "$saves" in
    *"start none"*) fail "$label: the recording carries no start savegame ($saves)" ;;
    *"end none"*) fail "$label: the recording carries no end savegame ($saves)" ;;
    esac

    # The header has to declare the arithmetic the session ran on, because that is what
    # lets a replay on another platform open by naming what it disagrees about instead
    # of diverging in the middle. A missing line is silent: the reader treats an absent
    # field as nothing to compare, so the warning disappears rather than failing.
    # tr drops the NULs before the substitution sees them: the header is text but the
    # records after it are not, and bash warns and discards on a null byte in command
    # substitution, which puts a warning in the suite output for every run.
    local head
    head="$(head -c 2048 "$rec" | tr -d '\0')"
    case "$head" in
    *"numeric.rng="*) ;;
    *) fail "$label: the header does not declare numeric.rng" ;;
    esac
    case "$head" in
    *"numeric.long_double_bits="*) ;;
    *) fail "$label: the header does not declare numeric.long_double_bits" ;;
    esac
    # Whether audio came up is a simulation input rather than a presentation detail: the
    # ambience pan is drawn only `if (!IsSamplePlaying(...))`, from the one stream actor
    # behaviour draws from, and a dialogue with the text off runs until the voice sample
    # ends. Asserted as 0 rather than merely present, because every run in this file is
    # headless and a line that reported the flag instead of the driver would say so.
    case "$head" in
    *"mode.audio=0"*) ;;
    *) fail "$label: the header does not declare mode.audio=0, and this run is headless" ;;
    esac

    # More ticks than the recording holds, so the stream runs out and the summary
    # prints. Ending on --tick first means no summary at all, and a run that reported
    # nothing reads exactly like a run that passed.
    local out
    out="$(ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$rec" --tick 400 --exit 2>&1)" ||
        fail "$label: replay run exited non-zero ($?) — hang or crash"

    case "$out" in
    *"consistency failure"*)
        fail "$label: $(printf '%s\n' "$out" | grep -m1 'consistency failure')"
        ;;
    esac

    # A replay that never checked a tick is not a replay that matched. The summary is
    # the only line that says how many were compared.
    local summary
    summary="$(printf '%s\n' "$out" | grep -m1 'replay ended')" ||
        fail "$label: replay printed no summary; it cannot be said to have matched"

    local checked
    checked="$(printf '%s\n' "$summary" | sed -n 's/.*: \([0-9]*\) ticks checked.*/\1/p')"
    [ -n "$checked" ] && [ "$checked" -gt 100 ] ||
        fail "$label: only ${checked:-0} ticks checked — the oracle barely ran ($summary)"

    case "$summary" in
    *"first hash mismatch -1"*) ;;
    *) fail "$label: $summary" ;;
    esac

    CHECKED="$checked"
}

record_and_replay "with --tick" --tick 300 --exit
bounded="$CHECKED"

# Unbounded: the player's form. Nothing ends the run, so the recording is stopped by a
# console command on a tick of its own and the run quits there. That command only
# arrives if the tick hook is still armed, which is the property this arm exists for:
# a run that finalizes on its first tick never reaches tick 300 and times out here.
record_and_replay "without --tick" --exec-at 300 "rec stop; exit"
unbounded="$CHECKED"

# A recording cut off mid-snapshot, which is what a process killed while writing one
# leaves behind. The frame's refusal is the whole safety argument for keeping the
# savegames inside the file, and tests/record_format proves it over a buffer through a
# reader written for the test. This drives the engine's own reader over a real file, so
# the two cannot drift: cut inside the start chunk, the replay has to say so and check
# nothing, rather than hand the save loader a savegame that stops early.
torn="$(user_dir)/torn.rec"

# Two ways a chunk fails to close. `cut` stops inside the payload, which is what a
# process killed mid-write leaves, and the reader has several ways to notice it: the
# short payload, the missing tail, and the tail comparison all refuse it, so this arm
# shows the behaviour rather than isolating one check. `magic` keeps every byte and
# damages the word behind them, which nothing but the tail comparison can see -- that
# one fails the moment the comparison is removed, which is what makes it the oracle for
# the claim the single-file layout rests on.
damage_recording() { # damage_recording <how> <src> <dst>
    python3 - "$1" "$2" "$3" <<'EOF'
import struct, sys
how, src, dst = sys.argv[1], sys.argv[2], sys.argv[3]
d = open(src, "rb").read()
at = d.find(b"\n\n") + 2
length = struct.unpack_from("<I", d, at + 1)[0]
if how == "cut":
    out = d[:at + 5 + length // 2]
else:
    out = bytearray(d)
    out[at + 5 + length + 7] ^= 0xFF  # the magic's top byte
open(dst, "wb").write(bytes(out))
EOF
}

for how in cut magic; do
    damage_recording "$how" "$rec" "$torn" || fail "torn/$how: could not build the file"

    tout="$(ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$torn" --tick 300 --exit 2>&1)" ||
        fail "torn/$how: replay run exited non-zero ($?) — hang or crash"

    case "$tout" in
    *"incomplete"*) ;;
    *)
        fail "torn/$how: a recording whose snapshot does not close was not refused; $(
            printf '%s\n' "$tout" | grep -m1 'replay ended' || echo 'the replay said nothing')"
        ;;
    esac
    # And refused means refused: a reader that reported the damage and then carried on
    # into the savegame would check ticks out of bytes that are not a stream.
    tchecked="$(printf '%s\n' "$tout" | sed -n 's/.*: \([0-9]*\) ticks checked.*/\1/p')"
    [ "${tchecked:-0}" = "0" ] ||
        fail "torn/$how: refused the snapshot and then checked $tchecked ticks anyway"
done
rm -f "$torn"

# The default home. A name with no directory in it is a name in <userDir>/recordings/,
# and the point of that is symmetry: the name a session was recorded under is the name it
# replays under, from whatever directory the run happens to start in. Recorded and replayed from
# a directory that holds no recording of its own, so a pass cannot come from the working
# directory answering instead of the folder.
bare="bare-name.rec"
recdir="$(user_dir)/recordings"
rm -f "$recdir/$bare"
# Removed by a trap rather than at the end of the arm: `fail` exits the shell, so every
# assertion below is a way out of here that never reaches an inline rm.
here="$(mktemp -d)"
trap 'rm -rf "$here"' EXIT

(cd "$here" && ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --record "$bare" --tick 200 --exit) \
    >/dev/null 2>&1 ||
    fail "bare name: recording run exited non-zero ($?) — hang or crash"

[ -s "$recdir/$bare" ] ||
    fail "bare name: nothing at $recdir/$bare; a name with no directory in it belongs there"
if [ -e "$here/$bare" ]; then
    fail "bare name: written to the working directory instead of $recdir"
fi

bareout="$(cd "$here" && ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$bare" \
    --tick 300 --exit 2>&1)" ||
    fail "bare name: replay run exited non-zero ($?) — hang or crash"

case "$bareout" in
*"first hash mismatch -1"*) ;;
*)
    fail "bare name: $(printf '%s\n' "$bareout" |
        grep -m1 -e 'replay ended' -e 'cannot open' || echo 'the replay said nothing')"
    ;;
esac

# A recording this build does not write.
#
# Every arm above records and replays with the same binary, which cannot catch a change
# that breaks both ends together -- and that is the class the format has already been
# bitten by. legacy-v10.rec is a format-10 session, captured once and never regenerated;
# tests/automation/recordings/README.md says why not.
#
# Two properties, and they fail differently. The tick count says the reader walked the
# stream correctly, which is what a record whose layout moved would break. The clean
# result says the sibling savegame was found and loaded, which is the only coverage the
# pre-single-file snapshot path has: measured, the same file with its .rec.lba moved
# away diverges at tick 0.
legacy="$REPO/tests/automation/recordings/legacy-v10.rec"
legacy_save="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
if [ ! -f "$legacy" ] || [ ! -f "$legacy".lba ]; then
    fail "the legacy recording is missing from $REPO/tests/automation/recordings"
fi
[ -f "$legacy_save" ] || skip "fixture save missing: $legacy_save"

lout="$(ctl --fixed-dt 16 --load "$legacy_save" --replay "$legacy" --tick 300 --exit 2>&1)" ||
    fail "legacy: replay run exited non-zero ($?) — hang or crash"

case "$lout" in
*"is format"*)
    fail "legacy: $(printf '%s\n' "$lout" | grep -m1 'is format')"
    ;;
esac

lsummary="$(printf '%s\n' "$lout" | grep -m1 'replay ended')" ||
    fail "legacy: the replay printed no summary; it cannot be said to have read the file"

lchecked="$(printf '%s\n' "$lsummary" | sed -n 's/.*: \([0-9]*\) ticks checked.*/\1/p')"
[ "$lchecked" = "198" ] ||
    fail "legacy: read ${lchecked:-0} ticks, the file holds 198 — the reader has lost the stream"

case "$lsummary" in
*"first hash mismatch -1"*) ;;
*)
    # Every differing mode line, not the first: a divergence from different retail data
    # reads exactly like a reader bug, and the line that says so can sit behind an
    # engine version that differs on any working tree.
    fail "legacy: $lsummary; $(printf '%s\n' "$lout" | grep 'mode differs' | tr '\n' ';' ||
        echo 'no mode line differed, so this is the format path')"
    ;;
esac

# Verbose telemetry. A plain recording carries one digest a tick, which can say that a
# tick stopped matching and never which of ~1300 values moved; --verbose stores them all.
# Recorded once and replayed twice: once expecting clean, and once with a variable
# deliberately changed part-way through. The second run is the point. A reporter that
# printed nothing would pass the first check exactly like one that works, and this is a
# diagnostic whose whole job is to be believed on the day something real diverges.
rm -f "$rec" "$rec".lba "$rec".end.lba

ctl --verbose --fixed-dt 16 --load "$LBA2_TEST_SAVE" --record "$rec" \
    --exec-at 30 "key up 60 2" --tick 300 --exit >/dev/null 2>&1 ||
    fail "verbose: recording run exited non-zero ($?) — hang or crash"

vout="$(ctl --verbose --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$rec" --tick 400 --exit 2>&1)" ||
    fail "verbose: replay run exited non-zero ($?) — hang or crash"
case "$vout" in
*"consistency failure"*)
    fail "verbose: $(printf '%s\n' "$vout" | grep -m1 'consistency failure')"
    ;;
esac

# Changing a game variable under the replay is a divergence the recording cannot know
# about, so the report has to come from comparing the stored values against the live
# ones. The check is on the output and not on the exit code, because measured, a replay
# that diverges under --tick still exits 0: the exit code carries the replay's verdict
# only on the path that has no tick budget (SOURCES/RECORD.CPP, Record_PollHook). The
# `|| true` is there for that path rather than this one.
bout="$(ctl --verbose --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$rec" --tick 400 \
    --exec-at 200 "vargame 77 42" --exit 2>&1 || true)"
case "$bout" in
*"var.game[77]"*) ;;
*)
    # Which half failed matters: a divergence reported but unnamed is the telemetry,
    # no divergence at all is the injection.
    fail "verbose: the replay did not name var.game[77]; $(printf '%s\n' "$bout" |
        grep -m1 -e 'consistency failure' -e 'state differs' || echo 'no divergence reported')"
    ;;
esac

# The other half of the mode.audio check above. That one shows the line is written; this
# shows the replay acts on it, which is the half that decides whether a recording made
# with sound is caught or reported as the simulation diverging. With no sample driver
# `IsSamplePlaying` is an unconditional no rather than a differently timed yes, so the
# two runs take different branches and the line is the only warning there is.
#
# Flipping the recorded answer is what shows the comparison can fail. One byte, so the
# stream behind the header is untouched and the replay still runs to the end.
flipped="$(user_dir)/audio-flipped.rec"
python3 - "$rec" "$flipped" <<'EOF' || fail "audio: could not flip the recorded audio state"
import sys
d = open(sys.argv[1], "rb").read()
n = d.replace(b"mode.audio=0", b"mode.audio=1", 1)
assert len(n) == len(d) and n != d, "the flip has to be one byte and has to change something"
open(sys.argv[2], "wb").write(n)
EOF

aout="$(ctl --verbose --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$flipped" \
    --tick 400 --exit 2>&1)" ||
    fail "audio: replay run exited non-zero ($?): hang or crash"
rm -f "$flipped"

case "$aout" in
*"mode differs: mode.audio=1"*) ;;
*)
    fail "audio: a recording made with sound replayed silently without a word; $(
        printf '%s\n' "$aout" | grep -m1 'mode differs' || echo 'no mode line differed at all')"
    ;;
esac

# Neither arm above can tell the engine writes anything but 0. Every other run in this
# file is headless, so `mode.audio=0` is also exactly what an accessor stuck at 0 would
# produce, and the flip is an edit to a file rather than something the engine said. This
# one opens a real sample device and asks for the other answer.
#
# Not `ctl`, which pins --headless, and --headless is the thing under test. SDL's dummy
# drivers give a device without needing a screen or a speaker. A box that will not start
# SDL with them is not a recording defect, so skip, and name the reason so a skip cannot
# quietly become the normal outcome.
withaudio="$(user_dir)/with-audio.rec"
rm -f "$withaudio"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    timeout "$LBA2_TEST_TIMEOUT" "$LBA2_BIN" --no-autosave --load "$LBA2_TEST_SAVE" \
    --record "$withaudio" --tick 60 --exit >/dev/null 2>&1 ||
    skip "SDL would not start under its dummy video and audio drivers, so no recording can be made with a device up"
[ -s "$withaudio" ] || skip "the run with a sample device wrote no recording"

grep -aq 'mode.audio=1' "$withaudio" ||
    fail "audio: a run with a sample device up still recorded mode.audio=0, so the line reports something other than the driver"

# The case the line exists for, end to end. Only the mode line is asserted: this
# recording pins no step, so what it diverges on afterwards is the clock rather than the
# audio, and the warning is the part that has to arrive either way.
wout="$(ctl --replay "$withaudio" --tick 200 --exit 2>&1 || true)"
rm -f "$withaudio"
case "$wout" in
*"mode differs: mode.audio=1"*) ;;
*)
    fail "audio: a recording made with a device up replayed headless without a word; $(
        printf '%s\n' "$wout" | grep -m1 'mode differs' || echo 'no mode line differed at all')"
    ;;
esac

# The loop a player can drive: record in one session, replay in the next, with nothing
# typed at either end and no flags at all.
#
# Two runs rather than one, and that is the whole point of the arm. In a single process
# `rec play` inherits a step the earlier `rec start` already pinned and a scene it is
# already in, so it cannot see either of the things this checks. A second process starts
# with neither, which is what a player does and what every check below turns on.
#
# No --fixed-dt. Every arm above pins the step on the command line, so none of them can
# see whether a session already in progress pins its own -- the difference between a
# recorder a harness drives and one a player can reach, since a player is already playing
# and cannot be sent back to a command line to relaunch.
#
# No path either. `rec start` names the session after the time of day, and `rec play` in
# the second run has no recording of its own to remember, so it goes and finds the most
# recent one in the folder. That is the cross-session half of the no-argument form, and
# only a second process exercises it.
#
# Its own user directory, so the recordings folder starts empty: the arms above have
# already put files in the suite's own, and "exactly one recording" is what says the
# auto-name landed where it belongs rather than somewhere that already had a file.
loopdir="$(mktemp -d)"
trap 'rm -rf "$here" "$loopdir"' EXIT

LBA2_USER_DIR="$loopdir" ctl --load "$LBA2_TEST_SAVE" \
    --exec-at 20 "rec start" --exec-at 120 "key up 60 2" \
    --exec-at 220 "rec stop" --tick 300 --exit >/dev/null 2>&1 ||
    fail "console loop: the recording run exited non-zero ($?) — hang or crash"

# Counted with -e per entry, because the shell hands back the pattern itself when nothing
# matches it: an unmatched glob would otherwise count as one file named `session-*.rec`,
# which is the case this assertion exists to catch.
loopcount=0
for f in "$loopdir"/recordings/session-*.rec; do
    if [ -e "$f" ]; then loopcount=$((loopcount + 1)); fi
done
[ "$loopcount" -eq 1 ] ||
    fail "console loop: expected one auto-named recording in $loopdir/recordings, found $loopcount"

loopout="$(LBA2_USER_DIR="$loopdir" ctl --load "$LBA2_TEST_SAVE" \
    --exec-at 20 "rec play" --tick 600 --exit 2>&1)" ||
    fail "console loop: the replay run exited non-zero ($?) — hang or crash"

case "$loopout" in
*"first hash mismatch -1"*) ;;
*)
    fail "console loop: $(printf '%s\n' "$loopout" |
        grep -m1 -e 'replay ended' -e 'cannot open' -e 'recordings in' ||
        echo 'the replay said nothing')"
    ;;
esac

# The replay pins its step and loads its scene after the file is opened, so a mode
# comparison made at open time reports the step of the run that has not armed one yet --
# a warning that this replay may not reproduce, on a replay that reproduces exactly, every
# time. A warning is worth having only if it stays quiet when nothing is wrong, and this
# is the run that can tell: the check above has already said the two agreed on every tick.
case "$loopout" in
*"mode differs"*)
    fail "console loop: $(printf '%s\n' "$loopout" | grep -m1 'mode differs') — but the replay was clean"
    ;;
esac

# Nothing to play. Reported rather than silent, and naming the folder it looked in:
# without that the answer is indistinguishable from a replay that started and did nothing.
emptydir="$(mktemp -d)"
trap 'rm -rf "$here" "$loopdir" "$emptydir"' EXIT
emptyout="$(LBA2_USER_DIR="$emptydir" ctl --load "$LBA2_TEST_SAVE" \
    --exec-at 20 "rec play" --tick 60 --exit 2>&1)" ||
    fail "console loop: the empty-folder run exited non-zero ($?) — hang or crash"
case "$emptyout" in
*"no .rec recordings in"*) ;;
*) fail "console loop: 'rec play' with no recordings to play said nothing about it" ;;
esac

pass "replayed clean: $bounded ticks checked with --tick, $unbounded without; a cut and a corrupted snapshot were both refused; a bare name went to the recordings folder; format 10 still reads ($lchecked ticks); telemetry named the injected change; mode.audio was written from the driver and reported both ways; a session recorded in one run replayed in the next with no flags and no paths"
