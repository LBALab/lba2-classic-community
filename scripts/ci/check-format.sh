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
    case "$file" in
        LIB386/libsmacker/*|LIB386/AIL/SDL/stb_vorbis.c|LIB386/AIL/SDL/stb_vorbis.h|LIB386/FILEIO/stb_image_write.h|SOURCES/3DEXT/LBA_EXT.H|SOURCES/3DEXT/MAPTOOLS.CPP|SOURCES/COMMON.H|SOURCES/DEC.CPP|SOURCES/DEC_XCF.CPP|SOURCES/EXTRA.CPP|build/*|build_test/*|build_logs/*|out/*)
            continue
            ;;
    esac

    if ! git diff --quiet -- "$file" || ! git diff --cached --quiet -- "$file"; then
        continue
    fi

    "$clang_format" --dry-run --Werror -style=file "$file" || status=1
done < <(git ls-files -z -- '*.c' '*.C' '*.cpp' '*.CPP' '*.h' '*.H' '*.hpp' '*.HPP')

exit "$status"