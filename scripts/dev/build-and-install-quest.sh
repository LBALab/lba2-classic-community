#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# LBA2 Classic Community — Quest build and install wizard
#
# Builds the Quest preset, bundles a debug-signed APK, pushes the retail
# HQR files to /sdcard/lba2cc/ on the headset, and installs the APK via ADB.
#
# Usage:
#   bash scripts/dev/build-and-install-quest.sh
#
# Prerequisites:
#   - Android NDK r28+  ($ANDROID_NDK)
#   - SDL3 for Android  ($SDL3_ANDROID_DIR)
#   - Android SDK with build-tools / platform android-34 ($ANDROID_HOME)
#   - ADB in PATH, Quest in developer mode
# ---------------------------------------------------------------------------
set -uo pipefail

# Re-exec inside a persistent shell when launched by a file manager so the
# window stays open long enough to read errors.
if [ -z "${LBA2_QUEST_KEEP_OPEN:-}" ]; then
  export LBA2_QUEST_KEEP_OPEN=1
  exec bash -c "bash '$0' \"\$@\"; echo; echo 'This window will remain open. Press any key when you are done reading it.'; read -n 1" _ "$@"
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LOG_PATH="${TMPDIR:-/tmp}/LBA2-Quest-Build-And-Install.log"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
title() { printf '\033]0;%s\007' "$1" 2>/dev/null || true; }

log() {
  echo "[quest-wizard] $*"
  echo "[quest-wizard] $*" >> "$LOG_PATH"
}

fatal() {
  log "ERROR: $*"
  echo ""
  echo "FAILED. Read the error above; nothing was silently ignored."
  echo ""
  echo "Diagnostic log:"
  echo "  $LOG_PATH"
  echo ""
  read -n 1 -p "Press any key when you are done reading it."
  exit 1
}

check_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    fatal "'$1' not found in PATH. Install it and retry."
  fi
}

# ---------------------------------------------------------------------------
# Banner
# ---------------------------------------------------------------------------
title "LBA2 Classic Community - Quest Builder" 2>/dev/null || true
echo ""
echo "============================================================"
echo "  LBA2 CLASSIC COMMUNITY - QUEST BUILD AND INSTALL WIZARD"
echo "============================================================"
echo ""
echo "This builds a personal APK for Meta Quest 3 and installs it."
echo "No APK or game data is included in this repository."
echo ""

# ---------------------------------------------------------------------------
# 1. Prerequisites
# ---------------------------------------------------------------------------
log "Checking prerequisites..."

check_cmd adb
check_cmd cmake
check_cmd ninja
check_cmd java
check_cmd javac
check_cmd keytool

if [ -z "${ANDROID_NDK:-}" ] || [ ! -d "$ANDROID_NDK" ]; then
  fatal "ANDROID_NDK is not set or does not point to a directory.\n" \
        "Example: export ANDROID_NDK=\$HOME/Android/Sdk/ndk/28.2.13676358"
fi

if [ -z "${SDL3_ANDROID_DIR:-}" ] || [ ! -d "$SDL3_ANDROID_DIR" ]; then
  fatal "SDL3_ANDROID_DIR is not set or does not point to a directory.\n" \
        "Example: export SDL3_ANDROID_DIR=\$PWD/out/android/sdl3-install"
fi

if [ -z "${ANDROID_HOME:-}" ] && [ -z "${ANDROID_SDK_ROOT:-}" ]; then
  fatal "ANDROID_HOME or ANDROID_SDK_ROOT is not set."
fi
SDK_ROOT="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [ ! -d "$SDK_ROOT" ]; then
  fatal "SDK root does not exist: $SDK_ROOT"
fi

BUILD_TOOLS=$(ls -d "$SDK_ROOT/build-tools/"* 2>/dev/null | sort -V | tail -1)
if [ -z "$BUILD_TOOLS" ]; then
  fatal "No Android build-tools found under $SDK_ROOT/build-tools/"
fi
AAPT="$BUILD_TOOLS/aapt"
AAPT2="$BUILD_TOOLS/aapt2"
ZIPALIGN="$BUILD_TOOLS/zipalign"
APKSIGNER="$BUILD_TOOLS/apksigner"
D8="$BUILD_TOOLS/d8"
for tool in "$AAPT" "$ZIPALIGN" "$APKSIGNER" "$D8"; do
  if [ ! -x "$tool" ]; then
    fatal "Required tool not found or not executable: $tool"
  fi
done

# Check platform exists
if [ ! -d "$SDK_ROOT/platforms/android-34" ]; then
  log "WARN: platforms/android-34 not found; trying to continue, but aapt may fail."
fi

# Check Quest is connected
if ! adb devices | grep -q $'\tdevice'; then
  fatal "No Quest device found via ADB.\n" \
        "Connect the headset, enable developer mode, and accept the USB debugging prompt."
fi
QUEST_SERIAL=$(adb devices | grep $'\tdevice' | head -n1 | cut -f1)
log "Quest device: $QUEST_SERIAL"

# ---------------------------------------------------------------------------
# 2. Original game files
# ---------------------------------------------------------------------------
echo ""
echo "The original LBA2 retail files are required."
echo "Expected files: lba2.hqr, ress.hqr, and other HQR resource files."
echo ""
read -r -p "Have you already pushed the game files to /sdcard/lba2cc/ on the Quest? [y/N] " HAVE_FILES
case "$HAVE_FILES" in
  [Yy]* )
    echo ""
    echo "Skipping file push."
    ;;
  * )
    echo ""
    read -r -p "Enter the path to your local LBA2 game-data folder: " GAME_DIR
    if [ -z "$GAME_DIR" ] || [ ! -d "$GAME_DIR" ]; then
      fatal "Game data directory not found: $GAME_DIR"
    fi
    HQR_COUNT=$(find "$GAME_DIR" -maxdepth 1 -name '*.hqr' | wc -l)
    if [ "$HQR_COUNT" -eq 0 ]; then
      fatal "No .HQR files found in $GAME_DIR"
    fi
    echo ""
    echo "Pushing game data to Quest..."
    adb shell mkdir -p /sdcard/lba2cc
    adb push "$GAME_DIR"/*.hqr /sdcard/lba2cc/ || fatal "adb push failed"
    log "Pushed $HQR_COUNT HQR file(s) to /sdcard/lba2cc/"
    echo "Game data pushed."
    ;;
esac

# ---------------------------------------------------------------------------
# 3. Build
# ---------------------------------------------------------------------------
echo ""
echo "Building LBA2 for Quest..."
cd "$REPO_ROOT" || fatal "Cannot cd to $REPO_ROOT"

BUILD_DIR="$REPO_ROOT/out/build/quest"
mkdir -p "$BUILD_DIR"

log "Configuring CMake (preset=quest)..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    --preset quest 2>&1 | tee -a "$LOG_PATH"
if [ ${PIPESTATUS[0]} -ne 0 ]; then
  fatal "CMake configure failed."
fi

log "Building native library..."
cmake --build "$BUILD_DIR" -- -j"$(nproc)" 2>&1 | tee -a "$LOG_PATH"
if [ ${PIPESTATUS[0]} -ne 0 ]; then
  fatal "Build failed."
fi

NATIVE_LIB="$BUILD_DIR/SOURCES/liblba2cc.so"
if [ ! -f "$NATIVE_LIB" ]; then
  fatal "Native library not found at $NATIVE_LIB"
fi
log "Native library ready: $NATIVE_LIB"

# ---------------------------------------------------------------------------
# 4. Bundle APK
# ---------------------------------------------------------------------------
echo ""
echo "Bundling APK..."

VERSION=$(cat "$REPO_ROOT/VERSION")
OUTPUT_DIR="$BUILD_DIR/apk"
mkdir -p "$OUTPUT_DIR"

bash "$REPO_ROOT/scripts/packaging/bundle-android.sh" \
    --lib "$NATIVE_LIB" \
    --version "$VERSION" \
    --arch arm64-v8a \
    --build-dir "$BUILD_DIR" \
    --sdk-root "$SDK_ROOT" \
    --sdl3-java-src "$REPO_ROOT/out/android/SDL3" \
    --sdl3-lib "$SDL3_ANDROID_DIR/lib/libSDL3.so" \
    --cxx-shared-lib "$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
    --output-dir "$OUTPUT_DIR" 2>&1 | tee -a "$LOG_PATH"
if [ ${PIPESTATUS[0]} -ne 0 ]; then
  fatal "APK bundling failed."
fi

APK_PATH="$OUTPUT_DIR/lba2cc-${VERSION}-android-arm64-v8a.apk"
if [ ! -f "$APK_PATH" ]; then
  fatal "APK not found at $APK_PATH"
fi
log "APK ready: $APK_PATH"

# ---------------------------------------------------------------------------
# 5. Install on Quest
# ---------------------------------------------------------------------------
echo ""
echo "Installing on Quest ($QUEST_SERIAL)..."
adb -s "$QUEST_SERIAL" install -r "$APK_PATH" 2>&1 | tee -a "$LOG_PATH"
if [ ${PIPESTATUS[0]} -ne 0 ]; then
  fatal "ADB install failed."
fi

# ---------------------------------------------------------------------------
# 6. Launch
# ---------------------------------------------------------------------------
echo ""
read -r -p "Install succeeded. Launch the game now? [Y/n] " LAUNCH_NOW
case "$LAUNCH_NOW" in
  [Nn]* )
    echo "Skipping launch."
    ;;
  * )
    echo "Launching..."
    adb -s "$QUEST_SERIAL" shell am start -n org.lbalab.lba2cc/org.lbalab.lba2cc.SDLActivity 2>&1 | tee -a "$LOG_PATH" || true
    ;;
esac

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
echo ""
echo "SUCCESS. The wizard completed all requested steps."
echo ""
echo "Diagnostic log:"
echo "  $LOG_PATH"
echo ""
echo "Next steps:"
echo "  - Put on the Quest headset."
echo "  - Game data should be available under /sdcard/lba2cc/."
echo "  - If the game does not detect the data, check the log with:"
echo "      adb logcat -s lba2cc"
echo ""
read -n 1 -p "Press any key when you are done reading it."
exit 0
