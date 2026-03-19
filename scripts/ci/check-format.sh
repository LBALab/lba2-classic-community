#!/usr/bin/env bash

set -euo pipefail

if command -v clang-format >/dev/null 2>&1; then
    clang_format=clang-format
elif command -v clang-format-18 >/dev/null 2>&1; then
    clang_format=clang-format-18
elif command -v clang-format-17 >/dev/null 2>&1; then
    clang_format=clang-format-17
else
    echo "clang-format is required to run the format check." >&2
    exit 1
fi

status=0

while IFS= read -r -d '' file; do
    case "$file" in
        LIB386/libsmacker/*|build/*|build_test/*|build_logs/*|out/*)
            continue
            ;;
    esac

    "$clang_format" --dry-run --Werror -style=file "$file" || status=1
done < <(git ls-files -z -- '*.c' '*.C' '*.cpp' '*.CPP' '*.h' '*.H' '*.hpp' '*.HPP')

exit "$status"