# LBA2 Classic Community — Android

This directory contains scripts and configuration for building
LBA2 Classic Community on Android (arm64-v8a).

## Quick start

You need:

1.  **Android NDK** r26 or later (recommended: r26.1.10909125).
2.  **SDL3** built for Android arm64-v8a.
3.  **Ninja** build system.
4.  Retail **game data** (HQR files) — you must provide these.

### 1. Build SDL3 for Android

```bash
git clone https://github.com/libsdl-org/SDL3.git
cd SDL3
cmake -B build-android -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=24 -DSDL_SHARED=ON -DSDL_STATIC=OFF
cmake --build build-android
cmake --install build-android --prefix $PWD/install-android
export SDL3_ANDROID_DIR=$PWD/install-android
```

### 2. Build LBA2

```bash
# From the repo root
export ANDROID_NDK=/path/to/android-ndk-r26
export SDL3_ANDROID_DIR=/path/to/sdl3-install-android
bash android/build_android.sh
```

### 3. Install on device

The build script produces `out/build/android_arm64/SOURCES/liblba2cc.so`.
To create an APK, either use the Gradle wrapper or package manually.

**Manual APK (fastest):**

```bash
# Create minimal APK structure
mkdir -p android/apk/lib/arm64-v8a
cp out/build/android_arm64/SOURCES/liblba2cc.so android/apk/lib/arm64-v8a/
cp android/AndroidManifest.xml android/apk/

# Package with aapt (from the Android SDK build-tools)
aapt package -f -M android/apk/AndroidManifest.xml \
    -S android/apk/res -I $ANDROID_SDK/platforms/android-34/android.jar \
    -F android/apk/unsigned.apk

# Align and sign
zipalign -f 4 android/apk/unsigned.apk android/apk/aligned.apk
apksigner sign --ks ~/.android/debug.keystore \
    --ks-pass pass:android android/apk/aligned.apk

# Install
adb install -r android/apk/aligned.apk
```

> **Note:** Retail game data (HQR files) must be placed in a location the
> game can find, such as `/sdcard/Android/data/org.lbalab.lba2cc/files/`
> or an external SD card directory. The game will scan common locations
> on first launch.

## Game data on Android

Place your retail LBA2 `.HQR` files in:

```
/storage/emulated/0/Android/data/org.lbalab.lba2cc/files/
```

Or use the `--game-dir` flag via a file manager intent (if supported)
or by editing the lba2.cfg after first launch.

## Key mapping (touch overlay)

The on-screen virtual gamepad uses the following layout:

```
+--(ESC)---------------------------(MEN)--+
|                                   (CAM) |
|                                          |
|       (UP)                               |
|  (L) (D) (R)                            |
|                                          |
|              (MD) (ACT) (USE)            |
|              (THR) (INV) (W)             |
+------------------------------------------+
```

- **ESC** — Escape (quit / back)
- **MEN** — Menu (F10 / in-game menu)
- **CAM** — Camera toggle (Backspace)
- **D-pad** — Arrow keys (movement)
- **MD** — Mode (Ctrl / behaviour mode)
- **ACT** — Action (Space)
- **USE** — Use/confirm (Enter)
- **THR** — Throw (Alt)
- **INV** — Inventory (Shift)
- **W** — Action (always-on action key)

The layout is defined in `SOURCES/TOUCH_INPUT.CPP` and can be customised
by editing the `kButtons[]` table (normalised 0..1 coordinates).

## Limitations

- **Software renderer**: The game uses a software 8-bit→ARGB pipeline.
  Performance on modern ARM64 devices is acceptable at 640x480.
- **CD audio**: CD-ROM music tracks are not available. Use the digital
  sample backend (`SOUND_BACKEND=sdl`).
- **Text input**: Save-game naming and console commands require a hardware
  keyboard or IME integration (future work).
- **Save/resume**: Android lifecycle pause/resume is handled by SDL3
  (window focus events). Surface re-creation is managed by the existing
  SDL3 infrastructure.
- **Game data**: You must provide your own retail LBA2 data files.
