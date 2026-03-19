#!/usr/bin/env bash

set -euo pipefail

if command -v clang-format-17 >/dev/null 2>&1; then
    clang_format=clang-format-17
elif command -v clang-format >/dev/null 2>&1; then
    clang_format=clang-format
elif command -v xcrun >/dev/null 2>&1; then
    clang_format="$(xcrun --find clang-format)"
else
    echo "clang-format is required to run the format check." >&2
    exit 1
fi

status=0

while IFS= read -r -d '' file; do
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
