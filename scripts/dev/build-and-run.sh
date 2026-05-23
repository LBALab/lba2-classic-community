#!/usr/bin/env bash
# Build and run lba2cc from any working directory. Resolves repo root and optional game data.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$("$SCRIPT_DIR/repo_root.sh")"
BUILD_DIR="${LBA2_BUILD_DIR:-$REPO_ROOT/build}"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"

# Resolve names from CMakeCache.txt so this script follows any
# -DLBA2_EXECUTABLE_NAME / -DLBA2_PRODUCT_NAME override on every platform
# (Linux/macOS/msys2). The AppImage env file in build/packaging/ is Linux-only
# and not sourced here.
LBA2_EXECUTABLE_NAME=$(awk -F= '/^LBA2_EXECUTABLE_NAME:[A-Z]+=/{print $2; exit}' \
                          "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || true)
LBA2_EXECUTABLE_NAME="${LBA2_EXECUTABLE_NAME:-lba2cc}"
LBA2_PRODUCT_NAME=$(awk -F= '/^LBA2_PRODUCT_NAME:[A-Z]+=/{print $2; exit}' \
                          "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || true)
LBA2_PRODUCT_NAME="${LBA2_PRODUCT_NAME:-LBA2 Classic Community}"

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

# Resolve the binary to exec. On macOS the build produces an .app bundle
# whose directory is named after LBA2_PRODUCT_NAME, with the inner Mach-O
# renamed back to LBA2_EXECUTABLE_NAME (see SOURCES/CMakeLists.txt). On
# Linux/Windows the binary sits directly under build/SOURCES/.
BIN="$BUILD_DIR/SOURCES/$LBA2_EXECUTABLE_NAME"
if [[ "$(uname -s)" == "Darwin" ]]; then
  MACOS_BIN="$BUILD_DIR/SOURCES/$LBA2_PRODUCT_NAME.app/Contents/MacOS/$LBA2_EXECUTABLE_NAME"
  if [[ -x "$MACOS_BIN" ]]; then
    BIN="$MACOS_BIN"
  fi
fi

exec "$BIN" "$@"
