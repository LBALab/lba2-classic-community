#!/usr/bin/env bash
# Build and run lba2 from any working directory. Resolves repo root and optional game data.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$("$SCRIPT_DIR/repo_root.sh")"
BUILD_DIR="${LBA2_BUILD_DIR:-$REPO_ROOT/build}"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"

# Resolve the executable name from CMakeCache.txt so this script follows any
# -DLBA2_EXECUTABLE_NAME override on every platform (Linux/macOS/msys2). The
# AppImage env file in build/packaging/ is Linux-only and not sourced here.
LBA2_EXECUTABLE_NAME=$(awk -F= '/^LBA2_EXECUTABLE_NAME:[A-Z]+=/{print $2; exit}' \
                          "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || true)
LBA2_EXECUTABLE_NAME="${LBA2_EXECUTABLE_NAME:-lba2}"

cmake --build "$BUILD_DIR"

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

exec "$BUILD_DIR/SOURCES/$LBA2_EXECUTABLE_NAME" "$@"
