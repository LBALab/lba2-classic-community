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

`--sdl3-lib` is required and takes `libSDL3.so` or the folder holding it.
`--sdl3-java-src` takes the SDL3 source root, the folder holding
`android-project/`, not the Java folder inside it. The bundler stops with a
message when either misses, rather than building an APK that installs and then
fails to start. `--cxx-shared-lib` points the bundler at the NDK's
`libc++_shared.so`; CI passes all three explicitly.

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
for it on launch and opens the system Settings page if it is missing. Grant it
and come back: the launch is waiting for the answer and carries on with it, so
the saves land in the right folder the first time. Declining is not fatal either,
though the game data cannot be read without it. You can also pre-grant it:

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

The two `lba2cc/` folders accept either extracted retail files or a disc image
such as a BIN/CUE rip. The bare external-storage roots only check for extracted
files, avoiding an expensive scan across an entire shared-storage directory.

## Where the game saves

Saves, `lba2.cfg`, `adeline.log`, `adeline.prev.log` and `recordings/` all live
in the user directory, and on Android that is:

```
/sdcard/lba2cc/user/LBA2/
```

After a native crash, the next launch also saves the system's tombstone of it
there as `crash-<time>.pb`, replacing the one saved before, and the boot banner
says so on a `Note:` line. Android 12 or later keeps the tombstone; Android 11
records only that the crash happened. The file is a protobuf, decoded off device
with AOSP's `pbtombstone` or `protoc --decode` against `tombstone.proto`. The
engine's own `CRASH` block for the same crash is in the log of that run.

Beside the game data rather than inside it, so backing up `/sdcard/lba2cc/`
takes both while keeping the player's files apart from a retail set they may
re-push at any time.

The last element names the build, the same way the desktop paths do
(`Twinsen/LBA2/` per user, `User/LBA2/` portable). A demo build writes to
`user/LBA2-Demo/` and so keeps its own saves, and a second game on this engine
gets a folder of its own without anything having to move.

That folder is ordinary external storage, so a file manager, a PC over USB and
any backup tool can read it, and it survives uninstalling the app. Until
`0.13.0` the user directory was whatever `SDL_GetPrefPath` returns, and on
Android that call ignores the org and app it is given and hands back
`Context.getFilesDir()`, which is `/data/data/<pkg>/files/`. Nothing reaches
that folder on a stock device: `adb shell` gets *Permission denied*, `run-as`
refuses a non-debuggable package, and `adb backup` returns an empty archive
even with `android:allowBackup="true"` set. The system also erases it on
uninstall.

**If All Files Access is not granted**, `/sdcard/lba2cc/user/LBA2/` cannot be
written to, and the game falls back in this order:

| | |
|---|---|
| `/sdcard/lba2cc/user/LBA2/` | needs All Files Access; survives uninstall |
| `/storage/emulated/0/lba2cc/user/LBA2/` | the same folder under its other name |
| `/sdcard/Android/data/<pkg>/files/` | no permission needed; erased on uninstall |
| `SDL_GetPrefPath` | app-private; unreachable; erased on uninstall |

Which one is in force is decided by writing a probe file, not by asking whether
the permission is held: on Android 11+ a path can be readable, listable and
unwritable at the same time, and the permission can be revoked between two
launches. A fallback is named in the boot banner, described below.

**Nothing is stranded when the answer changes.** If the chosen folder holds no
user data and one further down the list does, the tree is copied up on the next
launch, profiles included. That covers both the move out of app-private
storage and the case where a player grants All Files Access after already
playing. Nothing is deleted and nothing is overwritten: a folder that has been
played in is left exactly as it is.

"Has been played in" means it holds `lba2.cfg`, `save/` or `profiles/`, not
that it is non-empty. The app-specific external folder is one of the roots
here, and this document tells players they may drop their retail HQR set
there.

**If two folders both hold saves, both are left alone and the other one is
named.** Reinstalling drops All Files Access, so the next launch falls back and
starts a fresh folder; anything played before the permission is granted again
lands there and then stops being visible once the shared folder is reachable.
Merging is not on the table, because both sets are somebody's real progress and
nothing in the engine can tell which one they meant. The banner says where the
other one is so it can be copied over by hand.

Whichever root loses, any folder that was carried up, and any second set of
saves are all named in the boot banner at the top of `adeline.log` on `Note:`
lines, so a pasted log says where the saves went and why.

[check-android-userdir.sh](../scripts/dev/check-android-userdir.sh) asserts all
of this against a connected device:

```bash
ADB=/path/to/adb scripts/dev/check-android-userdir.sh dist/lba2cc-*-arm64-v8a.apk
```

It covers the parts a host test cannot reach: that All Files Access really does
let the engine write to `/sdcard`, and that the atomic config write survives the
FUSE-emulated filesystem `/sdcard` actually is. The "granted the permission
later" case needs `LBA2_ALLOW_REBOOT=1`, because `appops set` grants the
operation without rebuilding a running app's view of storage: `appops get` then
reports `allow` with a `rejectTime` beside it, and the app keeps using the
fallback until the device reboots or the permission is granted through Settings.

Put extracted retail files without `LBA2.CFG` in `/sdcard/lba2cc/` for the full
run. With a disc image there, the config write is skipped, because the game
reads the disc's own config and writes none on a first boot. The checks without
All Files Access are skipped too, because the game cannot open the image and
stops at "Game data not found". Checks that need root report SKIP on a retail
phone.

### Updating without losing your progress

Since `0.13.0` every published APK is signed with the same key, so a new
version installs over the old one and your saves stay put.

Builds before that were each signed with a throwaway key generated by the CI
job, which made every release a different app as far as Android is concerned.
Installing one over another fails with *"App not installed as package conflicts
with an existing package"*, and uninstalling first was the only way through.
That erased the saves, because they were in app-private storage.

Coming from a build older than `0.13.0` you have to uninstall once. From
`0.13.0` onward, updates are updates. See
[RELEASING.md](RELEASING.md#android-signing-key) for the key itself.

**The gate is root, not a PC.** Older builds kept saves in app-private
storage, which the uninstall erases, so a rescue has to happen before you
uninstall. A rooted device can do it on its own, and a PC does not help without
root: `adb shell` runs as the shell user, and the shell user cannot read another
app's private storage any more than you can.

**On a rooted device, with no PC.** A file manager with root access, or a backup
app such as Swift Backup, copies the folder directly:

```
/data/data/org.lbalab.lba2cc/files/   ->   /sdcard/lba2cc/user/LBA2/
```

Copy rather than move, then uninstall, then install the new build. The uninstall
is what clears the app-private copy, and leaving one behind means every launch
reports two sets of saves.

**On a rooted device, from a PC.** The same operation over `adb`:

```bash
adb root
adb shell "mkdir -p /sdcard/lba2cc/user/LBA2"
adb shell "cp -r /data/data/org.lbalab.lba2cc/files/. /sdcard/lba2cc/user/LBA2/"
adb uninstall org.lbalab.lba2cc
adb install lba2cc-0.13.0-android-arm64-v8a.apk
```

Either way the new build finds a folder that already holds saves and settings
and uses it as it is.

**Without root there is no way to get them out.** `adb shell` and `adb pull` are
refused by the sandbox, `run-as` only works on a debuggable build, and `adb
backup` returns an empty archive even though the manifest allows backups: the
attribute that decides it is `android:debuggable`, which a release build does not
carry. A cloud backup does not help either, because restoring one requires the
signing certificate the old build was published with. Progress from a build
older than `0.13.0` is lost.

**Rooting a device that is not already rooted does not rescue it.** Rooting
generally requires unlocking the bootloader, and unlocking wipes user data,
destroying the saves the exercise was meant to recover. Reading the bootloader
state with `fastboot getvar unlocked` is safe; `fastboot flashing unlock` is not.
Only a device that is already unlocked, or already rooted, has a route.

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

## Running tests on a device

[PLATFORM.md](PLATFORM.md) puts the floating-point split between *architectures*,
not operating systems: on ARM `long double` is a plain 53-bit double. Every
"works on x86, broken on ARM" question was previously answered by reading code.
This lane answers it by measuring.

**The target must be ARM.** An x86_64 Android image carries 80-bit `long double`
exactly like an x86 desktop, so it cannot show the difference. This is not
theoretical: an x86_64 emulator image can advertise
`ro.product.cpu.abilist=x86_64,arm64-v8a`, which means an arm64 binary runs there
under translation and reports a green result that means nothing.
`scripts/dev/android-lib.sh` refuses a non-ARM target for that reason, rather
than warning and continuing.

**Where to run it.** An Apple Silicon Mac runs an `arm64-v8a` emulator image
natively, so an emulator there is a valid target for `arm64-v8a`. An x86_64
host cannot run one at all; the emulator refuses the AVD:

```
FATAL | Avd's CPU Architecture 'arm64' is not supported by the QEMU2 emulator
        on x86_64 host. System image must match the host architecture.
```

`armeabi-v7a` needs a physical device on either host: Apple Silicon has no
32-bit ARM mode, and an x86 emulator would only translate it. With more than one
target attached, set `ANDROID_SERIAL`.

### The host test suite on ARM

The 69 test subdirectories under `tests/` build for `arm64-v8a` with no code
change, and CMake can run them on a device through
`CMAKE_CROSSCOMPILING_EMULATOR`, so plain `ctest` drives the device:

```bash
cmake -S . -B out/build/android_arm64_tests -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=24 -DANDROID_STL=c++_shared \
    -DSDL3_DIR="$SDL3_ANDROID_DIR/lib/cmake/SDL3" \
    -DLBA2_BUILD_TESTS=ON -DLBA2_BUILD_ASM_EQUIV_TESTS=OFF \
    -DCMAKE_CROSSCOMPILING_EMULATOR="$PWD/scripts/dev/android-exec.sh"
cmake --build out/build/android_arm64_tests --target host_tests
ctest --test-dir out/build/android_arm64_tests --output-on-failure
```

`LBA2_BUILD_ASM_EQUIV_TESTS=OFF` is required. It defaults ON and those tests need
UASM and x86. Point `SDL3_DIR` at an install of the version in
`.github/sdl3-version.txt`.

`android-exec.sh` copies each test, `libc++_shared.so` and `libSDL3.so` to
`/data/local/tmp/lba2cc` (skipping files already there), runs the test in an
empty `run/<test>` folder and returns its exit code. Repo files a test opens are
listed in its `CMakeLists.txt` with `lba2_test_device_data()` and copied to
`repo/`; scratch folders come from `lba2_test_tmp_dir()`. Both give absolute
paths, because SDL on Android resolves a relative path through the Java
Activity.

A test run this way is a plain executable with no Activity and no JVM, which
limits it in three ways:

- `SDL_Init` refuses to start until main is marked ready. Tests that start SDL
  include `tests/sdl_main_ready.h` and call `TEST_SDL_MAIN_READY()`.
- SDL asks Java for the app name the first time it creates a thread, and
  crashes when there is none. `android-exec.sh` sets `SDL_APP_NAME`, which SDL
  reads first.
- Creating a renderer, starting the event system and `SDL_GetPrefPath` all need
  the Activity: the first two wait for it forever and the third crashes. Tests
  that do these are marked with `lba2_test_needs_android_activity()` and show as
  not run, or skip themselves (`test_getascii`).

**What this adds over the macOS CI job, which is less than it first appears.**
`macos-latest` is Apple Silicon and `.github/workflows/macos.yml` already runs
`ctest -L host_quick` plus a verbose `test_numeric_contract` there on every push,
specifically to report the ARM rounding figures. macOS arm64 and Android arm64
share the same 53-bit `long double`, so the headline arithmetic question is
already answered in CI and does not need a device.

What a device still adds:

- **Bionic, not Apple libc.** Same mantissa, different `libm`. Any rounding that
  comes out of `sin`, `cos` or `pow` rather than out of the ABI can differ
  between the two and neither host can speak for the other.
- **`armeabi-v7a`.** 32-bit ARM, different pointer size and alignment rules, and
  what all current Android TV hardware runs. No Apple target says anything at
  all about it.
- **The engine, not just the host tests.** macOS CI runs `tests/`, which is pure
  computation. `tests/automation/` drives the actual game, and the executable
  below is the way to run it anywhere ARM.

So read the numeric measurement from the macOS job. Use a device for the three
things above.

### The engine as a command-line executable

`-DLBA2_ANDROID_EXECUTABLE=ON` builds the Android target as a PIE executable
instead of the `.so` the Java Activity loads. Run under `adb shell` it has real
argv, stdout and stderr kept separate, and a real exit code, which is what the
automation suite in `tests/automation/` reads and what the Activity cannot
provide. It is a measurement tool, not a shippable artifact.

Give it its own build directory: `scripts/dev/build-android.sh` derives the build
directory from the ABI alone, so a variant build would otherwise overwrite the
normal one.

Build it on the null platform. SDL3 cannot work here: even with `--headless` its
window code creates a renderer, and on Android that waits for the Activity
(see above). The null platform needs no SDK, draws into its own buffer and stubs
the Android calls, and the CMake check that keeps it off Android makes an
exception for this build:

```bash
cmake -S . -B out/build/android_arm64_cli -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=24 -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=Release -DLBA2_ANDROID_EXECUTABLE=ON \
    -DPLATFORM_BACKEND=null -DSOUND_BACKEND=null -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON
cmake --build out/build/android_arm64_cli --target lba2cc
```

With no Activity there is no app storage either: the null platform's
`Android_GetExternalFilesDir()` answers nothing and its preferences folder
comes from `HOME`. Automation always sets a user directory, and
`android-lba2cc.sh` points `HOME` inside its device folder.

### The automation suite on a device

`scripts/dev/android-lba2cc.sh` stands in for the engine binary, so the suite
runs unchanged:

```bash
export ANDROID_SERIAL=emulator-5554   # or the phone's serial
export LBA2_ANDROID_ENGINE=$PWD/out/build/android_arm64_cli/SOURCES/lba2cc
export LBA2_BIN=$PWD/scripts/dev/android-lba2cc.sh
export LBA2_GAME_DIR=/path/to/data LBA2_TEST_SAVE=/path/to/save.LBA
tests/automation/run.sh --no-record -j 4
```

For each engine run it copies the engine, `libc++_shared.so`, the data folder and
the save folder to `/data/local/tmp/lba2cc` (each once, until it changes),
copies the repo and temp paths the arguments name, rewrites those paths for the
device, runs the engine there and copies what it wrote back, mapping device
paths in its output back to host paths. Compare the result with the suite on a
null-platform build on the host, not with an SDL build: fixtures that need keys
typed into a name editor, an audio device or another window size fail on the
null platform anywhere.

Limits:

- A path the engine writes into a file keeps its device spelling, so
  `test_profile_bind`, which reads the game folder a profile stored, fails.
- Each engine run costs about a second of adb round trips. Fixtures that start
  hundreds of runs (the ring fixtures) take several times longer than on the
  host, and under `-j` they can reach their timeout.
- An `armeabi-v7a` engine needs a physical device (see above).

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
- **Android TV**: the touch overlay starts off when a TV device is detected
  (`android.software.leanback` feature). Use a gamepad or remote control
  instead. Leanback is only the starting value, not a lock: a touch still
  brings the overlay up, so a leanback device that does have a touchscreen can
  use it. A first touch is better evidence than a manifest feature flag, and
  the alternative was a control the player has no way to reach.
