#!/usr/bin/env bash

set -euo pipefail

# Build lba2cc with -Wall -Wextra and compare the result against the checked-in
# baseline, so a change can remove warnings but not add them.
#
# Usage:
#   scripts/ci/warning-baseline.sh            compare against the baseline (CI mode)
#   scripts/ci/warning-baseline.sh --update   rewrite the baseline from this build
#
# The baseline counts diagnostics per (file, flag), never per line. Line numbers
# move whenever anything above them is edited, so a line-keyed baseline goes
# stale on contact and contributors learn to regenerate it without reading it.
# A count only moves when the number of warnings in that file actually changes.
#
# Counting is over unique file:line:column:flag, not raw diagnostic lines: a
# warning in a header is re-reported by every translation unit that includes it,
# and that multiplier changes when an unrelated file adds an include.

repo_root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$repo_root"

baseline="scripts/ci/warnings-baseline.txt"
build_dir="${WARNING_BASELINE_BUILD_DIR:-build-warnings}"
mode="check"
[ "${1:-}" = "--update" ] && mode="update"

# Suppressed on purpose, with the count each was contributing when the baseline
# was introduced:
#
#   unknown-pragmas (138)          Watcom `#pragma aux` register-calling
#                                  conventions. Dead text to gcc and clang, and
#                                  removing them would lose the record of how the
#                                  1997 build passed arguments.
#   unused-parameter (72)          A faithful port keeps a signature even where
#                                  this build ignores an argument.
#   missing-field-initializers (10)  Partial aggregate initialisation is
#                                  pervasive and deliberate here.
#
# Everything else -Wall -Wextra reports is in the baseline and gated.
warn_flags="-Wall -Wextra -Wno-unknown-pragmas -Wno-unused-parameter -Wno-missing-field-initializers"

# A fresh configure and build every time. An incremental build only recompiles
# what changed, so it reports a fraction of the warnings and would silently
# shrink the comparison to whatever the last edit touched.
rm -rf "$build_dir"
mkdir -p "$build_dir"
cmake -S . -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLBA2_BUILD_TESTS=OFF \
    -DLBA2_BUILD_ASM_EQUIV_TESTS=OFF \
    -DCMAKE_C_FLAGS="$warn_flags" \
    -DCMAKE_CXX_FLAGS="$warn_flags" \
    --preset linux > "$build_dir/configure.log" 2>&1 ||
    { echo "configure failed; see $build_dir/configure.log" >&2; exit 1; }

cmake --build "$build_dir" --target lba2cc -j"$(nproc)" > "$build_dir/build.log" 2>&1 ||
    { echo "build failed; see $build_dir/build.log" >&2; exit 1; }

# Normalise to "<count> <file> <flag>":
#   - absolute build paths back to repo-relative
#   - vendored trees dropped (libsmacker, the stb single-file libraries)
#   - generated sources dropped: they live in the build dir, so their path
#     changes with it and they are not something a contributor can edit
#   - unique file:line:column:flag, then counted per file and flag
current=$(mktemp)
grep -E '^/.*warning:' "$build_dir/build.log" |
    sed -E "s#^${repo_root}/##" |
    grep -vE '^(LIB386/libsmacker/|LIB386/AIL/SDL/stb_|/)' |
    sed -E 's/^([^:]+:[0-9]+:[0-9]+): warning: .*(\[-W[a-z0-9=+-]+\])$/\1 \2/' |
    grep -E '\[-W[a-z0-9=+-]+\]$' |
    sort -u |
    sed -E 's/^([^:]+):[0-9]+:[0-9]+ (\[-W[a-z0-9=+-]+\])$/\1 \2/' |
    sort | uniq -c | awk '{printf "%s %s %s\n", $1, $2, $3}' | sort -k2,2 -k3,3 > "$current"

total=$(awk '{s+=$1} END {print s+0}' "$current")

if [ "$mode" = "update" ]; then
    {
        echo "# Baseline of -Wall -Wextra diagnostics in lba2cc, one line per file and flag."
        echo "#"
        echo "# Regenerate with: scripts/ci/warning-baseline.sh --update"
        echo "# The gate fails when a count rises or a new (file, flag) pair appears."
        echo "# Lowering a count is the point: fix warnings, then regenerate."
        echo "#"
        echo "# Counts are compiler-specific. Generated with: $(${CC:-gcc} --version | head -1)"
        echo "# total: $total"
        echo ""
        cat "$current"
    } > "$baseline"
    echo "wrote $baseline ($total diagnostics)"
    exit 0
fi

if [ ! -f "$baseline" ]; then
    echo "No baseline at $baseline. Create one with: $0 --update" >&2
    exit 1
fi

# Warning sets move between compiler releases, so a version mismatch shows up as
# a wall of NEW lines that look like a regression and are not one. Say so before
# the diff rather than leaving someone to work it out from the list.
baseline_cc=$(sed -n 's/^# Counts are compiler-specific. Generated with: //p' "$baseline")
current_cc=$(${CC:-gcc} --version | head -1)
if [ -n "$baseline_cc" ] && [ "$baseline_cc" != "$current_cc" ]; then
    echo "note: baseline was generated with a different compiler." >&2
    echo "      baseline: $baseline_cc" >&2
    echo "      current:  $current_cc" >&2
    echo "      Differences below may be the compiler, not the change." >&2
    echo >&2
fi

expected=$(mktemp)
grep -vE '^\s*(#|$)' "$baseline" > "$expected"

status=0
while read -r count file flag; do
    was=$(awk -v f="$file" -v g="$flag" '$2==f && $3==g {print $1}' "$expected")
    if [ -z "$was" ]; then
        echo "NEW      $file $flag ($count)" >&2
        status=1
    elif [ "$count" -gt "$was" ]; then
        echo "INCREASE $file $flag ($was -> $count)" >&2
        status=1
    fi
done < "$current"

while read -r count file flag; do
    now=$(awk -v f="$file" -v g="$flag" '$2==f && $3==g {print $1}' "$current")
    now=${now:-0}
    [ "$now" -lt "$count" ] && echo "improved $file $flag ($count -> $now)"
done < "$expected"

if [ "$status" -ne 0 ]; then
    cat >&2 <<EOF

This change adds compiler warnings that were not in the baseline.

Fix them, or if the new warning is genuinely wanted, regenerate with
  scripts/ci/warning-baseline.sh --update
and say in the commit message why the count went up.
EOF
    exit 1
fi

echo "no new warnings ($total diagnostics, baseline unchanged or improved)"
