#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build LBA2 Classic Community for Android
#
# Supports arm64-v8a (default) and armeabi-v7a (via --abi armeabi-v7a).
#
# Prerequisites:
#   1. Android NDK r26+  — set ANDROID_NDK or install under $HOME/Android/Sdk/ndk/
#   2. SDL3 built for the target ABI — set SDL3_ANDROID_DIR or run build_sdl3.sh first
#   3. Ninja (build tool)
#   4. Java 17+ and Gradle for APK packaging (optional: use android/gradlew)
#   5. Retail game data HQR files (NOT included in the repo)
#
# Usage:
#   export ANDROID_NDK=$HOME/Android/Sdk/ndk/26.1.10909125
#   export SDL3_ANDROID_DIR=$PWD/out/android/sdl3-install-arm64
#   bash android/build_android.sh                         # arm64-v8a (default)
#   bash android/build_android.sh --abi armeabi-v7a        # 32-bit
#
# Output:
#   out/build/android_{abi}/SOURCES/liblba2cc.so  — native game library
# ---------------------------------------------------------------------------
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# ---- Config ---------------------------------------------------------------
ANDROID_NDK="${ANDROID_NDK:-${HOME}/Android/Sdk/ndk/26.1.10909125}"
SDL3_ANDROID_DIR="${SDL3_ANDROID_DIR:-${REPO_DIR}/out/android/sdl3-install}"
ABI="${ABI:-arm64-v8a}"
API_LEVEL=24

# Parse --abi override
while [[ $# -gt 0 ]]; do
    case "$1" in
        --abi) ABI="$2"; shift 2 ;;
        -h|--help)
            sed -n '/^# Usage:/,/^set -e/p' "$0" | sed 's/^# \?//' | head -n -1
            exit 0
            ;;
        *) echo "Unknown: $1" >&2; exit 2 ;;
    esac
done

# Map ABI → preset name
case "$ABI" in
    arm64-v8a)   PRESET="android_arm64" ;;
    armeabi-v7a) PRESET="android_armv7a" ;;
    *) echo "Unsupported ABI: $ABI (use arm64-v8a or armeabi-v7a)" >&2; exit 2 ;;
esac

BUILD_DIR="${REPO_DIR}/out/build/${PRESET}"

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
    -DANDROID_STL=c++_static \
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
