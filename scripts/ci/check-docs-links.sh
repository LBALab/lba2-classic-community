#!/usr/bin/env bash

set -euo pipefail

# Verify that documentation references resolve. Two checks, because they cover
# two different breakage classes and neither tool sees the other's.
#
#   1. lychee over tracked markdown: relative links and #anchors. Offline by
#      default -- the tree holds ~350 unique external URLs, 332 of them
#      github.com. Checking those needs the network and can only fail for
#      reasons outside the change under review, so it is not a PR gate. Pass
#      --external to check them instead (the weekly job does).
#
#      Shared settings live in lychee.toml at the repo root, so a bare lychee
#      run in a terminal behaves the same way. Only --offline and
#      --no-progress are set here, because only those should differ.
#
#   2. A grep for docs/<name>.md paths mentioned in NON-markdown files. Source
#      comments, tests, scripts and CMake point at docs by bare path, which is
#      not link syntax and so is invisible to a link checker. Moving a doc
#      breaks these silently. Markdown is deliberately excluded here: prose
#      names docs that do not exist yet, and paths belonging to other repos.
#
# Exit non-zero on the first class that fails, after running both.

cd "$(git rev-parse --show-toplevel)"

external=0
if [ "${1:-}" = "--external" ]; then
    external=1
fi

status=0

# --- 1. markdown links and anchors ------------------------------------------
if command -v lychee >/dev/null 2>&1; then
    lychee_args=(--no-progress)
    if [ "$external" -eq 1 ]; then
        echo "Checking external URLs (slow, network-dependent)..."
    else
        lychee_args+=(--offline)
    fi

    if ! git ls-files -z '*.md' | xargs -0 lychee "${lychee_args[@]}"; then
        echo "Link check failed. See the errors above." >&2
        status=1
    fi
elif [ "$external" -eq 1 ]; then
    echo "lychee is required for --external." >&2
    echo "  cargo install lychee, or grab a binary from https://github.com/lycheeverse/lychee/releases" >&2
    exit 1
else
    echo "lychee not found -- skipping the markdown link check (install it to run that half)." >&2
fi

# --- 2. docs/ paths named in source, tests, scripts, CMake ------------------
while IFS= read -r file; do
    while IFS= read -r ref; do
        if [ ! -e "$ref" ]; then
            echo "Dead docs reference: $file -> $ref" >&2
            status=1
        fi
    done < <(grep -ohE '\bdocs/[A-Za-z0-9_/.-]+\.md' "$file" 2>/dev/null || true)
done < <(git ls-files | grep -vE '\.md$')

exit "$status"
