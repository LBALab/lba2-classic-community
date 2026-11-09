#!/usr/bin/env bash
# Build the experimental UWP exe (Xbox in Developer Mode) with a pinned llvm-mingw and check it.
# Usage: bash scripts/dev/build-uwp.sh   (LLVM_MINGW_ROOT set: use that toolchain instead)
# Output: out/build/uwp-ci/SOURCES/lba2cc.exe
set -euo pipefail

if [ "$#" -ne 0 ]; then
    if [ "$#" -eq 1 ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
        sed -n '2,4s/^# //p' "$0"
        exit 0
    fi
    echo "Usage: bash scripts/dev/build-uwp.sh" >&2
    exit 2
fi

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# One release, by its published SHA-256 digests.
VERSION=20261104
case "$(uname -s)-$(uname -m)" in
Linux-x86_64)
    ASSET="llvm-mingw-$VERSION-ucrt-ubuntu-22.04-x86_64"
    DIGEST=919380403de87dca3c581c2053230cd298af5e891286628ba0613b7fed370926
    ;;
Darwin-*)
    ASSET="llvm-mingw-$VERSION-ucrt-macos-universal"
    DIGEST=66cbd5178c4a71b0d473e68ede172f8eb7d15d023e67f39201d0a69e21f6bf3c
    ;;
*)
    echo "No pinned llvm-mingw for $(uname -s)-$(uname -m); set LLVM_MINGW_ROOT." >&2
    [ -n "${LLVM_MINGW_ROOT:-}" ] || exit 1
    ;;
esac

sha256() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
    else
        shasum -a 256 "$1" | cut -d' ' -f1
    fi
}

if [ -z "${LLVM_MINGW_ROOT:-}" ]; then
    TOOLS="$REPO_DIR/out/toolchains"
    LLVM_MINGW_ROOT="$TOOLS/$ASSET"
    if [ ! -x "$LLVM_MINGW_ROOT/bin/x86_64-w64-mingw32uwp-clang" ]; then
        mkdir -p "$TOOLS"
        tarball="$TOOLS/$ASSET.tar.xz.part"
        echo "Downloading $ASSET"
        curl -fsSL --retry 3 --retry-delay 2 --retry-all-errors --connect-timeout 10 --max-time 600 \
            -o "$tarball" "https://github.com/mstorsjo/llvm-mingw/releases/download/$VERSION/$ASSET.tar.xz"
        actual="$(sha256 "$tarball")"
        if [ "$actual" != "$DIGEST" ]; then
            echo "$ASSET.tar.xz: SHA-256 $actual, expected $DIGEST" >&2
            rm -f "$tarball"
            exit 1
        fi
        tar -xJf "$tarball" -C "$TOOLS"
        rm -f "$tarball"
    fi
fi
export LLVM_MINGW_ROOT
echo "llvm-mingw: $LLVM_MINGW_ROOT"

BUILD="$REPO_DIR/out/build/uwp-ci"
cmake -S "$REPO_DIR" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$REPO_DIR/cmake/llvm-mingw-x86_64-uwp.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPLATFORM_BACKEND=uwp -DSOUND_BACKEND=null -DMVIDEO_BACKEND=null \
    -DLBA2_BUILD_TESTS=OFF -DLBA2_BUILD_SAVE_TOOLS=OFF
cmake --build "$BUILD" --target lba2cc

CHECK="$REPO_DIR/scripts/ci/check-uwp-build.py"
python3 "$CHECK" selftest --toolchain "$LLVM_MINGW_ROOT"
python3 "$CHECK" table
python3 "$CHECK" imports --toolchain "$LLVM_MINGW_ROOT" "$BUILD/SOURCES/lba2cc.exe"
