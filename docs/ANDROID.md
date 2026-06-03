# LBA2 Classic Community — Android

This directory contains scripts and configuration for building
LBA2 Classic Community on Android (arm64-v8a and armeabi-v7a).

## Quick start

Minimum Android version: **7.0 (API 24)**. The APK targets API 24 (required by SDL3) and is tested on API 24–34.

You need:

1.  **Android NDK** r28 or later (recommended: r28.2.13676358) — required for 16 KB memory-page support on arm64 (see [16 KB page support](#16-kb-page-support)).
2.  **SDL3** built for your target ABI(s).
3.  **Ninja** build system.
4.  Retail **game data** (HQR files) — you must provide these.

### 1. Build SDL3 for Android

```bash
# arm64-v8a: clones SDL (release-3.2.16) to out/android/SDL3, installs to
# out/android/sdl3-install, and applies the 16 KB-page linker flags.
bash scripts/dev/build-sdl3-android.sh
```

The source tree it leaves under `out/android/SDL3` is also the
`--sdl3-java-src` the bundler needs for the SDLActivity Java sources. For
`armeabi-v7a`, configure SDL3 by hand with `-DANDROID_ABI=armeabi-v7a`
(the script targets arm64-v8a).

### 2. Build LBA2

```bash
# From the repo root, point SDL3_ANDROID_DIR at the install from step 1
export ANDROID_NDK=$HOME/Android/Sdk/ndk/28.2.13676358
export SDL3_ANDROID_DIR=$PWD/out/android/sdl3-install
bash scripts/dev/build-android.sh                    # arm64-v8a (default)
bash scripts/dev/build-android.sh --abi armeabi-v7a  # 32-bit
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
    --sdl3-java-src out/android/SDL3 \
    --sdl3-lib $SDL3_ANDROID_DIR/lib/libSDL3.so \
    --cxx-shared-lib $ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so \
    --output-dir dist

# Install
adb install -r dist/lba2cc-*-android-arm64-v8a.apk
```

`--sdl3-lib` is required — without it `libSDL3.so` is omitted from the APK
and the app fails to load. `--cxx-shared-lib` points the bundler at the
NDK's `libc++_shared.so`; CI passes both explicitly.

## 16 KB page support

Android 15+ runs 64-bit apps on 16 KB memory pages on some devices. A
native library whose `LOAD` segments are 4 KB-aligned fails to load there
(`... program alignment (4096) cannot be smaller than system page size
(16384)`). The arm64-v8a build is 16 KB-safe:

- **NDK r28** ships `libc++_shared.so` 16 KB-aligned and defaults the
  linker `max-page-size` to 16384. r26 cannot be fixed by flags alone —
  its prebuilt STL is 4 KB-aligned.
- `-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384`, set in the
  `android_common` preset and the build scripts, keep `liblba2cc.so` and
  `libSDL3.so` aligned even on r27.
- The APK stores the `.so` uncompressed and 16 KB-zipaligned with
  `extractNativeLibs="false"`, so the loader maps them straight from the
  APK.
- `scripts/dev/check-16k-align.sh <apk>` verifies all of the above; CI
  runs it on every arm64 build.

Backward-compatible: `max-page-size` sets a *maximum*, so a 4 KB-page
kernel loads the libraries normally. **armeabi-v7a is unaffected** — 16 KB
pages are a 64-bit concern; 32-bit ARM (older devices, all current
Android TV) is always 4 KB.

## Game data on Android

Place your retail LBA2 `.HQR` files on the device via ADB:

```bash
# Android 11+ and Android TV (scoped storage): file managers can't access Android/data/
# Use ADB instead:
adb push lba2.hqr /sdcard/Android/data/org.lbalab.lba2cc/files/
adb push libraa.hqr /sdcard/Android/data/org.lbalab.lba2cc/files/

# (Recommended) place inside /sdcard/lba2cc/ — keeps loose files clean and the
# game automatically probes this path on startup:
adb push lba2.hqr /sdcard/lba2cc/
adb push libraa.hqr /sdcard/lba2cc/

# Also probes /sdcard/ directly as a fallback:
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
+--(ESC)---------(HIDE)---------(MEN)(CAM)-+
|                                           |
|                                           |
|                                           |
|                                           |
|      (U)               (MD) (ACT) (USE)   |
|   (L) (D) (R)          (THR) (INV) (W)    |
+-------------------------------------------+
```

- **ESC** — Escape (quit/back)
- **HIDE** — Show or hide the overlay (persistent pill when hidden)
- **MEN** — Menu (F10 / in-game menu)
- **CAM** — Camera toggle (Backspace)
- **D-pad** — U/D/L/R → Arrow keys (movement)
- **MD** — Mode (Ctrl / behaviour mode)
- **ACT** — Action (Space / jump/interact)
- **USE** — Use/confirm (Enter)
- **THR** — Throw (Alt)
- **INV** — Inventory (Shift)
- **W** — Action (always-on action key, "W" for walk/run)

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
