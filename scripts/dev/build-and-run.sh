#!/usr/bin/env bash
# Build and run lba2 from any working directory. Resolves repo root and optional game data.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$("$SCRIPT_DIR/repo_root.sh")"
BUILD_DIR="${LBA2_BUILD_DIR:-$REPO_ROOT/build}"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"
cmake --build "$BUILD_DIR" --target lba2

GAME_DIR="${LBA2_GAME_DIR:-}"
if [[ -z "$GAME_DIR" ]]; then
  for cand in "$REPO_ROOT/data" "$REPO_ROOT/../LBA2" "$REPO_ROOT/../game"; do
    if [[ -f "$cand/lba2.hqr" ]]; then
      GAME_DIR="$(cd "$cand" && pwd)"
      export LBA2_GAME_DIR="$GAME_DIR"
      break
    fi
  done
fi

exec "$BUILD_DIR/SOURCES/lba2" "$@"
