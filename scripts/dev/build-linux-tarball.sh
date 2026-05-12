#!/usr/bin/env bash
# Local dry-run for the Linux binary tarball release artifact.
#
# Builds a statically-linked Linux binary using the `linux` preset with
# -DLBA2_LINK_STATIC=ON, then bundles it into a portable .tar.gz via
# scripts/packaging/bundle-linux-tarball.sh.
#
# Outputs: dist/lba2cc-<version>-linux-<arch>.tar.gz
#
# Same packaging script the CI release workflow
# (.github/workflows/release-linux-tarball.yml) invokes, so layout cannot
# drift between local dry-run and CI.
#
# Static SDL3 caveat: most distros ship SDL3 as a shared library only,
# and -DLBA2_LINK_STATIC=ON requires the SDL3-static target. With only a
# shared SDL3 installed, `find_package(SDL3)` will fail outright. To
# produce a truly static binary locally, build SDL3 from source with
# -DSDL_STATIC=ON -DSDL_SHARED=OFF and pass its prefix via the
# CMAKE_PREFIX_PATH env var (CMake picks it up natively):
#
#     git clone --depth 1 --branch release-3.2.16 https://github.com/libsdl-org/SDL /tmp/SDL
#     cmake -S /tmp/SDL -B /tmp/SDL/build -G Ninja \
#           -DCMAKE_BUILD_TYPE=Release \
#           -DSDL_STATIC=ON -DSDL_SHARED=OFF \
#           -DCMAKE_INSTALL_PREFIX=/tmp/sdl3-static-prefix
#     cmake --build /tmp/SDL/build && cmake --install /tmp/SDL/build
#     CMAKE_PREFIX_PATH=/tmp/sdl3-static-prefix bash scripts/dev/build-linux-tarball.sh
#
# CI does the same dance via libsdl-org/setup-sdl in
# .github/workflows/release-linux-tarball.yml.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
cd "$REPO_ROOT"

PRESET="linux"
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

ARCH="$(uname -m)"

BUILD_DIR="${LBA2_BUILD_DIR:-out/build/$PRESET}"
OUTPUT_DIR="${LBA2_DIST_DIR:-dist}"

echo "[build-linux-tarball] preset:    $PRESET"
echo "[build-linux-tarball] arch:      $ARCH"
echo "[build-linux-tarball] static:    LBA2_LINK_STATIC=ON"
echo "[build-linux-tarball] build dir: $BUILD_DIR"
echo "[build-linux-tarball] output:    $OUTPUT_DIR"

cmake --preset "$PRESET" -DLBA2_LINK_STATIC=ON
cmake --build --preset "$PRESET"

# Resolve the executable name from the cache (follows any
# -DLBA2_EXECUTABLE_NAME override).
EXE_NAME=$(awk -F= '/^LBA2_EXECUTABLE_NAME:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt")
EXE_NAME="${EXE_NAME:-lba2cc}"
EXE_PATH="$BUILD_DIR/SOURCES/${EXE_NAME}"

if [[ ! -f "$EXE_PATH" ]]; then
    echo "[build-linux-tarball] expected $EXE_PATH but it doesn't exist" >&2
    exit 1
fi

VERSION=$(< "$BUILD_DIR/VERSION.txt")

mkdir -p "$OUTPUT_DIR"

bash "$REPO_ROOT/scripts/packaging/bundle-linux-tarball.sh" \
    --exe "$EXE_PATH" \
    --version "$VERSION" \
    --arch "$ARCH" \
    --build-dir "$BUILD_DIR" \
    --output-dir "$OUTPUT_DIR"
