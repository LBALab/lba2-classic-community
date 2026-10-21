#!/usr/bin/env bash
# Run the CLI control-harness test suite. Local-only: needs the built binary, retail game data
# (LBA2_GAME_DIR), and a display. Tests skip cleanly when prerequisites are absent, so an empty
# environment reports SKIP, not FAIL.
#
#   LBA2_GAME_DIR=/path/to/data tests/automation/run.sh                 # everything
#   LBA2_GAME_DIR=/path/to/data tests/automation/run.sh --fast          # the small tier only
#   LBA2_GAME_DIR=/path/to/data tests/automation/run.sh -j 4 camzone    # names matching, 4 at once
#
# Exit 0 if nothing failed (passes and skips ok), 1 if any test failed.
#
#   --tier LIST   only fixtures in these tiers (comma-separated: small,medium,big)
#   --fast        same as --tier small
#   -j N          run N fixtures at once (default 1). Fixtures are isolated, so this is safe;
#                 the ring fixtures start engine runs of their own, see RING_JOBS in ringlib.sh
#   --profile     log every engine run (count and seconds per fixture) through engine-profile.sh
#   --list        print the fixtures with their tier and last time, run nothing
#   --no-record   run without touching last-run.md
#   NAME...       only fixtures whose name contains one of these words
#
# Each fixture declares its tier on a `# Tier:` line in its header. The tiers are cost budgets on
# the reference machine (an Apple Silicon Mac): small finishes within SMALL_S seconds, medium
# within MEDIUM_S, and big is anything slower. A fixture that runs past its tier's budget is
# reported as over budget: not a failure, since a slower machine is slower, but the sign that it
# belongs in another tier or has grown a bottleneck. Budgets are checked only at -j 1, where a
# fixture's time is its own. docs/TESTING.md has the rules.
#
# Writes last-run.md after a full run (no tier or name filter), because no workflow can run this
# suite and an unrecorded result is indistinguishable from never having run it. Commit it, or
# paste it into a PR that touches engine behaviour. A partial run records nothing there: it would
# claim a result for fixtures it did not run.
#
# Every run, full or not, appends one line per fixture to .runs/history.tsv (not tracked): the
# timing telemetry. --list reads it, and -j starts the slowest fixtures first from it.
set -u
# Seconds are printed with a decimal point, whatever the machine's locale writes (awk follows
# LC_NUMERIC, and a comma would also break the history file for anything that reads it back).
export LC_NUMERIC=C
DIR="$(cd "$(dirname "$0")" && pwd)"
RECORD="$DIR/last-run.md"
HISTORY_DIR="$DIR/.runs"
HISTORY="$HISTORY_DIR/history.tsv"

SMALL_S=3
MEDIUM_S=15

record=true tiers="" jobs=1 profile=false list=false
names=()
while [ $# -gt 0 ]; do
    case "$1" in
        --no-record) record=false ;;
        --fast) tiers=small ;;
        --tier) tiers="${2:-}"; shift ;;
        --tier=*) tiers="${1#--tier=}" ;;
        -j) jobs="${2:-1}"; shift ;;
        -j*) jobs="${1#-j}" ;;
        --profile) profile=true ;;
        --list) list=true ;;
        -h | --help) sed -n '2,33p' "$0"; exit 0 ;;
        -*) echo "run.sh: unknown option $1" >&2; exit 2 ;;
        *) names+=("$1") ;;
    esac
    shift
done
case "$jobs" in '' | *[!0-9]* | 0) echo "run.sh: -j needs a positive number" >&2; exit 2 ;; esac
for t in ${tiers//,/ }; do
    case "$t" in small | medium | big) ;; *) echo "run.sh: no tier named '$t'" >&2; exit 2 ;; esac
done

# Milliseconds since the epoch. EPOCHREALTIME (bash 5) prints its decimal point in the locale's
# own spelling, a comma in many, so the separator is dropped rather than parsed.
now_ms() {
    if [ -n "${EPOCHREALTIME:-}" ]; then
        local us="${EPOCHREALTIME/[.,]/}"
        echo $((10#$us / 1000))
    else
        echo $(($(date +%s) * 1000))
    fi
}

secs() { awk -v m="$1" 'BEGIN { printf "%.1f", m / 1000 }'; }

tier_of() { # tier_of <fixture path>: its declared tier, or "none"
    local t
    t="$(sed -n 's/^# Tier: *\([a-z]*\).*/\1/p' "$1" | head -1)"
    echo "${t:-none}"
}

budget_s() { # budget_s <tier>: seconds the tier allows, empty for no ceiling
    case "$1" in small) echo "$SMALL_S" ;; medium) echo "$MEDIUM_S" ;; esac
}

last_ms() { # last_ms <name>: the latest recorded time for a fixture, 0 if none
    [ -f "$HISTORY" ] || { echo 0; return; }
    awk -F'\t' -v n="$1" '$5 == n { ms = $7 } END { print ms + 0 }' "$HISTORY"
}

# The fixtures this run covers.
selected=()
for t in "$DIR"/test_*.sh; do
    name="$(basename "$t" .sh)"
    if [ ${#names[@]} -gt 0 ]; then
        hit=false
        for w in "${names[@]}"; do case "$name" in *"$w"*) hit=true ;; esac; done
        $hit || continue
    fi
    if [ -n "$tiers" ]; then
        case ",$tiers," in *",$(tier_of "$t"),"*) ;; *) continue ;; esac
    fi
    selected+=("$t")
done
[ ${#selected[@]} -gt 0 ] || { echo "run.sh: no fixture matches" >&2; exit 2; }

if $list; then
    printf '%-40s %-7s %9s\n' fixture tier last
    for t in "${selected[@]}"; do
        name="$(basename "$t" .sh)"
        printf '%-40s %-7s %8ss\n' "$name" "$(tier_of "$t")" "$(secs "$(last_ms "$name")")"
    done
    exit 0
fi

# Parallel runs wait for any one job to finish, which needs bash 4.3; macOS's /bin/bash is 3.2.
if [ "$jobs" -gt 1 ] && { [ "${BASH_VERSINFO[0]}" -lt 4 ] ||
    { [ "${BASH_VERSINFO[0]}" -eq 4 ] && [ "${BASH_VERSINFO[1]}" -lt 3 ]; }; }; then
    echo "run.sh: -j needs bash 4.3 or newer (this is $BASH_VERSION); running one at a time" >&2
    jobs=1
fi

# Slowest first when running several at once, so the long ones do not start last and leave the
# others idle at the end. One at a time keeps the alphabetical order people read the log in.
if [ "$jobs" -gt 1 ]; then
    ordered=()
    while IFS=$'\t' read -r _ path; do ordered+=("$path"); done < <(
        for t in "${selected[@]}"; do printf '%s\t%s\n' "$(last_ms "$(basename "$t" .sh)")" "$t"; done |
            sort -t$'\t' -k1,1nr)
    selected=("${ordered[@]}")
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/lba2-run-XXXXXX")"
trap 'rm -rf "$work"' EXIT

if $profile; then
    : "${LBA2_BIN:=}"
    if [ -z "$LBA2_BIN" ]; then
        # Resolve the binary the way the fixtures would, then put the logger in front of it.
        LBA2_BIN="$(bash -c '. "$1/lib.sh" >/dev/null 2>&1; printf %s "$LBA2_BIN"' _ "$DIR")"
    fi
    [ -x "$LBA2_BIN" ] || { echo "run.sh: --profile found no binary (set LBA2_BIN)" >&2; exit 2; }
    export LBA2_PROFILE_BIN="$LBA2_BIN"
    export LBA2_BIN="$DIR/engine-profile.sh"
fi

# run_one <fixture>: run it in a folder of its own and leave rc, ms and output behind in $work.
# The folder is the fixture's TMPDIR and working directory, so what it leaks is removed with it,
# and two fixtures running at once cannot meet in a shared temp path.
run_one() {
    local t="$1" name sandbox t0 rc
    name="$(basename "$t" .sh)"
    sandbox="$work/$name"
    mkdir -p "$sandbox/tmp"
    t0="$(now_ms)"
    (cd "$sandbox" && TMPDIR="$sandbox/tmp" LBA2_PROFILE_LOG="$work/$name.engine" bash "$t") \
        >"$work/$name.out" 2>&1
    rc=$?
    echo "$(($(now_ms) - t0))" >"$work/$name.ms"
    echo "$rc" >"$work/$name.rc"
    rm -rf "$sandbox"
}

pass=0 skip=0 failn=0 total_ms=0 failed="" results="" overs=""
sha="$(git -C "$DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)"
host="$(uname -s -m)"
mkdir -p "$HISTORY_DIR"
when="$(date -u '+%Y-%m-%dT%H:%MZ')"

# report <fixture>: print its output and verdict, count it, add it to the history.
report() {
    local t="$1" name tier rc ms verdict budget eng=""
    name="$(basename "$t" .sh)" tier="$(tier_of "$t")"
    rc="$(cat "$work/$name.rc")" ms="$(cat "$work/$name.ms")"
    cat "$work/$name.out"
    case "$rc" in
        0) verdict=pass; pass=$((pass + 1)) ;;
        77) verdict=skip; skip=$((skip + 1)) ;;
        *) verdict=FAIL; failn=$((failn + 1)); failed="$failed $name" ;;
    esac
    total_ms=$((total_ms + ms))
    budget="$(budget_s "$tier")"
    # Only one at a time: fixtures running side by side slow each other down, so a time
    # taken at -j 8 says nothing about the fixture's own cost.
    if [ "$jobs" -eq 1 ] && [ -n "$budget" ] && [ "$verdict" != skip ] && [ "$ms" -gt $((budget * 1000)) ]; then
        overs="$overs $name($(secs "$ms")s>${budget}s)"
    fi
    if [ -f "$work/$name.engine" ]; then
        eng="$(awk -F'\t' '{ n++; s += $1 } END { printf "%d runs, %.1fs in the engine", n, s / 1000 }' \
            "$work/$name.engine")"
    fi
    echo "  [$verdict] $name $(secs "$ms")s ($tier)${eng:+, $eng}"
    results="$results$ms"$'\t'"| $name | $tier | $verdict | $(secs "$ms") |"$'\n'
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$when" "$sha" "$host" "$jobs" "$name" "$tier" "$ms" \
        "$verdict" >>"$HISTORY"
}

t_start="$(now_ms)"
if [ "$jobs" -eq 1 ]; then
    for t in "${selected[@]}"; do run_one "$t"; report "$t"; done
else
    running=0
    for t in "${selected[@]}"; do
        run_one "$t" &
        running=$((running + 1))
        if [ "$running" -ge "$jobs" ]; then wait -n; running=$((running - 1)); fi
    done
    wait
    # Reported in name order once all are done, so the log reads the same at any -j.
    while IFS= read -r t; do report "$t"; done < <(printf '%s\n' "${selected[@]}" | sort)
fi
wall_ms=$(($(now_ms) - t_start))

echo "----------------------------------------"
echo "control-harness tests: $pass passed, $skip skipped, $failn failed in $(secs "$wall_ms")s" \
    "(fixtures add up to $(secs "$total_ms")s, -j $jobs)"
[ -n "$failed" ] && echo "failed:$failed"
[ -n "$overs" ] && echo "over budget:$overs"
echo "slowest:"
printf '%s' "$results" | sort -t$'\t' -k1,1nr | head -5 |
    awk -F'\t' '{ split($2, c, " [|] "); sub(/^[|] /, "", c[1]); printf "  %7.1fs  %s\n", $1 / 1000, c[1] }'

full=true
{ [ -n "$tiers" ] || [ ${#names[@]} -gt 0 ]; } && full=false
if [ "$record" = true ] && [ "$full" = true ]; then
    dirty=""
    git -C "$DIR" diff --quiet 2>/dev/null || dirty=" (working tree dirty)"
    {
        echo "<!-- Written by tests/automation/run.sh. -->"
        echo
        echo "# Control-harness last run"
        echo
        echo "- when: $(date -u '+%Y-%m-%d %H:%M UTC')"
        echo "- commit: \`$sha\`$dirty"
        echo "- host: $host"
        echo "- result: **$pass passed, $skip skipped, $failn failed**"
        echo "- time: $(secs "$wall_ms") s with -j $jobs (fixtures add up to $(secs "$total_ms") s)"
        echo
        if [ "$skip" -gt 0 ]; then
            echo "A skip means a prerequisite was missing, not that the fixture agreed with the"
            echo "engine. Only the passes are evidence."
            echo
        fi
        echo "| Fixture | Tier | Result | Seconds |"
        echo "|---|---|---|---|"
        printf '%s' "$results" | cut -f2 | sort
    } >"$RECORD"
    echo "recorded: ${RECORD#"$DIR"/} ($sha)"
elif [ "$record" = true ]; then
    echo "not recorded: a filtered run does not stand for the suite"
fi

[ "$failn" -eq 0 ]
