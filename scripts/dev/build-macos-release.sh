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
LINK_STATIC="ON"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset) PRESET="$2"; shift 2 ;;
        --no-static)
            # Skip LBA2_LINK_STATIC=ON. Use when the host's SDL3 install
            # lacks the static archive (common: brew SDL3 ships .dylib only,
            # SDL3::SDL3-static target isn't exported). The resulting .app
            # still passes through the full bundle pipeline (Info.plist,
            # .icns, hdiutil DMG layout) so you can iterate on packaging
            # logic — but the binary references SDL3.dylib at runtime and
            # is NOT a release-quality artifact. CI release builds remain
            # static (CI uses libsdl-org/setup-sdl, which provides static).
            LINK_STATIC="OFF"
            shift
            ;;
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
echo "[build-macos-release] static:    LBA2_LINK_STATIC=$LINK_STATIC"
echo "[build-macos-release] build dir: $BUILD_DIR"
echo "[build-macos-release] output:    $OUTPUT_DIR"
if [[ "$LINK_STATIC" == "OFF" ]]; then
    echo "[build-macos-release] WARN: --no-static — produced .app references" >&2
    echo "[build-macos-release]       SDL3.dylib at runtime; useful for testing" >&2
    echo "[build-macos-release]       packaging logic, NOT a release artifact." >&2
fi

# Auto-discover Homebrew so `find_package(SDL3 ...)` resolves to a brew
# install — Apple Silicon brew's prefix is /opt/homebrew, Intel brew's
# is /usr/local; CMake doesn't search either by default. Prepended to
# CMAKE_PREFIX_PATH so brew SDL3 wins over anything else on the host.
# CI doesn't need this — it uses libsdl-org/setup-sdl with an explicit
# prefix — so this hint is local-dev-only.
if command -v brew >/dev/null 2>&1; then
    BREW_PREFIX="$(brew --prefix 2>/dev/null || true)"
    if [[ -n "$BREW_PREFIX" ]]; then
        export CMAKE_PREFIX_PATH="${BREW_PREFIX}${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
        echo "[build-macos-release] brew:      $BREW_PREFIX (prepended to CMAKE_PREFIX_PATH)"
    fi
fi

cmake --preset "$PRESET" -DLBA2_LINK_STATIC="$LINK_STATIC"
cmake --build --preset "$PRESET"

# Resolve executable / app names (follow LBA2_EXECUTABLE_NAME and
# LBA2_PRODUCT_NAME overrides). The bundle dir is named after the product
# (display name shown in Finder); the inner Mach-O is named after the
# executable (CLI-friendly, space-free).
EXE_NAME=$(awk -F= '/^LBA2_EXECUTABLE_NAME:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt")
EXE_NAME="${EXE_NAME:-lba2cc}"
PRODUCT_NAME=$(awk -F= '/^LBA2_PRODUCT_NAME:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt")
PRODUCT_NAME="${PRODUCT_NAME:-LBA2 Classic Community}"
APP_PATH="$BUILD_DIR/SOURCES/${PRODUCT_NAME}.app"

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
