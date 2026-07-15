#!/usr/bin/env bash

set -euo pipefail

# Apply the repo clang-format policy to tracked C/C++ files.
# The file list is filtered through .clang-format-ignore so local formatting
# and CI checks stay aligned on what is intentionally excluded.

# shellcheck source=scripts/ci/clang-format-select.sh
. "$(dirname "${BASH_SOURCE[0]}")/clang-format-select.sh"
if [ -z "$clang_format" ]; then
    echo "clang-format ${CLANG_FORMAT_MAJOR} is required to apply formatting." >&2
    echo "  Debian/Ubuntu: apt-get install clang-format-${CLANG_FORMAT_MAJOR}   macOS: brew install clang-format" >&2
    exit 1
fi

while IFS= read -r -d '' file; do
    # Leave already-modified files alone so the script can be used safely in a
    # dirty worktree without mixing formatting into unrelated edits.
    if ! git diff --quiet -- "$file" || ! git diff --cached --quiet -- "$file"; then
        continue
    fi

    "$clang_format" -i "$file"
# The file list is filtered through .clang-format-ignore so local formatting
done < <(git ls-files -z -- '*.c' '*.C' '*.cpp' '*.CPP' '*.h' '*.H' '*.hpp' '*.HPP' | python3 scripts/ci/filter-format-files.py)
