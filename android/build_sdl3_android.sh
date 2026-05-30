#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build SDL3 for Android (arm64-v8a) and install to a known prefix.
#
# Prerequisites: Android NDK r26+, Ninja, git.
#
# Usage:
#   export ANDROID_NDK=$HOME/Android/Sdk/ndk/26.1.10909125
#   bash android/build_sdl3_android.sh
#
# Output:
#   out/android/sdl3-install/  — SDL3 headers, libs, and CMake config
# ---------------------------------------------------------------------------
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="${REPO_DIR}/out/android"
SDL3_SRC="${OUT_DIR}/SDL3"
SDL3_BUILD="${OUT_DIR}/sdl3-build"
SDL3_INSTALL="${OUT_DIR}/sdl3-install"

ANDROID_NDK="${ANDROID_NDK:-${HOME}/Android/Sdk/ndk/26.1.10909125}"

if [ ! -d "${ANDROID_NDK}" ]; then
    echo "ERROR: ANDROID_NDK not found at ${ANDROID_NDK}"
    echo "Set ANDROID_NDK to your NDK path or install it via Android Studio."
    exit 1
fi

echo "=== Building SDL3 for Android ==="
echo "  NDK:       ${ANDROID_NDK}"
echo "  Source:    ${SDL3_SRC}"
echo "  Install:   ${SDL3_INSTALL}"

# Clone SDL3 if not present
if [ ! -d "${SDL3_SRC}" ]; then
    echo "Cloning SDL3..."
    git clone --depth 1 https://github.com/libsdl-org/SDL.git -b main "${SDL3_SRC}"
fi

# Configure
cmake -S "${SDL3_SRC}" -B "${SDL3_BUILD}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=24 \
    -DANDROID_STL=c++_static \
    -DSDL_SHARED=ON \
    -DSDL_STATIC=OFF \
    -DSDL_TEST=OFF

# Build
cmake --build "${SDL3_BUILD}" -- -j$(nproc)

# Install
cmake --install "${SDL3_BUILD}" --prefix "${SDL3_INSTALL}"

echo ""
echo "=== SDL3 for Android built ==="
echo "  Export SDL3_ANDROID_DIR=${SDL3_INSTALL}"
echo "  Then run: bash android/build_android.sh"
