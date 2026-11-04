#!/usr/bin/env bash

set -euo pipefail

# Verify that tracked C/C++ files already match the repo clang-format policy.
# Like apply-format.sh, this reads exclusions from .clang-format-ignore so the
# local check and CI are evaluating the same file set.

# shellcheck source=scripts/ci/clang-format-select.sh
. "$(dirname "${BASH_SOURCE[0]}")/clang-format-select.sh"
if [ -z "$clang_format" ]; then
    echo "clang-format ${CLANG_FORMAT_MAJOR} is required to run the format check." >&2
    echo "  Debian/Ubuntu: apt-get install clang-format-${CLANG_FORMAT_MAJOR}   macOS: brew install clang-format" >&2
    exit 1
fi

status=0

while IFS= read -r -d '' file; do
    # Skip dirty files so local checks stay focused on committed content.
    if ! git diff --quiet -- "$file" || ! git diff --cached --quiet -- "$file"; then
        continue
    fi

    if ! output=$("$clang_format" --dry-run --Werror -style=file "$file" 2>&1); then
        echo "Format check failed: $file" >&2
        printf '%s\n' "$output" >&2
        status=1
    fi
done < <(git ls-files -z -- '*.c' '*.C' '*.cpp' '*.CPP' '*.h' '*.H' '*.hpp' '*.HPP' | python3 scripts/ci/filter-format-files.py)

exit "$status"
