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

Place your retail LBA2 `.HQR` files on the device with ADB. **Recommended:
`/sdcard/lba2cc/`** — the game probes it ahead of the other external roots and
it survives an uninstall/reinstall:

```bash
adb push lba2.hqr /sdcard/lba2cc/
adb push ress.hqr /sdcard/lba2cc/
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
- **Text input**: Console commands still require a hardware keyboard. Save-game
  naming no longer does — without a physical keyboard (touch, gamepad, or
  Android TV) the save menu auto-generates a name from the date and the current
  island instead of prompting.
- **Save/resume**: Android lifecycle pause/resume is handled by SDL3
  (window focus events). Surface re-creation is managed by the existing
  SDL3 infrastructure.
- **Game data**: You must provide your own retail LBA2 data files.
- **Android TV**: Touch overlay is automatically disabled when a TV device
  is detected (`android.software.leanback` feature). Use a gamepad or
  remote control instead.

## OpenXR (Meta Quest 3)

Optional, off by default. The Quest build preset (`cmake --preset quest`)
turns on `-DLBA2_OPENXR=ON` and makes the engine speak to the OpenXR
runtime on the headset. With the Stereo 3D display option enabled, both
eyes render into the left and right halves of one double-wide MSAA scene
target, and `OpenXR_PresentMono` blits that target into the double-wide
swapchain in one pass. When brightness, contrast, or saturation is
non-neutral, that final pass applies the display transform to the whole
pair instead of costing a second composite. Per-eye capture FBOs remain as
a fallback for FXAA/SMAA or a double-wide FBO failure. Set
`LBA2_OXR_GLDEBUG=1` to log GL errors from the present stages; release runs
skip the per-present error queries.

For a device experiment, `LBA2_OXR_DIRECT=1` (or a build configured with
`-DLBA2_OXR_DIRECT=ON`) renders both eyes and the HUD directly into the
acquired swapchain texture with `EXT_multisampled_render_to_texture`, removing
the resolve/blit pair. It is off by default, requires the resolve-in-tile probe
to pass and a neutral display transform, and falls back to the normal
double-wide blit on any mismatch or incomplete FBO. The runtime resolves the
texture when the compositor samples it; if the headset shows a black or
single-eye image, leave it off. `OpenXRPerf` reports a `direct=` count when the
path is active.

### Foveation on Quest

`VrFoveation` in the Display menu sets the fixed-foveation level forwarded to
the OpenXR compositor (`XR_FB_foveation` plus
`XR_FB_foveation_configuration`). It is a compositor-side setting: the runtime
samples the submitted swapchain at reduced density away from the focal point.
The engine still renders the double-wide scene target at full resolution, so
this setting does not reduce the engine's render cost; it can reduce the
compositor's sampling cost.

`GL_QCOM_framebuffer_foveated` and `GL_QCOM_texture_foveated` are not a
substitute on the Quest 3 Adreno 740 driver. The driver advertises the
extensions and reports `features=0x7` with two focal points, and accepts the
feature bits and focal-point parameters without a GL error, but:

- framebuffer foveation reports `provided=0x0` (the entry points are stubs);
- texture foveation produces no peripheral blur and no frame-time change on
  a multisample or single-sample scene target, with the scaled-bin or
  subsampled-layout method, including a present shader compiled with
  `layout(subsampled) sampler2D`.

A `glBlitFramebuffer` from a subsampled-layout target also reconstructs the
storage to full resolution, per the extension specification, so a blit-based
present cannot show that layout's foveation. Treat the GLES foveation entry
points as non-functional on this driver; do not re-add an explicit-MSAA
foveation target without re-validating on a driver that actually reduces
density. The in-tile MSAA path is the baseline.

The only remaining route to foveate the engine's own rendering is to render
directly into the runtime swapchain (`LBA2_OXR_DIRECT`), where the compositor's
foveation applies to the app's writes. That path is currently off by default
and is not validated; see the direct-stereo paragraph above.

In first person, each eye
renders from its tracked view
(`OpenXR_GetEyeView`): the orientation comes from the runtime-located
pose and the IPD split runs along the view-space right axis, so the
compositor's time-warp reprojects head rotation cleanly and the stereo
depth is real. The classic and auto cameras keep the game's rig position
but anchor the view to the world, not to Twinsen: the rig framing
captured when the look session engages plus the head's world-relative
rotation 1:1 (recentered on the camera-center key), so the auto
camera's swings around the hero no longer pan the view, and the tracked
head translation rides on the rig position as a clamped lean (proper
6dof, `SOURCES/STEREOLOOK.CPP` holds the conventions). Frames without a
fresh per-eye pair
(menus, videos, interiors, the first frame) fall back to a single
composite copied to both halves. Controller state comes through the
OpenXR action pipeline, mapped onto the same virtual-scancode space the
SDL gamepad path uses so `JOYSTICK.CPP` picks it up unchanged.

### Where the loader comes from

When `LBA2_OPENXR=ON`, the build consumes the official Khronos Android
OpenXR loader via a pre-staged install at
`out/android/openxr/install/`. The AAR
[`org.khronos.openxr:openxr_loader_for_android`](https://central.sonatype.com/artifact/org.khronos.openxr/openxr_loader_for_android)
(version 1.1.43) was extracted there once: headers under `include/openxr/`
and the per-ABI `libopenxr_loader.so` under `lib/`. CMake consumes the
install through `LBA2_OPENXR_BASE_DIR` and exposes it as the
`openxr_loader` target.

Pointing `LBA2_OPENXR_BASE_DIR` at a different pre-staged install (or
setting it empty to have CMake fetch
[KhronosGroup/OpenXR-SDK](https://github.com/KhronosGroup/OpenXR-SDK)
release-1.1.43 and build the loader from source) both work; the AAR path
is what the Quest build uses by default because it ships a loader
tested against the current Quest firmware.

If no OpenXR runtime is reachable, `OpenXR_Init()` logs the failing
loader call and the rest of the engine runs unmodified.

### Why the Quest 3 init needs `xrInitializeLoaderKHR`

The Quest 3 forward loader (`libopenxr_forwardloader.so` inside the
Meta `VrDriver` APK) only hands the runtime to callers that have
called `xrInitializeLoaderKHR` with the calling app's `JavaVM` and a
global reference to the activity class. The Khronos loader exposes
this as a "loader function" through `xrGetInstanceProcAddr` with
`XR_NULL_HANDLE`. `LIB386/SYSTEM/OPENXR.CPP` does exactly that dance,
then chains `XrInstanceCreateInfoAndroidKHR` into `xrCreateInstance`
with `applicationVM` and `applicationActivity` set. The same
`XrLoaderInitInfoAndroidKHR`/`XrInstanceCreateInfoAndroidKHR` pair
is what the Meta OpenXR SDK's `XrPassthrough` sample uses.

### Building and running on a Quest 3

```bash
cmake --preset quest
cmake --build build/quest
# Push the resulting liblba2cc.so + assets to the headset; see
# scripts/packaging/bundle-android.sh for the full bundler flow.
```

### License

Apache 2.0 throughout (Khronos OpenXR SDK + AAR). No Meta SDK is
fetched or linked; the engine does not need Meta's partner-only
`com.oculus.systemdriver.DriverLoader` Java class because the Khronos
loader performs the runtime-broker handshake directly.
