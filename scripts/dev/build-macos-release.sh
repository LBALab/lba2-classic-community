#!/usr/bin/env bash
# Local dry-run for the macOS release artifact.
#
# Detects host architecture by default and picks the matching preset
# (macos_arm64 on Apple Silicon, macos_x86_64 on Intel). Override with
# --preset to force the other arch (cross-arch build via --preset
# macos_x86_64 on Apple Silicon works only if you have an x86_64 SDK
# installed and Rosetta-friendly tooling — usually fine on a recent Xcode).
#
# Outputs: dist/lba2cc-<version>-macos-<arch>.dmg
#
# Both this script and the eventual CI release workflow call into
# scripts/packaging/bundle-macos.sh, so the DMG layout can't drift.
#
# Requires macOS host (uses hdiutil + sips + iconutil — all macOS-native).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "build-macos-release.sh: must run on macOS (Darwin)." >&2
    echo "Detected: $(uname -s). Use scripts/dev/build-windows-release.sh" >&2
    echo "or scripts/packaging/make-appimage.sh for the other platforms." >&2
    exit 1
fi

PRESET=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset) PRESET="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,/^set -e/p' "$0" | sed 's/^# \?//' | head -n -1
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

# Auto-detect arch if preset not specified.
if [[ -z "$PRESET" ]]; then
    case "$(uname -m)" in
        arm64)  PRESET="macos_arm64";  ARCH="arm64"  ;;
        x86_64) PRESET="macos_x86_64"; ARCH="x86_64" ;;
        *)
            echo "Unsupported macOS arch: $(uname -m). Pass --preset explicitly." >&2
            exit 1
            ;;
    esac
else
    case "$PRESET" in
        macos_arm64)  ARCH="arm64"  ;;
        macos_x86_64) ARCH="x86_64" ;;
        *)
            echo "Unsupported preset: $PRESET" >&2
            exit 1
            ;;
    esac
fi

BUILD_DIR="${LBA2_BUILD_DIR:-out/build/$PRESET}"
OUTPUT_DIR="${LBA2_DIST_DIR:-dist}"

echo "[build-macos-release] preset:    $PRESET"
echo "[build-macos-release] arch:      $ARCH"
echo "[build-macos-release] static:    LBA2_LINK_STATIC=ON"
echo "[build-macos-release] build dir: $BUILD_DIR"
echo "[build-macos-release] output:    $OUTPUT_DIR"

cmake --preset "$PRESET" -DLBA2_LINK_STATIC=ON
cmake --build --preset "$PRESET"

# Resolve executable / app names (follow LBA2_EXECUTABLE_NAME overrides).
EXE_NAME=$(awk -F= '/^LBA2_EXECUTABLE_NAME:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt")
EXE_NAME="${EXE_NAME:-lba2cc}"
APP_PATH="$BUILD_DIR/SOURCES/${EXE_NAME}.app"

if [[ ! -d "$APP_PATH" ]]; then
    echo "[build-macos-release] expected $APP_PATH but it doesn't exist" >&2
    echo "[build-macos-release] (CMake's MACOSX_BUNDLE wiring should have produced it)" >&2
    exit 1
fi

VERSION=$(< "$BUILD_DIR/VERSION.txt")

mkdir -p "$OUTPUT_DIR"

bash "$REPO_ROOT/scripts/packaging/bundle-macos.sh" \
    --app "$APP_PATH" \
    --version "$VERSION" \
    --arch "$ARCH" \
    --build-dir "$BUILD_DIR" \
    --output-dir "$OUTPUT_DIR"
