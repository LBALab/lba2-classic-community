# LBA2 Classic Community — Android

How to build, package, and run LBA2 Classic Community on Android
(arm64-v8a and armeabi-v7a). The build and packaging scripts live under
`scripts/dev/` and `scripts/packaging/`.

## Quick start

Minimum Android version: **7.0 (API 24)**. The APK targets API 24 (required by SDL3) and is tested on API 24–36.

You need:

1.  **Android NDK** r28 or later (recommended: r28.2.13676358) — required for 16 KB memory-page support on arm64 (see [16 KB page support](#16-kb-page-support)).
2.  **SDL3** built for your target ABI(s).
3.  **Ninja** build system.
4.  Retail **game data** (HQR files) — you must provide these.

### 1. Build SDL3 for Android

```bash
# arm64-v8a: clones SDL at the tag in .github/sdl3-version.txt to
# out/android/SDL3, installs to out/android/sdl3-install, and applies the
# 16 KB-page linker flags.
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

Place your retail LBA2 `.HQR` files on the device with ADB. **Recommended:
`/sdcard/lba2cc/`** — the game probes it ahead of the other external roots and
it survives an uninstall/reinstall:

```bash
adb push LBA2.HQR /sdcard/lba2cc/
adb push RESS.HQR /sdcard/lba2cc/
# ...and the rest of your retail HQR set
```

`/sdcard/lba2cc/` is general external storage, so on **Android 11+ (API 30+)**
the app needs **All Files Access** (`MANAGE_EXTERNAL_STORAGE`). The game checks
for it on launch and opens the system Settings page if it is missing — grant it
and relaunch. You can also pre-grant it:

```bash
adb shell appops set org.lbalab.lba2cc MANAGE_EXTERNAL_STORAGE allow
```

The **app-specific** dir `/sdcard/Android/data/org.lbalab.lba2cc/files/` needs
no permission, but on stricter API 30+ / TV images files `adb push`ed there can
be invisible to the app's own view, so prefer `/sdcard/lba2cc/`.

`ResolveGameDataDir` (`SOURCES/RES_DISCOVERY.CPP`) scans, in order:

1. `--game-dir` / `--data-dir` CLI flag
2. `LBA2_GAME_DIR` environment variable
3. Persisted last-used directory (from a previous picker session)
4. SDL base path and its `data/` / `game/` subdirs
5. Current running directory
6. App-specific external-files dir (Android; no permission) — `/sdcard/Android/data/<pkg>/files/`
7. **`/sdcard/lba2cc/`** and `/storage/emulated/0/lba2cc/`, then `/sdcard/` and `/storage/emulated/0/`
8. Parent-directory walk
9. Folder picker fallback (if compiled with debug tools)

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
- **HIDE** — Dismiss the controls now; the next touch brings them back
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

## When the overlay shows

It follows the last input device the player actually used, and fades when they
stop using it:

| state | overlay |
|-------|---------|
| a finger is down | full opacity, for as long as it is held |
| ~1.5s after the lift | dimmed, still legible enough to aim by |
| ~6s after the lift | gone |
| a gamepad or key was the last input | gone, until a finger says otherwise |
| nothing has been touched yet | dimmed, or gone if a pad is connected or the device is a TV |
| HIDE was pressed | gone now, without waiting out the fade |

The first touch after using another device only wakes the overlay; it presses
nothing. Every touch after that both wakes and presses, so a player who knows
the layout never loses the press that mattered. It also stops a phone in a
controller clip latching a direction when a palm brushes the glass.

Every one of those states is left by touching the screen. That is what removed
the old "persistent pill when hidden": a control whose only job was to undo
something a touch already undoes. HIDE is now a dismiss rather than a mode, so
hitting it by accident (it sits where you tap to skip dialogue) costs one touch
instead of stranding you in a state with an unexplained chip on screen.

The buttons are placed in coordinates normalised over the whole SDL surface, so
they sit on top of the game's letterbox bars rather than inside the rendered
picture. That is only coherent because the surface fills the display, which the
fullscreen coercion below guarantees on Android. Touch hit-testing does not
depend on it either way: `MapToGameCoords` reads the live window size, so it
follows the surface whatever size it is.

## Window and display

`Window_SupportsWindowedMode()` (`LIB386/SYSTEM/WINDOW.CPP`) returns false on
Android, so the Display submenu drops its Fullscreen/Windowed row and the window
is always fullscreen. `CoerceFullscreenForPlatform` ignores a windowed request
there: with the row gone, a player whose config asked for windowed would sit
behind the system bars with no control to undo it. The config value is kept
rather than rewritten, so a profile carried to desktop still honours it.

The manifest names `@android:style/Theme.DeviceDefault.NoActionBar.Fullscreen`.
With no theme the framework picks one that has an action bar, and SDL puts the
SDL window title into the activity title, so that bar draws the product name and
version over the game. DeviceDefault rather than the legacy
`Theme.NoTitleBar.Fullscreen` from SDL's android-project template, because the
theme also styles the dialogs SDL builds against the activity.

## Limitations

- **Software renderer**: The game uses a software 8-bit→ARGB pipeline.
  Performance on modern ARM64 devices is acceptable at 640x480.
  On older 32-bit ARM (armeabi-v7a) devices, expect lower framerates.
- **CD audio**: CD-ROM music tracks are not available. Use the digital
  sample backend (`SOUND_BACKEND=sdl`).
- **Text input**: Console commands still require a hardware keyboard. Save-game
  naming no longer does — without a physical keyboard (touch, gamepad, or
  Android TV) the save menu auto-generates a name from the date and the current
  island instead of prompting.
- **Save/resume**: Android lifecycle pause/resume is handled by SDL3
  (window focus events). Surface re-creation is managed by the existing
  SDL3 infrastructure.
- **Game data**: You must provide your own retail LBA2 data files.
- **Android TV**: Touch overlay is off when a TV device is detected
  (`android.software.leanback` feature). Use a gamepad or remote control
  instead. This is only the starting value: the last-input rule reaches the
  same answer on its own, because a TV is a screen nobody ever touches.
