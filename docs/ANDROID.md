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
- **Graphics quality (SSAA)**: The Display → Graphics → Quality row is
  desktop-only. Android fixes render quality at Native: 2x/4x SSAA at headset
  resolution exceeds the mobile GPU's fill-rate and memory budget. Use
  Antialiasing (MSAA×2 or FXAA) or, on a headset, Display → VR → Supersampling
  (Off / 1.25x / 1.5x). Supersampling renders the 3D scene above the eye buffer
  and downsamples it on present; it costs fill rate and memory, and reverts to
  the previous scale if the target cannot be allocated.
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
a fallback for FXAA or a double-wide FBO failure. Set
`LBA2_OXR_GLDEBUG=1` to log GL errors from the present stages; release runs
skip the per-present error queries.

Exterior scenes use a single-pass path: the 3D scene is traversed once from
the eye-0 camera, and each draw is submitted twice with its own scissor and
an eye-1 camera delta supplied to the shader. The screen-space 2D overlay is
drawn once for the pair, at eye 1's present, after the scene resolve, and
painted into both halves from one band list: the two eyes cannot drift apart,
and the glyphs avoid the MSAA softening the scene target applies to everything
else. Eye 1 also skips the terrain and decor walk, the object
sort insertions and 3D GL calls; the object walk still performs its state
updates. CPU culling widens edge candidates by the eye-1 disparity, so
geometry visible only in the second eye is not rejected by the eye-0
projection. Tracked per-eye orientation is not a pure translation, so true
6dof first-person keeps the two-pass eye renderer. The two-pass path remains
the fallback and the reference for visual comparisons.

The interior diorama panel (`VrDioramaPanel`, Display → VR → Diorama panel,
console `vrdiorama`) presents the room as a world-anchored pane instead of a
screen-space image: on the first interior frame it is anchored 2 m in front of
the player, 0.35 m below the gaze, tilted to face the eyes, and its four
corners are projected through each tracked eye's declared FOV. Both eyes
sample the left eye's half of the double-wide scene target, so the stereo
depth is the panel's real position in space rather than a per-eye camera
offset. The player can lean or move around the pane and see it from an angle;
looking away leaves it where it was hung. The pane carries the whole
composited frame (room, models and HUD). It is off by default; the normal
screen-space interior presentation is unchanged.

The optional interior hover rig (`VrInteriorHover`, Display → VR → Interior
hover, console `vrhover`) treats the head as a camera flying over the room. A
head turn moves the isometric projection centre by exactly the angle turned
in the eye's own projection (`focal * tan(angle)`), so the room stays hung in
the world instead of following the face; looking far enough carries it out of
view. A head lean feeds the single-pass stereo stages a camera delta common
to both eyes (`GL_SetSinglePassBaseDelta`), so near planes move further than
far ones and the lean reads as depth. The brick background takes the same
projection delta (`Map2ScreenPanned`), so models and room cannot slide
against each other. Roll is held level: the room is one authored isometric
composite and rolling it would only rotate a painted image with no hidden
surfaces. The rig anchors the pose when it engages and recaptures on Enter or
a cube change, so enabling it never jumps the view. The head look runs inside
an authored camera zone too (the zone chooses the room framing, not the
projection centre); only cinema scenes keep the authored view.

Modal dialogue renders outside `MainLoop`'s stereo block but uses the same
per-eye renderer. Its box, text and portrait are 2D content in `Log`, and the
per-eye present composites `Log` into each eye, so a stereo dialogue draws the
overlay before the scene pass and lets that pass submit it once. A separate
overlay blit must never present on its own: a mono frame between per-eye pairs
makes both eyes sample the same half, which reads as one eye jumping every
frame. The per-eye pass clears `Log` so per-eye 2D content (rain drops and
object incrusts project per eye) cannot stack; the dialogue overlay is
snapshotted before the pass and restored after each eye's clear. `Screen` keeps
the pristine dialogue window strip the flat game only preserved because its
dialogue frames were incremental.

For a device experiment, `LBA2_OXR_DIRECT=1` (or a build configured with
`-DLBA2_OXR_DIRECT=ON`) renders both eyes and the HUD directly into the
acquired swapchain texture with `EXT_multisampled_render_to_texture`, removing
the resolve/blit pair. It is off by default, requires the resolve-in-tile probe
to pass and a neutral display transform, and falls back to the normal
double-wide blit on any mismatch or incomplete FBO. The runtime resolves the
texture when the compositor samples it; if the headset shows a black or
single-eye image, leave it off. `OpenXRPerf` reports a `direct=` count when the
path is active.

### Reviewing the stereo pair without the headset

`adb exec-out screencap` on a headset captures the compositor's two eye views,
but each half carries its own pose/reprojection, so the halves are not
comparable pixel by pixel. A desktop GL build can render the same pair
instead:

- `LBA2_FAKE_STEREO=30` forces the double-wide single-pass target on a desktop
  build and synthesises the eye-1 delta so a point at `aZ` 1000 lands 30 px
  away (the headset's room depths sit near 900-1300). `--screenshot` then
  writes both halves side by side at eye resolution.
- `LBA2_FAKE_STEREO_SS=1` additionally raises the scene target to the
  headset's 1.5x supersampling, so HUD band/scissor maths is exercised at a
  non-1:1 scale.
- `LBA2_SS15=1` raises the scene target to the headset's 1.5x supersampling,
  which is what makes resampling faults inside the interior's foreground-brick
  patches visible at all.
- `LBA2_OVERLAY_OLD=1` restores the earlier one-depth brick-overlay patch and
  `LBA2_NO_OVERLAY=1` turns the overlay off, to compare what it paints.
- `LBA2_INTERIOR_STEREO=<percent>` sets the interior stereo baseline as a
  percentage of the full eye separation (default 55). It applies to the
  game's own eye delta as well as to the fake pair, so a lower value shows the
  flattened room the headset renders.
- `LBA2_FAKE_VR_DIALOG=1` takes the head-mount dialogue layout (strip lifted
  above the eye's lower periphery), so the dialogue box can be reviewed over
  the room rather than over the desktop letterbox.

Repro: `LBA2_FAKE_STEREO=30 LBA2_FAKE_VR_DIALOG=1 xvfb-run -a lba2cc
--no-autosave --load <save> --exec-at 2 "cube 15" --exec-at 4 "teleport 25856
768 29000" --exec-at 30 "ui dialog 3 /tmp/dlg.png" --fixed-dt 16 --tick 120
--exit`. The dialog capture path runs `GL_SavePresentPNG`, which writes the
pair.

### Static terrain on the GPU

Stereo draws exterior terrain through a static VBO path instead of the
per-tile CPU projection. Each cube's heightfield, UVs, light and packed
material word are uploaded once as a VBO in cube-local coordinates, the same
frame `SetPosCamera()` shifts the camera into, and a vertex shader reproduces
`LongProjectPoint3D`. Clip-space `w` is the camera-space depth, so the GPU
clips geometry behind the eye; the fragment shader discards the sub-`NearClip`
sliver and anything past `ClipZFar`, matching the software path's per-tile
near/far gates. Screen-linear UV and depth interpolation, which the batch
shader gets from `w = 1`, is restored by premultiplying those varyings by `w`
and dividing by the interpolated `w`.

The current cube and the eight horizon neighbours use the path; animated
water/flower tiles stay in the VBO as their current frame and are also
projected on the CPU into `PtrAniPoly` so the animated-poly pass keeps
working. Cube VBOs are cached by `(x, y)` with LRU eviction and dropped when
`LoadIsland()` or `InitSeaCube()` replaces the per-cube HQR data. The cache
also drops stale handles after a renderer restart without deleting them.

The path is enabled only while a stereo eye is active, so desktop mono builds
and the ASM/CPU renderer are unchanged. `LBA2_GPU_TERRAIN=1` forces it on a
desktop GL build for visual comparison, `LBA2_GPU_TERRAIN=0` disables it, and
`LBA2_TERRAIN_NOCULL=1` turns off back-face culling while comparing against
the batch path.

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

### Static decors on the GPU

Opaque, non-animated decor bodies (buildings, rocks, signs, and similar
props) are replayed once in model space and cached as a per-`(body, beta)`
VBO. The vertex shader applies the decor rotation, the per-decor translation
and `MatriceWorld`, reproduces the CPU projection, and computes the
camera-space light; flat, flat-textured and gouraud shading select the same
palette/CLUT rows as the software handlers. The VBO is split into
per-texture-page groups and samples the 2D atlas page cache with its
`RepMask`, so the static path does not use the texture array that corrupted
pages on Adreno.

Transparent bodies, alpha/stipple polygons, environment mapping,
lines/spheres and bodies whose normals do not match their points remain on
the per-frame CPU stream path. The static path runs in mono and stereo GL
rendering. `LBA2_NO_DECOR_VBO=1` disables it and
`LBA2_DECOR_NOCULL=1` disables back-face culling for comparison.

The original software filler maps decor/object textures affinely
(types 8–15), so a large wall seen obliquely swims as the head moves. The
opt-in Display → Graphics → Perspective toggle makes the GL batch use its
existing per-vertex 1/W path for those types; it is off by default and does
not affect the software/CPU renderer.

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
