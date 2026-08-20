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

    rm -f "$rec" "$rec".lba "$rec".end.lba

    ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --record "$rec" \
        --exec-at 30 "key up 60 2" "$@" \
        >/dev/null 2>&1 ||
        fail "$label: recording run exited non-zero ($?) — hang or crash"

    [ -s "$rec" ] || fail "$label: no recording written to $rec"

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

# A recording this build cannot make: version 10, written before the keyframe carried a
# length, so the reader has to know that a version 10 keyframe is 23 fields and no count.
# Everything else here records and replays with the same build, which cannot catch a
# change that breaks both ends together. That is exactly how the keyframe grew a field
# with no version bump: every earlier recording then read four bytes long at its first
# keyframe and reported garbage ticks, and no test here noticed.
legacy="$REPO/tests/automation/recordings/legacy-v10.rec"
legacy_save="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
if [ -f "$legacy" ] && [ -f "$legacy_save" ]; then
    lout="$(ctl --fixed-dt 16 --load "$legacy_save" --replay "$legacy" --tick 300 --exit 2>&1)" ||
        fail "legacy: replay run exited non-zero ($?)"
    case "$lout" in
    *"first hash mismatch -1"*) ;;
    *)
        fail "legacy: $(printf '%s\n' "$lout" |
            grep -m1 -e 'replay ended' -e 'consistency failure' -e 'too old' ||
            echo 'the version 10 recording did not replay')"
        ;;
    esac
fi

record_and_replay "with --tick" --tick 300 --exit
bounded="$CHECKED"

# Unbounded: the player's form. Nothing ends the run, so the recording is stopped by a
# console command on a tick of its own and the run quits there. That command only
# arrives if the tick hook is still armed, which is the property this arm exists for:
# a run that finalizes on its first tick never reaches tick 300 and times out here.
record_and_replay "without --tick" --exec-at 300 "rec stop; exit"
unbounded="$CHECKED"

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

pass "replayed clean: $bounded ticks checked with --tick, $unbounded without; the version 10 recording still reads; telemetry named the injected change"
