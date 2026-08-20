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
TESTNAME="record_replay"
. "$(dirname "$0")/lib.sh"
precheck
need_save

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

# The default home. A name with no directory in it is a name in <userDir>/recordings/,
# and the point of that is symmetry: the name a session was recorded under is the name it
# replays under, from whatever directory the run happens to start in. Recorded and replayed from
# a directory that holds no recording of its own, so a pass cannot come from the working
# directory answering instead of the folder.
bare="bare-name.rec"
recdir="$(user_dir)/recordings"
rm -f "$recdir/$bare"
here="$(mktemp -d)"

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
rm -rf "$here"

case "$bareout" in
*"first hash mismatch -1"*) ;;
*)
    fail "bare name: $(printf '%s\n' "$bareout" |
        grep -m1 -e 'replay ended' -e 'cannot open' || echo 'the replay said nothing')"
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
# ones. Exits non-zero by design: the run diverged.
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

pass "replayed clean: $bounded ticks checked with --tick, $unbounded without; telemetry named the injected change"
