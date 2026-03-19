#!/usr/bin/env bash

set -euo pipefail

if command -v clang-format-17 >/dev/null 2>&1; then
    clang_format=clang-format-17
elif command -v clang-format >/dev/null 2>&1; then
    clang_format=clang-format
elif command -v xcrun >/dev/null 2>&1; then
    clang_format="$(xcrun --find clang-format)"
else
    echo "clang-format is required to apply formatting." >&2
    exit 1
fi

while IFS= read -r -d '' file; do
    if ! git diff --quiet -- "$file" || ! git diff --cached --quiet -- "$file"; then
        continue
    fi

    "$clang_format" -i "$file"
done < <(git ls-files -z -- '*.c' '*.C' '*.cpp' '*.CPP' '*.h' '*.H' '*.hpp' '*.HPP' | python3 scripts/ci/filter-format-files.py)
