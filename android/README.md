# LBA2 Classic Community — Android

This directory contains scripts and configuration for building
LBA2 Classic Community on Android (arm64-v8a and armeabi-v7a).

## Quick start

You need:

1.  **Android NDK** r26 or later (recommended: r26.1.10909125).
2.  **SDL3** built for your target ABI(s).
3.  **Ninja** build system.
4.  Retail **game data** (HQR files) — you must provide these.

### 1. Build SDL3 for Android

```bash
# For arm64-v8a (modern phones/tablets, 64-bit Android TV)
git clone https://github.com/libsdl-org/SDL3.git
cd SDL3
cmake -B build-arm64 -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=24 -DSDL_SHARED=ON -DSDL_STATIC=OFF
cmake --build build-arm64
cmake --install build-arm64 --prefix $PWD/install-arm64

# For armeabi-v7a (32-bit Android TV, older devices)
cmake -B build-armv7a -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=24 -DSDL_SHARED=ON -DSDL_STATIC=OFF
cmake --build build-armv7a
cmake --install build-armv7a --prefix $PWD/install-armv7a
```

### 2. Build LBA2

```bash
# From the repo root, point SDL3_ANDROID_DIR to the install for your target ABI
export ANDROID_NDK=/path/to/android-ndk-r26
export SDL3_ANDROID_DIR=/path/to/sdl3-install-arm64
bash android/build_android.sh
```

### 3. Install on device

The build script produces `out/build/android_arm64/SOURCES/liblba2cc.so`
(or `armeabi-v7a` for 32-bit builds). To package into an APK, use the
bundler script:

```bash
bash scripts/packaging/bundle-android.sh \
    --lib out/build/android_arm64/SOURCES/liblba2cc.so \
    --version "$(cat out/build/android_arm64/VERSION.txt)" \
    --arch arm64-v8a \
    --build-dir out/build/android_arm64 \
    --sdk-root $ANDROID_HOME \
    --output-dir dist

# Install
adb install -r dist/lba2cc-*-android-arm64-v8a.apk
```

## Game data on Android

Place your retail LBA2 `.HQR` files on the device via ADB:

```bash
# Android 11+ (scoped storage): file managers can't access Android/data/
# Use ADB instead:
adb push lba2.hqr /sdcard/Android/data/org.lbalab.lba2cc/files/
adb push libraa.hqr /sdcard/Android/data/org.lbalab.lba2cc/files/

# Or place at the SD card root — the game also probes /sdcard/ and
# /storage/emulated/0/ on startup:
adb push lba2.hqr /sdcard/
adb push libraa.hqr /sdcard/
```

The game's `ResolveGameDataDir` scans these paths automatically:

1. `--game-dir` CLI flag
2. `LBA2_GAME_DIR` environment variable
3. Persisted last-used directory
4. SDL base path and subdirs (`./data/`, `./game/`)
5. Current directory and parent walk (up to 8 levels)
6. **`/sdcard/`** and **`/storage/emulated/0/`** (Android external root)
7. Parent sibling directories (e.g. `../LBA2/`, `../game/`)
8. Folder picker fallback (if compiled with debug tools)

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
  On older 32-bit ARM (armeabi-v7a) devices, expect lower framerates.
- **CD audio**: CD-ROM music tracks are not available. Use the digital
  sample backend (`SOUND_BACKEND=sdl`).
- **Text input**: Save-game naming and console commands require a hardware
  keyboard or IME integration (future work).
- **Save/resume**: Android lifecycle pause/resume is handled by SDL3
  (window focus events). Surface re-creation is managed by the existing
  SDL3 infrastructure.
- **Game data**: You must provide your own retail LBA2 data files.
- **Android TV**: Touch overlay is automatically disabled when a TV device
  is detected (`android.software.leanback` feature). Use a gamepad or
  remote control instead.
