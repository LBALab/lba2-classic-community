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
# Every temp directory this file makes goes on one list, and the trap reads the list.
# The alternative, which this replaced, was a fresh trap per arm repeating every
# directory before it: seven of them by the end, each a chance to drop one. One was
# dropped, and the leak it left is invisible to every check the suite runs.
# An array, and quoted in the trap. A plain string would word-split, so a TMPDIR with a
# space in it would hand `rm -rf` the pieces rather than the path.
CLEAN=("$here")
clean_add() { CLEAN+=("$1"); }
trap 'rm -rf "${CLEAN[@]}"' EXIT

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
# tick stopped matching and never which of ~1300 values moved; --record-telemetry stores
# them all. Only the recording run needs it: the replay's reporter reads the values out of
# the file, so a replay that passed the flag would be arming nothing.
# Recorded once and replayed twice: once expecting clean, and once with a variable
# deliberately changed part-way through. The second run is the point. A reporter that
# printed nothing would pass the first check exactly like one that works, and this is a
# diagnostic whose whole job is to be believed on the day something real diverges.
rm -f "$rec" "$rec".lba "$rec".end.lba

ctl --record-telemetry --fixed-dt 16 --load "$LBA2_TEST_SAVE" --record "$rec" \
    --exec-at 30 "key up 60 2" --tick 300 --exit >/dev/null 2>&1 ||
    fail "verbose: recording run exited non-zero ($?) — hang or crash"

vout="$(ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$rec" --tick 400 --exit 2>&1)" ||
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
bout="$(ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$rec" --tick 400 \
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

aout="$(ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --replay "$flipped" \
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
clean_add "$loopdir"

LBA2_USER_DIR="$loopdir" ctl --load "$LBA2_TEST_SAVE" \
    --exec-at 20 "rec start" --exec-at 120 "key up 60 2" \
    --exec-at 220 "rec stop" --tick 300 --exit >/dev/null 2>&1 ||
    fail "console loop: the recording run exited non-zero ($?) — hang or crash"

# Counted with -e per entry, because the shell hands back the pattern itself when nothing
# matches it: an unmatched glob would otherwise count as one file named `session-*.rec`,
# which is the case this assertion exists to catch.
loopcount=0
looprec=""
for f in "$loopdir"/recordings/session-*.rec; do
    if [ -e "$f" ]; then
        loopcount=$((loopcount + 1))
        looprec="$f"
    fi
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
clean_add "$emptydir"
emptyout="$(LBA2_USER_DIR="$emptydir" ctl --load "$LBA2_TEST_SAVE" \
    --exec-at 20 "rec play" --tick 60 --exit 2>&1)" ||
    fail "console loop: the empty-folder run exited non-zero ($?) — hang or crash"
case "$emptyout" in
*"no .rec recordings in"*) ;;
*) fail "console loop: 'rec play' with no recordings to play said nothing about it" ;;
esac

# `rec start verbose`: the diagnostic recording, asked for from inside the game.
#
# --record-telemetry arms the same telemetry for a whole run, and that is the wrong shape
# for the case that wants it. A player watching something go wrong cannot go back and
# relaunch with the flag, and by the time they have, the session that showed it is gone.
# The word on the command is the same telemetry from the middle of a session, and it has
# one thing to prove: that the ask survives the snapshot-and-reload a mid-session start
# goes through, which lands a couple of ticks after the command that made it.
#
# Read back with no engine in the loop, because the question is what the file carries.
# The console saying it armed something and the stream holding the records are separate
# claims, and only the second one is any use a week later.
verbdir="$(mktemp -d)"
clean_add "$verbdir"

LBA2_USER_DIR="$verbdir" ctl --load "$LBA2_TEST_SAVE" \
    --exec-at 20 "rec start verbose" --exec-at 120 "key up 60 2" \
    --exec-at 220 "rec stop" --tick 300 --exit >/dev/null 2>&1 ||
    fail "verbose console: the recording run exited non-zero ($?) — hang or crash"

verbrec=""
for f in "$verbdir"/recordings/session-*.rec; do
    if [ -e "$f" ]; then verbrec="$f"; fi
done
[ -n "$verbrec" ] || fail "verbose console: nothing recorded under $verbdir/recordings"

verbcounts="$(python3 "$REPO/scripts/dev/dump_recording.py" "$verbrec" | grep -m1 '^polls=')" ||
    fail "verbose console: could not read $verbrec back"
case "$verbcounts" in
*"telemetry=0 "*) fail "verbose console: 'rec start verbose' recorded no telemetry ($verbcounts)" ;;
esac

# The control, and it is what makes the check above mean anything: the console loop
# recorded the same scene without the word, so a recorder that wrote telemetry for every
# session would pass the assertion above and fail here.
loopcounts="$(python3 "$REPO/scripts/dev/dump_recording.py" "$looprec" | grep -m1 '^polls=')" ||
    fail "verbose console: could not read $looprec back"
case "$loopcounts" in
*"telemetry=0 "*) ;;
*) fail "verbose console: a plain 'rec start' recorded telemetry anyway ($loopcounts)" ;;
esac

# Did the session do anything?
#
# Every arm above asserts that a replay agrees with its recording, and none asserts that
# there was anything to agree about. A recording of a hero who never moved replays clean
# and says nothing, and that is not hypothetical here: LBA2_TEST_SAVE is the game opening,
# where the scene's own script owns the hero and injected input is inert, so those arms
# record a stationary session and the per-tick digest faithfully confirms that one
# stationary session reproduces another. A bug that froze the game clock outright -- hero
# unable to walk, game time not advancing at all -- passed every one of them.
#
# So this arm walks the hero and asks three separate questions: did he move while
# recording, did the replay move him, and did it put him in the same place.
#
# The in-tree corpus save rather than LBA2_TEST_SAVE, because the answer has to come from
# a scene where input reaches the hero and that cannot depend on which save a machine
# happens to point at. No --fixed-dt either: the step the recorder pins for itself is the
# path the frozen clock was on, and the flag hid it.
movesave="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
[ -f "$movesave" ] || fail "movement: the corpus save is missing from $movesave"

movedir="$(mktemp -d)"
clean_add "$movedir"

# hero_xz <state.json> -- the engine's own dump, because `status` reports Nxw/Nyw/Nzw,
# which are collision scratch and not the hero.
hero_xz() {
    python3 -c "
import json, sys
h = json.load(open(sys.argv[1]))['hero']
print(h['x'], h['z'])" "$1"
}

# The control is the same command line with the input taken out, rather than a plain idle
# run: `rec start` pins the step and a run without it free-runs on the host clock, so a
# control missing both would be answering about two variables at once and could pass on
# the wrong one. Here the only difference between the two runs is the key.
#
# `key` rather than `input`, and that is not interchangeable. `key` holds a raw scancode in
# TabKeys, which is where a player's input arrives and what the recorder captures at the
# poll hook. `input` is OR'd straight into Input by MainLoop and never touches TabKeys, so
# a recording only reproduces it because the console command itself was recorded and runs
# again -- which would leave this arm passing while testing command replay rather than
# input replay.
# Its own profile, so the folder the replay below reads holds exactly one recording and
# `rec play` with no argument cannot pick the control's by accident.
LBA2_USER_DIR="$movedir/control" ctl --load "$movesave" \
    --exec-at 20 "rec start" --exec-at 620 "rec stop" \
    --tick 700 --dump-state "$movedir/idle.json" --exit >/dev/null 2>&1 ||
    fail "movement: the control run exited non-zero ($?) — hang or crash"

LBA2_USER_DIR="$movedir" ctl --load "$movesave" \
    --exec-at 20 "rec start" --exec-at 200 "key up 300" --exec-at 620 "rec stop" \
    --tick 700 --dump-state "$movedir/rec.json" --exit >/dev/null 2>&1 ||
    fail "movement: the recording run exited non-zero ($?) — hang or crash"

idle_xz="$(hero_xz "$movedir/idle.json")" || fail "movement: could not read the control state dump"
rec_xz="$(hero_xz "$movedir/rec.json")" || fail "movement: could not read the recorded state dump"

if [ "$idle_xz" = "$rec_xz" ]; then
    fail "movement: the hero is at $rec_xz with input and $idle_xz without it — the recorded session never moved, so a clean replay of it would prove nothing"
fi

# Replayed through --replay rather than the console `rec play`, because this arm asks
# where the replay *left* the hero and `rec play` no longer leaves him there: a
# console-driven playback saves the player's session first and puts it back when the
# replay ends, so the state at exit would be the restore rather than the replay. The flag
# path takes no return point -- that run exists to replay and exit -- so it is the one
# that can still be asked this question. The restore has an arm of its own below.
moverec="$(ls "$movedir"/recordings/*.rec 2>/dev/null | head -1)"
[ -n "$moverec" ] || fail "movement: the recording run wrote no recording to $movedir/recordings"

moveout="$(LBA2_USER_DIR="$movedir" ctl --load "$movesave" --replay "$moverec" \
    --tick 900 --dump-state "$movedir/play.json" --exit 2>&1)" ||
    fail "movement: the replay run exited non-zero ($?) — hang or crash"

case "$moveout" in
*"first hash mismatch -1"*) ;;
*)
    fail "movement: $(printf '%s\n' "$moveout" |
        grep -m1 -e 'replay ended' -e 'cannot open' -e 'recordings in' ||
        echo 'the replay said nothing')"
    ;;
esac

play_xz="$(hero_xz "$movedir/play.json")" || fail "movement: could not read the replayed state dump"
[ "$play_xz" = "$rec_xz" ] ||
    fail "movement: the recording left the hero at $rec_xz and the replay left him at $play_xz — the digest matched every tick, so this is the replay ending somewhere else, not diverging"

# Giving the step back.
#
# A mid-session `rec start` pins the simulation step, and a pinned step advances game time
# by dt per rendered frame rather than by wall clock. The recorder paces frames to match
# only while it is actually recording, so a session that kept the step afterwards ran at
# whatever rate it rendered at: measured headless, 1.00x real time before a recording and
# 3.12x after one. On a vsynced 60 Hz window that reads as roughly right and on a 144 Hz
# one as more than twice too fast, which is the worse of the two -- it looks like the game,
# only wrong.
#
# A run given --fixed-dt is the other case and must not be touched: there the step belongs
# to the whole run and to whoever asked for it. So both directions are asserted here, and
# the arm is the reason: a release that fired for everyone would silently unpin every
# harness fixture in this file.
#
# `rec info` reports the live run's mode, which makes this a question about a printed
# number rather than about elapsed time.
stepdir="$(mktemp -d)"
clean_add "$stepdir"

# Through a file rather than a pipeline. The suite does not set `pipefail`, so a pipeline
# carries the status of its last command -- `tr` here, which succeeds whatever the engine
# did -- and a `|| fail ... exited non-zero` hung off one can never fire. Run, check, then
# read.
step_after_stop() { # step_after_stop <extra ctl args...>
    LBA2_USER_DIR="$stepdir" ctl --load "$movesave" "$@" \
        --exec-at 20 "rec start" --exec-at 300 "rec stop" --exec-at 320 "rec info" \
        --tick 400 --exit > "$stepdir/out.txt" 2>&1 || return $?
    # The last one: `rec stop` prints this same block itself, before it stops.
    grep 'mode.fixed_dt=' "$stepdir/out.txt" | tail -1 | tr -d ' '
}

recarmed="$(step_after_stop)" ||
    fail "step: the recorder-armed run exited non-zero ($?) — hang or crash"
case "$recarmed" in
*"mode.fixed_dt=0") ;;
*)
    fail "step: the recorder pinned the step and still held it after rec stop ($recarmed) — the session keeps running at frame rate instead of wall clock"
    ;;
esac

flagarmed="$(step_after_stop --fixed-dt 16)" ||
    fail "step: the flag-armed run exited non-zero ($?) — hang or crash"
case "$flagarmed" in
*"mode.fixed_dt=16") ;;
*)
    fail "step: --fixed-dt 16 was given and rec stop unpinned it anyway ($flagarmed) — the recorder is releasing a step it did not take"
    ;;
esac

# Watching a recording must not cost the player their game.
#
# A playback loads the recording's world over the live one. Before this it was one-way:
# the replay ended and the player was left standing wherever the recording finished, in
# its world rather than theirs. Measured over the control socket, a player who walked away
# from the recording's end point and then watched it back was put at the recording's end,
# 1585 units from where they had been.
#
# The arm has to prove the two destinations are actually different, or it passes on a
# coincidence. So the hero walks one way while recording and a different way afterwards,
# and all three positions come out of one run: `dumpstate` at a chosen tick for the two
# mid-run ones, `--dump-state` for the last.
#
# Not the flag path. `--replay` takes no return point by design -- that run exists to
# replay and exit, and has no player session to protect -- so the question only means
# something for a playback started from inside a session.
retdir="$(mktemp -d)"
clean_add "$retdir"

LBA2_USER_DIR="$retdir" ctl --load "$movesave" \
    --exec-at 20 "rec start" --exec-at 200 "key up 250" --exec-at 520 "rec stop" \
    --exec-at 560 "dumpstate $retdir/recend.json" \
    --exec-at 600 "key down 250" \
    --exec-at 920 "dumpstate $retdir/before.json" \
    --exec-at 960 "rec play" \
    --tick 2000 --dump-state "$retdir/after.json" --exit >/dev/null 2>&1 ||
    fail "return: the run exited non-zero ($?) — hang or crash"

for f in recend before after; do
    [ -s "$retdir/$f.json" ] || fail "return: no $f state dump was written"
done
recend_xz="$(hero_xz "$retdir/recend.json")" || fail "return: could not read the recording's end"
before_xz="$(hero_xz "$retdir/before.json")" || fail "return: could not read the pre-playback state"
after_xz="$(hero_xz "$retdir/after.json")" || fail "return: could not read the post-playback state"

# Without this the arm below passes whenever the two happen to coincide, which is most of
# the ways it could be broken.
[ "$recend_xz" != "$before_xz" ] ||
    fail "return: the hero is at $before_xz both at the recording's end and when playback started, so this arm cannot tell a restore from doing nothing"

[ "$after_xz" = "$before_xz" ] ||
    fail "return: playback started with the hero at $before_xz and left him at $after_xz (the recording ended at $recend_xz) — the player's session was not put back"

# The return point is scratch and belongs to the recorder, not to the player's save folder,
# and it has to be gone once it has been read.
for leftover in "$retdir"/recordings/*.return.lba "$retdir"/recordings/*.staging.lba; do
    if [ -e "$leftover" ]; then
        fail "return: $leftover was left behind"
    fi
done

# Stopping a playback early has to return the player too, and the narrow window is the one
# that got this wrong: between `rec play` and the reload it asks for landing, two ticks
# later. The load cannot be taken back once issued -- the engine changes cube either way --
# so the player is standing in the recording's world with the session called off, and
# Record_Stop cannot ask for the way back because that reload is still in flight when it
# runs. Measured before it was handled: the hero was left at the recording's start.
#
# `rec stop` one tick after `rec play` lands in that window; the arm below it stops well
# clear of it, so the two together cover the abort path and the ordinary one.
for stopat in 961 1100; do
    LBA2_USER_DIR="$retdir/early$stopat" ctl --load "$movesave" \
        --exec-at 20 "rec start" --exec-at 200 "key up 250" --exec-at 520 "rec stop" \
        --exec-at 600 "key down 250" \
        --exec-at 920 "dumpstate $retdir/early$stopat-before.json" \
        --exec-at 960 "rec play" --exec-at "$stopat" "rec stop" \
        --tick 1600 --dump-state "$retdir/early$stopat-after.json" --exit >/dev/null 2>&1 ||
        fail "return: the early-stop run (stop at $stopat) exited non-zero ($?) — hang or crash"

    early_before="$(hero_xz "$retdir/early$stopat-before.json")" ||
        fail "return: could not read the pre-playback state for stop at $stopat"
    early_after="$(hero_xz "$retdir/early$stopat-after.json")" ||
        fail "return: could not read the post-stop state for stop at $stopat"
    [ "$early_after" = "$early_before" ] ||
        fail "return: a playback stopped at tick $stopat started with the hero at $early_before and left him at $early_after — stopping a playback has to put the player back too"
done

# --- recording does not perturb the run -------------------------------------------
#
# The claim the recorder rests on, and until this arm nothing checked it. Every other arm
# here runs --fixed-dt 16, and under a pinned step the question cannot even be asked: the
# game advances 16 ms per tick whatever the frame cost, so a recorder that doubled the
# frame time would show up as the run taking longer and not as the game behaving
# differently.
#
# Asked as a rate rather than as a state diff, and that is not a shortcut. On a loose
# clock two runs of the same scripted session legitimately part -- game time is a function
# of how long each frame took -- so diffing their states would be flaky from the first run
# and the obvious repair would be to pin the step, which is the thing the arm exists to
# rule out.
#
# Two questions, because either alone passes for the wrong reason. Whether each run is
# real time catches a step pinned when it should not be: --fixed-dt 16 with no recorder at
# all runs 600 ticks of game time in a fraction of the wall time they describe, measured
# at 3.49x. Whether the two runs agree catches the recorder costing time without changing
# the clock it reports.
ratedir="$(mktemp -d)"
clean_add "$ratedir"

rate_of() { # rate_of <label> [extra args...] -- game seconds per wall second, on stdout
    local label="$1"
    shift
    local ud="$ratedir/$label"
    mkdir -p "$ud"
    local t0 t1
    # python rather than `date +%s.%N`: %N is GNU-only, and a BSD date passes the literal
    # N through to the arithmetic below, where the arm fails as though the run had hung.
    t0="$(python3 -c 'import time; print(time.time())')"
    LBA2_USER_DIR="$ud" ctl --load "$LBA2_TEST_SAVE" \
        --exec-at 20 "dumpstate $ud/before.json" \
        --exec-at 40 "key up 800" \
        --tick 900 --dump-state "$ud/after.json" --exit "$@" >/dev/null 2>&1 || return 1
    t1="$(python3 -c 'import time; print(time.time())')"
    python3 -c "
import json, sys
b = json.load(open('$ud/before.json'))['timer_ref_hr']
a = json.load(open('$ud/after.json'))['timer_ref_hr']
game = (a - b) / 1000.0
wall = $t1 - $t0
if wall <= 0 or game <= 0:
    sys.exit(1)
print('%.3f' % (game / wall))"
}

# Not in a command substitution that swallows it: `fail` ends the shell it runs in, so a
# failure inside $( ) would kill the subshell and let the caller print PASS over the top.
rate_ctl="$(rate_of control)" ||
    fail "rate: the control run exited non-zero or dumped no clock — hang, crash, or a stopped clock"
rate_rec="$(rate_of recording --record "$ratedir/recording/s.rec")" ||
    fail "rate: the recording run exited non-zero or dumped no clock — hang, crash, or a stopped clock"

# The band is wide on purpose, and the floor especially. It has one job -- separate real
# time from a pinned step, which reads about 3.5 -- and two reasons not to be tight: boot
# sits inside the wall time while it is outside the game time, and this suite shares a
# machine with whatever else is running on it. A tight floor would fail the control arm on
# a busy host and say nothing about recording. The ratio check below is the part that is
# actually about the recorder, and it is not affected by either.
for pair in "control:$rate_ctl" "recording:$rate_rec"; do
    lbl="${pair%%:*}"
    val="${pair#*:}"
    ok="$(python3 -c "print(1 if 0.25 <= $val <= 2.0 else 0)")"
    [ "$ok" = 1 ] ||
        fail "rate: the $lbl run advanced ${val}s of game time per wall second, which is not real time — a step pinned here would read about 3.5"
done

# Between the two, where the recorder is the only difference. Machine load moves both, so
# the ratio is the part that is about recording.
rate_gap="$(python3 -c "print('%.3f' % ($rate_rec / $rate_ctl))")"
gap_ok="$(python3 -c "print(1 if 0.7 <= $rate_gap <= 1.3 else 0)")"
[ "$gap_ok" = 1 ] ||
    fail "rate: recording ran at $rate_rec game seconds a wall second against $rate_ctl without it (${rate_gap}x) — recording is costing the run time"

# The claim above is conditional, and this is the condition. RECORD.CPP writes a 20-byte
# analog block per poll whenever any of the stick, pad, mouse or click fields is non-zero,
# and on a host where the mouse delta never drains to zero that gate is always true: a
# contributed 90,692-tick session came back 96.7 MB, of which 89.5 MB was an analog block
# on every one of 4.5 million polls, against about 7 MB without. So "recording is free" is
# free on the cheap side of that gate, and a run that quietly crossed it would still pass
# the rate checks on a fast enough machine while describing nothing. Assert the side we
# are on rather than let it go unsaid.
ratecounts="$(python3 "$REPO/scripts/dev/dump_recording.py" "$ratedir/recording/s.rec" |
    grep -m1 '^polls=')" ||
    fail "rate: could not read the recording back"
ratepolls="$(printf '%s\n' "$ratecounts" | sed 's/.*polls=\([0-9]*\).*/\1/')"
rateanalog="$(printf '%s\n' "$ratecounts" | sed 's/.*analog=\([0-9]*\).*/\1/')"
# Counted rather than inferred from the file size: a short session is mostly header and
# the two savegames it carries, so bytes a poll says more about those than about the
# analog gate.
analog_ok="$(python3 -c "print(1 if $rateanalog <= $ratepolls // 10 else 0)")"
[ "$analog_ok" = 1 ] ||
    fail "rate: $rateanalog of $ratepolls polls carry an analog block, so the gate at RECORD.CPP is firing on most of them — the rate result above is not describing an ordinary recording"

# --- a modal loop is held to real time too -------------------------------------------
#
# The arm above asks the question over a whole run, and over a whole run it cannot see
# this. A fade, a menu or a dialogue box advances the game clock inside its own loop, and
# that loop is short: measured, FadeToBlack (SOURCES/AMBIANCE.CPP) took 208 ms of game
# clock in 26 ms of wall, eight times real speed, inside a session whose overall rate read
# 1.02x because the paced main loop averaged the spike away. Every figure averaged over a
# session has that blind spot, so this one is asked over a window instead.
#
# Read from clock_src_ms and not timer_ref_hr: timer_ref_hr is accumulated play time and
# moves backwards through a scene change -- measured -4096 ms across this very window --
# so a bracket around a transition reads negative. clock_src_ms is what ManageTime would
# read, which is the pinned clock while one is armed and the host clock otherwise.
#
# One-sided on purpose. A slow or loaded machine only ever pushes the rate down, so a
# ceiling cannot fail for being busy; the failure it is looking for is the game running
# faster than the player, which is the whole complaint.
modaldir="$(mktemp -d)"
clean_add "$modaldir"

ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --record "$modaldir/s.rec" \
    --exec-at 40 "key up 200" \
    --exec-at 295 "dumpstate $modaldir/w0.json" \
    --exec-at 300 "cube 154" \
    --exec-at 340 "dumpstate $modaldir/w1.json" \
    --tick 600 --exit >/dev/null 2>&1 ||
    fail "modal pacing: the scene-change run exited non-zero"
[ -s "$modaldir/w0.json" ] && [ -s "$modaldir/w1.json" ] ||
    fail "modal pacing: the run dumped no window — it never reached the scene change"

# Two numbers out of one reader, so the window is described once.
modalout="$(python3 -c "
import json
a = json.load(open('$modaldir/w0.json'))
b = json.load(open('$modaldir/w1.json'))
game = b['clock_src_ms'] - a['clock_src_ms']
wall = b['wall_ms'] - a['wall_ms']
ticks = b['tick'] - a['tick']
if wall <= 0 or game <= 0 or ticks <= 0:
    raise SystemExit(1)
print('%.3f %d %d' % (game / wall, game, ticks * 16))")" ||
    fail "modal pacing: the window dumped no usable clock — a stopped clock, or a bracket that caught nothing"

modalrate="${modalout%% *}"
modalgame="$(printf '%s\n' "$modalout" | cut -d" " -f2)"
modalticks="$(printf '%s\n' "$modalout" | cut -d" " -f3)"

# The condition the check rests on: that this window holds a modal at all. Ticks mint dt
# each and nothing else in a quiet window does, so a window whose clock advanced no
# further than its ticks account for never entered one -- and would pass the rate check
# below by having nothing in it to fail. Without this the arm quietly stops testing the
# moment the scripted scene change stops landing.
[ "$modalgame" -gt "$modalticks" ] ||
    fail "modal pacing: the window advanced ${modalgame} ms over ticks worth ${modalticks} ms, so no modal ran inside it — the scene change did not land and this arm tested nothing"

modal_ok="$(python3 -c "print(1 if $modalrate <= 1.15 else 0)")"
[ "$modal_ok" = 1 ] ||
    fail "modal pacing: a window holding a scene change advanced ${modalrate}s of game time per wall second while recording — the modal loops are minting clock they do not pay for, which is the too-fast transitions players report"

pass "replayed clean: $bounded ticks checked with --tick, $unbounded without; a cut and a corrupted snapshot were both refused; a bare name went to the recordings folder; format 10 still reads ($lchecked ticks); telemetry named the injected change; mode.audio was written from the driver and reported both ways; a session recorded in one run replayed in the next with no flags and no paths; \
'rec start verbose' carried telemetry and a plain one carried none; a recorded walk moved the hero and the replay walked it again; the recorder gave the step back and left the flag's alone; a playback put the player back where it found them, stopped early or run out; a window holding a scene change ran at ${modalrate}x real, not faster"
