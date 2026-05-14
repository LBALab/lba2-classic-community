#!/usr/bin/env bash

set -euo pipefail

# Run clang-tidy across the project's own C/C++ sources for memory-safety and
# undefined-behaviour checks. Scope and rationale live in .clang-tidy.
#
# Requires a configured build directory with compile_commands.json
# (CMAKE_EXPORT_COMPILE_COMMANDS is ON by default in CMakeLists.txt).
#
# Usage: scripts/ci/run-clang-tidy.sh [build-dir]   (default: build)

build_dir="${1:-build}"

if [ ! -f "$build_dir/compile_commands.json" ]; then
    echo "No compile_commands.json in '$build_dir'." >&2
    echo "Configure a build first, e.g.: cmake -B build" >&2
    exit 1
fi

if command -v run-clang-tidy >/dev/null 2>&1; then
    runner=run-clang-tidy
elif command -v run-clang-tidy-18 >/dev/null 2>&1; then
    runner=run-clang-tidy-18
else
    echo "run-clang-tidy is required (part of the clang-tools-extra package)." >&2
    exit 1
fi

# Tracked project sources, minus vendored third-party trees (libsmacker and the
# stb single-file libraries). Vendored headers are excluded via .clang-tidy's
# HeaderFilterRegex.
mapfile -d '' files < <(
    git ls-files -z -- '*.c' '*.C' '*.cpp' '*.CPP' \
        | grep -zv -e '^LIB386/libsmacker/' -e '^LIB386/AIL/SDL/stb_vorbis\.c$'
)

if [ "${#files[@]}" -eq 0 ]; then
    echo "No files to analyze." >&2
    exit 0
fi

"$runner" -p "$build_dir" -quiet "${files[@]}"
