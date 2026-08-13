#!/usr/bin/env bash
# Every flag against the contract its own table declares.
#
# A flag names a mode for one run. A run told to render at 640x480, or to throttle
# the sim, or to speak German, must not leave that behind for the next one: the
# settings belong to the player, and the harness runs constantly on the same
# machine. The exceptions are the flags whose job is to act on stored state, where
# persisting is the point rather than a side effect. CLI_ARGS.CPP's `writes` column
# is where that is declared, and `--help-all` prints it as "[keeps this in your
# settings]", which is what this test reads.
#
# Both directions, because either alone passes on a broken engine: a flag that does
# nothing satisfies "leaves the settings alone", and one that changes them for good
# satisfies "takes effect". So the modes are checked for byte-identical settings,
# and the writing flags are checked to actually write.
#
# The list below is checked against --help-all, so a flag added to the table
# without a case here fails rather than going untested.
#
# adeline.log is excluded throughout: it is this run's own record and is meant to
# differ. Settings and saves are the subject.
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
TESTNAME=cli_flag_contract
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
[ -f "$SAVE" ] || skip "fixture save missing: $SAVE"

# flag|sample arguments, re-parsed by eval so a value stays one argument: an
# --exec split into words becomes a different command, and the case would then
# pass or fail for a reason that has nothing to do with the flag. One case per
# flag that takes part; the coverage check below proves the list is complete.
# Flags that exit before the engine boots (--help, -h, --version, --help-all) and
# the ones needing a display or an image nobody can assume (--pick-game-dir,
# --disc) are listed as skipped, on purpose and by name, so "not covered" is
# never silent.
MODES="
--resolution|--resolution 1280x720
--language|--language Deutsch
--no-audio|--no-audio
--game-dir|--game-dir \"$LBA2_GAME_DIR\"
--data-dir|--data-dir \"$LBA2_GAME_DIR\"
--user-dir|
--headless|--headless
--no-autosave|--no-autosave
--tick|--tick 3
--exit|--exit
--fixed-dt|--fixed-dt 16
--fixed-timestep|--fixed-timestep 100
--screenshot|--screenshot \"$tmp/shot.png\"
--dump-state|--dump-state \"$tmp/dump.json\"
--demo|--demo
--log-level|--log-level debug
--verbose|--verbose
--polyrec|--polyrec \"$tmp/poly.rec\"
--capture-projection|--capture-projection \"$tmp/proj.txt\"
--projection-hash|--projection-hash \"$tmp/proj.hash\"
--res-switch-test|--res-switch-test 2:1280x720
--save-load-test|--save-load-test \"$tmp/slt.lba\"
"
WRITERS="
--profile|--profile contract
--load|--load \"$SAVE\"
--exec|--exec \"vsync off\"
--exec-at|--exec-at 1 \"vsync off\"
"
NOT_RUN="--help -h --version --help-all --pick-game-dir --disc"

settings_snapshot() { # settings_snapshot <user-dir> <out-file>
    ( cd "$1" 2>/dev/null && find . -type f ! -name 'adeline.log' | sort | while read -r f; do
        printf '%s %s\n' "$(md5sum < "$f" | cut -c1-16)" "$f"
      done ) > "$2"
}

# stdin comes from /dev/null: the loops below feed their case list on stdin, and a
# child that inherits it eats the rest of the list. That failure is silent, and
# looks like the flags it swallowed passing.
run_into() { # run_into <user-dir> <extra args...>
    ctl --user-dir "$1" --language English --no-audio --fixed-dt 16 --tick 3 --exit \
        "${@:2}" >/dev/null 2>&1 </dev/null
}

# --- the baseline every case is compared against ------------------------------
base_dir="$tmp/base"; base="$tmp/base.snap"
run_into "$base_dir"
[ -f "$base_dir/lba2.cfg" ] || fail "the baseline run wrote no config at all"
settings_snapshot "$base_dir" "$base"
[ -s "$base" ] || fail "the baseline snapshot is empty; nothing would be compared"

# --- a mode leaves the settings exactly as it found them ----------------------
n_modes=0
while IFS='|' read -r flag args; do
    [ -n "$flag" ] || continue
    d="$tmp/m$n_modes"
    eval run_into "\"\$d\"" "$args"
    snap="$tmp/m$n_modes.snap"
    settings_snapshot "$d" "$snap"
    if ! diff -q "$base" "$snap" >/dev/null 2>&1; then
        fail "$flag changed the settings: $(diff "$base" "$snap" | grep -E '^[<>]' | tr '\n' ' ')"
    fi
    n_modes=$((n_modes + 1))
done <<EOF
$MODES
EOF

# --- a flag that is meant to write does write ---------------------------------
# Without this the block above would also pass on an engine that had stopped
# writing the config at all, which is the failure it is least able to see.
n_writers=0
while IFS='|' read -r flag args; do
    [ -n "$flag" ] || continue
    d="$tmp/w$n_writers"
    eval run_into "\"\$d\"" "$args"
    snap="$tmp/w$n_writers.snap"
    settings_snapshot "$d" "$snap"
    if diff -q "$base" "$snap" >/dev/null 2>&1; then
        fail "$flag is declared as keeping a setting but changed nothing"
    fi
    n_writers=$((n_writers + 1))
done <<EOF
$WRITERS
EOF

# --- the list above covers the table ------------------------------------------
# --help-all prints every flag, and marks the ones the table says may write. A
# flag missing from this test, or one whose declaration disagrees with the column
# it is tested against, fails here rather than passing unnoticed.
help_all="$tmp/help-all.txt"
"$LBA2_BIN" --help-all > "$help_all" 2>/dev/null || fail "--help-all exited non-zero"

not_run=$(printf '%s\n' "$NOT_RUN" | tr ' ' '\n' | grep -v '^$' | sort -u)
declared_writers=$(awk '
    /^  -/ { name = $1 }
    /\[keeps this in your settings\]/ { if (name != "") { print name; name = "" } }
' "$help_all" | sort -u)
# --pick-game-dir is declared as writing and cannot be exercised here: it opens a
# folder picker, which a headless run has no way to answer. Subtracting the
# not-run list rather than relaxing the comparison keeps the coupling: a new
# writing flag that can be run still has to have a case.
declared_runnable=$(comm -23 <(printf '%s\n' "$declared_writers") <(printf '%s\n' "$not_run"))
tested_writers=$(printf '%s\n' "$WRITERS" | cut -d'|' -f1 | grep -v '^$' | sort -u)
[ "$declared_runnable" = "$tested_writers" ] || fail "the flags declared as writing and the flags tested as writing differ:
  declared: $(printf '%s' "$declared_runnable" | tr '\n' ' ')
  tested:   $(printf '%s' "$tested_writers" | tr '\n' ' ')"

all_flags=$(awk '/^  -/ { print $1 }' "$help_all" | sort -u)
covered=$(printf '%s\n%s\n%s\n' \
    "$(printf '%s' "$MODES" | cut -d'|' -f1)" \
    "$tested_writers" \
    "$not_run" | grep -v '^$' | sort -u)
missing=$(comm -23 <(printf '%s\n' "$all_flags") <(printf '%s\n' "$covered"))
[ -z "$missing" ] || fail "no contract case for: $(printf '%s' "$missing" | tr '\n' ' ')"

# Counts, because a loop that read a short list would otherwise report a pass for
# the cases it never ran. This is the same silent-truncation failure the /dev/null
# on run_into's stdin exists to prevent, asserted rather than assumed.
want_modes=$(printf '%s' "$MODES" | grep -c '|')
want_writers=$(printf '%s' "$WRITERS" | grep -c '|')
[ "$n_modes" = "$want_modes" ] && [ "$n_writers" = "$want_writers" ] \
    || fail "ran $n_modes/$want_modes mode cases and $n_writers/$want_writers writer cases"

pass "$n_modes flags left the settings untouched, $n_writers kept what they were asked to"
