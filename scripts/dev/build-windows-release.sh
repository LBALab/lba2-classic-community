#!/usr/bin/env bash
# Local dry-run for the Windows release artifact.
#
# Auto-detects the build environment and picks a preset:
#
#   MSYS2 (UCRT64 / MINGW64)   → windows_ucrt64 preset, x64 artifact.
#                                 Mirrors the CI release flow exactly.
#   Linux (incl. WSL)          → cross_linux2win preset, i686 artifact.
#                                 Useful for iterating on packaging logic;
#                                 requires mingw-w64 toolchain + SDL3 for
#                                 the cross-arch (most distros only ship
#                                 SDL3 for the host arch — CI uses
#                                 setup-sdl to handle this).
#
# Override the auto-detection with --preset <name>, e.g. for testing the
# 64-bit MinGW preset on MSYS2 instead of UCRT64.
#
# Outputs: dist/lba2cc-<version>-windows-<arch>.zip
#
# Both paths run the same scripts/packaging/bundle-windows.sh, so the ZIP
# layout can't drift. The CI release workflow (B2, separate PR) calls the
# same bundle script.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
cd "$REPO_ROOT"

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

# Auto-detect preset if not specified.
if [[ -z "$PRESET" ]]; then
    if [[ -n "${MSYSTEM:-}" ]]; then
        # On MSYS2 — use the native preset.
        case "$MSYSTEM" in
            UCRT64)  PRESET="windows_ucrt64" ;;
            MINGW64) PRESET="windows_mingw64" ;;
            *)
                echo "Unsupported MSYSTEM: $MSYSTEM" >&2
                echo "Supported on MSYS2: UCRT64, MINGW64. Pass --preset explicitly to override." >&2
                exit 1
                ;;
        esac
    else
        # On Linux (incl. WSL).
        PRESET="cross_linux2win"
    fi
fi

# Map preset → arch label and toolchain prerequisite check.
case "$PRESET" in
    cross_linux2win)
        ARCH="i686"
        if ! command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
            echo "i686-w64-mingw32-gcc not found." >&2
            echo "Install with: sudo apt install mingw-w64    (Debian/Ubuntu/WSL)" >&2
            echo "         or:  sudo pacman -S mingw-w64-gcc  (Arch)" >&2
            exit 1
        fi
        echo "[build-windows-release] note: cross_linux2win also needs SDL3 for i686." >&2
        echo "[build-windows-release]       most distros ship only host-arch SDL3 — if" >&2
        echo "[build-windows-release]       configure fails on find_package(SDL3), build" >&2
        echo "[build-windows-release]       SDL3-i686 from source or use MSYS2 UCRT64 instead." >&2
        ;;
    windows_ucrt64|windows_mingw64)
        ARCH="x64"
        ;;
    *)
        echo "[build-windows-release] using preset $PRESET (arch label inferred as host)" >&2
        ARCH="$(uname -m)"
        ;;
esac

BUILD_DIR="${LBA2_BUILD_DIR:-out/build/$PRESET}"
OUTPUT_DIR="${LBA2_DIST_DIR:-dist}"

echo "[build-windows-release] preset:    $PRESET"
echo "[build-windows-release] arch:      $ARCH"
echo "[build-windows-release] static:    LBA2_LINK_STATIC=ON"
echo "[build-windows-release] build dir: $BUILD_DIR"
echo "[build-windows-release] output:    $OUTPUT_DIR"

# Override LBA2_LINK_STATIC=ON via cache override — the preset stays
# unchanged so it remains a fast iteration target for anyone who just
# wants to confirm Windows builds.
cmake --preset "$PRESET" -DLBA2_LINK_STATIC=ON
cmake --build --preset "$PRESET"

# Resolve the executable name from the cache (follows any
# -DLBA2_EXECUTABLE_NAME override).
EXE_NAME=$(awk -F= '/^LBA2_EXECUTABLE_NAME:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt")
EXE_NAME="${EXE_NAME:-lba2cc}"
EXE_PATH="$BUILD_DIR/SOURCES/${EXE_NAME}.exe"

if [[ ! -f "$EXE_PATH" ]]; then
    echo "[build-windows-release] expected $EXE_PATH but it doesn't exist" >&2
    exit 1
fi

VERSION=$(< "$BUILD_DIR/VERSION.txt")

mkdir -p "$OUTPUT_DIR"

bash "$REPO_ROOT/scripts/packaging/bundle-windows.sh" \
    --exe "$EXE_PATH" \
    --version "$VERSION" \
    --arch "$ARCH" \
    --build-dir "$BUILD_DIR" \
    --output-dir "$OUTPUT_DIR"
