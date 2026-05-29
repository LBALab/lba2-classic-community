#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build LBA2 Classic Community for Android (arm64-v8a)
#
# Prerequisites:
#   1. Android NDK r26+  — set ANDROID_NDK or install under $HOME/Android/Sdk/ndk/
#   2. SDL3 built for Android arm64 — set SDL3_ANDROID_DIR or run build_sdl3.sh first
#   3. Ninja (build tool)
#   4. Java 17+ and Gradle for APK packaging (optional: use android/gradlew)
#   5. Retail game data HQR files (NOT included in the repo)
#
# Usage:
#   export ANDROID_NDK=$HOME/Android/Sdk/ndk/26.1.10909125
#   export SDL3_ANDROID_DIR=$PWD/out/android/sdl3-install
#   bash android/build_android.sh
#
# Output:
#   out/build/android_arm64/SOURCES/liblba2cc.so  — native game library
#   android/app/build/outputs/apk/debug/          — APK (if gradle step runs)
# ---------------------------------------------------------------------------
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# ---- Config ---------------------------------------------------------------
ANDROID_NDK="${ANDROID_NDK:-${HOME}/Android/Sdk/ndk/26.1.10909125}"
SDL3_ANDROID_DIR="${SDL3_ANDROID_DIR:-${REPO_DIR}/out/android/sdl3-install}"
BUILD_DIR="${REPO_DIR}/out/build/android_arm64"
PRESET="android_arm64"
ABI="arm64-v8a"
API_LEVEL=24

echo "=== LBA2 Android build ==="
echo "  NDK:       ${ANDROID_NDK}"
echo "  SDL3:      ${SDL3_ANDROID_DIR}"
echo "  Build:     ${BUILD_DIR}"

# ---- Step 1: CMake configure ---------------------------------------------
cmake -S "${REPO_DIR}" -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="${ABI}" \
    -DANDROID_PLATFORM="${API_LEVEL}" \
    -DANDROID_STL=c++_shared \
    -DSDL3_DIR="${SDL3_ANDROID_DIR}/lib/cmake/SDL3" \
    -DSOUND_BACKEND=sdl \
    -DMVIDEO_BACKEND=smacker

# ---- Step 2: Build native library ----------------------------------------
cmake --build "${BUILD_DIR}" -- -j$(nproc)

echo ""
echo "=== Native library built ==="
echo "  ${BUILD_DIR}/SOURCES/liblba2cc.so"
ls -lh "${BUILD_DIR}/SOURCES/liblba2cc.so" 2>/dev/null || \
    ls -lh "${BUILD_DIR}/SOURCES/liblba2cc.so" 2>/dev/null || \
    echo "(check BUILD_DIR for output)"

# ---- Step 3: (Optional) Build APK via Gradle -----------------------------
if command -v gradle &>/dev/null || [ -f "${SCRIPT_DIR}/gradlew" ]; then
    echo ""
    echo "=== Packaging APK ==="

    # Copy native lib to the Android project's jniLibs
    mkdir -p "${SCRIPT_DIR}/app/src/main/jniLibs/${ABI}"
    cp "${BUILD_DIR}/SOURCES/liblba2cc.so" \
       "${SCRIPT_DIR}/app/src/main/jniLibs/${ABI}/"

    # Build the APK
    cd "${SCRIPT_DIR}"
    if [ -f gradlew ]; then
        ./gradlew assembleDebug
    else
        gradle assembleDebug
    fi

    echo ""
    echo "=== APK ready ==="
    echo "  ${SCRIPT_DIR}/app/build/outputs/apk/debug/"
else
    echo ""
    echo "=== Gradle not found — native lib built, APK skipped ==="
    echo "  To build APK: install Gradle or run gradle wrapper."
    echo "  See android/README.md for manual APK packaging instructions."
fi
