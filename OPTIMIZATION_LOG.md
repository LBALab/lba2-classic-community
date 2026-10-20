# Quest 72 FPS night — optimization log

Target: ~72 FPS (13.9 ms/frame) on Quest 3, stereo (OpenXR) mode.
Constraints: desktop-only verification (Linux build + host tests + Docker
ASM suite); every change gated so desktop behavior is unchanged. The
changes below land in small commits on the optimization branch.

## Baseline (Quest 3, in-game, third-person auto camera, MSAA on)

From `adb logcat` (`StereoPerf` / `GrillePerf` lines, 2026-09-25 23:29):

| Metric | Value |
|---|---|
| CPU frame (`StereoPerf frame`) | 18–27 ms (eye0 11–16 ms, eye1 7–11 ms) |
| Horizon (`GrillePerf horizon`) | 2–6 ms (CPU, eye 0 only) |
| Terrain (`GrillePerf terrain`) | 0–6 ms per eye (CPU) |
| Batch flushes per frame | 1 (batching is fine) |

`eye0 > eye1` is the horizon path: full MSAA resolve + full-surface blit +
8 neighbour cubes worth of vertex transforms. Frame time must drop ~2× to
reach 13.9 ms — and the measured CPU times include GL stalls from the GPU
being the bottleneck, so the GPU-side cuts below should also collapse the
stall component of those numbers.

## Changes landed on this branch

### GPU / frame pipeline

| # | Change | File | Expected effect |
|---|---|---|---|
| 1 | **Horizon capture color-only** (eye 0, every frame): the full color+depth resolve became color-only, and the capture blit drops its depth half under MSAA (the MSAA restore path never reads horizon depth — confirmed by its own comment). | RENDER_GL.CPP | −1 full-surface depth resolve + smaller blit per frame |
| 1a | **Resolve-in-tile v3 (enabled, device-verified)**: the first attempt sampled `s_fboColorTex` without forcing a resolve; v2 added `glInvalidateFramebuffer`, which Adreno treats as discard-don't-resolve. Both attached `s_fboColorTex` to the MSAA FBO as well as the scene FBO. v3 gives the MSAA FBO a dedicated `s_msaaColorTex`, attached only to that FBO and a single-sample resolve-view FBO. The composite samples the texture directly; the OpenXR direct present blits from the resolve view; snapshot frames explicitly copy color+depth into `s_fbo`. No `glInvalidateFramebuffer`. Driver rejection of either FBO, or a failed resolve probe, falls back to explicit resolve for the session. | RENDER_GL.CPP | removes the full-surface color resolve from every present; direct present resolves the scene once inside its blit |
| 3 | ~~Swapchain-image FBOs cached per image index~~ **REVERTED — found by on-device bisection.** The Meta runtime does not tolerate a persistent FBO attachment on its swapchain textures across xrAcquire/xrRelease (or re-backs the images on session resume): the composited frame landed in orphaned memory and the eyes saw black with only the 2D overlay (rain) showing. The original fresh `glGenFramebuffers` + attach + delete per present is restored — its cost is negligible next to the blit. | OPENXR.CPP | (would have removed per-frame FBO churn) |
| 4 | **Frame pacing**: `xrWaitFrame` + `xrBeginFrame` now run at frame START (from `OpenXR_BeginFrame`, one-shot `OpenXR_EnsureFrameBegun`) instead of after both eye renders inside `OpenXR_PresentMono`. CPU render work now overlaps the compositor's wait window and frames submit on time instead of late. Menu/title paths still funnel through the same one-shot. | OPENXR.CPP | removes submit lateness; pacing/stutter win |
| 5 | **Fixed foveation LOW** (XR_FB_foveation + configuration + swapchain_update_state, all ext-gated): the tiler renders the periphery at reduced bin density. The swapchain is created with `XR_SWAPCHAIN_CREATE_FOVEATION_SCALED_BIN_BIT_FB` — the method Adreno implements; chaining the create-info with flags 0 (the first attempt) left the runtime rendering the swapchain undefined. LOW chosen to be invisible behind the engine-resolution upscale; the knob is one enum (`XR_FOVEATION_LEVEL_*_FB`). | OPENXR.CPP | major fill-rate save on Quest |
| 6 | **Clock governor** (XR_EXT_performance_settings, ext-gated): GPU = BOOST, CPU = SUSTAINED_HIGH at session begin. | OPENXR.CPP | clocks needed for 72 Hz |

### CPU

| # | Change | File | Expected effect |
|---|---|---|---|
| 7 | **Neighbour-cube frustum cull** in `DrawHorizon2ZBuf` (`HorizonCubeOffscreen`): a horizon cube is skipped only when all 8 corners of its padded bounding box fail the SAME clip plane (exact for a convex box — a visible cube is never culled). Uses the engine's own rotate/project/clip sequence. Output-identical; ASM equivalence unaffected (invisible cubes draw nothing on either side). | SOURCES/3DEXT/TERRAIN.CPP | cuts a large share of the 2–6 ms horizon CPU when the view cone covers part of the neighbour grid |
| 8 | **Horizon-capture reuse**: the capture (eye 0) now skips its resolve-and-blit whenever the camera pose is unchanged since the previous capture AND no missing-cube sea placeholder drew this frame (`HorizonBackdropStable()` from TERRAIN.CPP guards against freezing a half-streamed backdrop). The common VR case — rig standing still, head looking around — previously paid the capture every frame for an identical image. | RENDER_GL.CPP, TERRAIN.CPP, EXTFUNC.CPP | removes a full-surface blit per frame while stationary |
| 10 | **Per-eye terrain snapshot skip**: `GL_SnapshotTerrain()` (full-surface color+depth blit at eye resolution) ran once per EYE every frame, but its output is consumed only by incremental-frame restores — and stereo eye passes are always AFF_ALL_FLIP full redraws. Skipped inside stereo eye passes (`GL_GetStereoEye() < 0` gate); mono/menu frames still snapshot. | SOURCES/EXTFUNC.CPP | −1 full-surface blit per eye per frame (2/frame) |
| 9 | **Build fixes that unblock the ASM suite**: (a) `SVGA/SDL.CPP` referenced `GPURendererActive` outside the `USE_GPURENDERER` guards (added in 0ee18ee0); (b) `DisplayBrightness`/`Contrast`/`Saturation`/`VrFovScale` declarations lived in a GL-only guard while the console cvars and cfg load/save reference them unconditionally, and the definitions were guarded too (moved out — the values are inert without the GL renderer). Every non-GL configuration — including the Docker linux_test preset the ASM equivalence suite runs in — failed to compile; the suite now builds and runs again on this branch. No behavior change with the GL renderer on. | LIB386/SVGA/SDL.CPP, SOURCES/C_EXTERN.H, SOURCES/GLOBAL.CPP | Docker ASM↔CPP suite builds and runs again on this branch |
| 11 | **Stereo depth-readback removal**: `GL_ReadbackDepthBuffer()` resolves the full depth buffer into a PBO and maps it on the next call. Stereo eye passes are always `AFF_ALL_FLIP`: each `AffGrilleExt` clears `PtrZBuffer` before the object pass, so neither eye can observe the readback. Skip it when `GL_GetStereoEye() >= 0`; mono incremental frames keep the one-frame-old depth. | SOURCES/OBJECT.CPP | −1 full-surface depth resolve + PBO read per frame |
| 12 | **Empty-overlay direct present**: the indexed present already detects an all-zero Log overlay. When that overlay is empty, the viewport is full, the display transform is neutral, no FXAA/SMAA is active, and the scene FBO is content-sized, OpenXR can blit the resolved 3D scene directly for mono and for stereo eye 1 (the last eye rendered). Eye 0 still composites because eye 1 overwrites the scene FBO. A new `GL_StereoEyeSceneDirect()` handshake tells the stereo present to read the scene for the right eye; `GL_PresentSceneDirect()` reports the same safety for the mono fast path. The previous mono fast path could bypass a non-neutral display transform; it now shares the full gate. | RENDER_GL.CPP, OPENXR.CPP, RENDER_GL.H | −1 full-screen composite on mono/eye-1 empty-overlay frames |
| 13 | **Present state cleanup**: drop the redundant full-target `glViewport` before the scissored letterbox clear (`glClear` ignores the viewport), and the duplicate vertex-array/program teardown at the end of the plain present branch. | RENDER_GL.CPP | small CPU/driver-state win |
| 14 | **Stereo horizon capture lifetime**: `GL_BeginFrame` runs at every eye's present, so clearing `s_horizonValid` there discarded eye 0's capture between the two eyes; eye 1 always redrew the horizon (`GrillePerf eye=1 draw=2–7 ms, restore=0`). The flag is now owned by `GL_SetStereoEye`: cleared when a pair starts and when stereo is left, preserved across eye 0's present. Mono/menu frames still start invalid. | RENDER_GL.CPP | removes the duplicate eye-1 horizon pass (~2–7 ms/frame) |

The v3 in-tile path sets `GL_INTILE_MSAA 1`. Desktop GL does not expose
`GL_EXT_multisampled_render_to_texture`, so desktop keeps the explicit path;
the Quest native build compiles the in-tile branch. The failure mode of the
earlier attempts is removed: the resolve texture is never attached to
`s_fbo`, and the resolve is triggered by sampling/blitting the texture rather
than by invalidating it. At creation the renderer clears the MSAA target and
reads one pixel through the resolve view; a stale or black read falls back to
explicit resolve for the session instead of risking a black headset.

### CPU measurement (Linux micro-benchmark, tests/bench_terrain)

`bench_terrain` drives the real engine math (LongRotatePointF,
LongProjectPoint3D, InitMatrixStdF, SetFollowCamera) over a synthetic
65×65 cube with the same loop shape as AffichageTerrainZBuf:

- vertex rotate+project: **12 ns/vertex** → 4225 verts ≈ 0.05 ms
- tile mask+classify+radix sort: **5 ns/tile** → 4096 tiles ≈ 0.02 ms
- one cube at two eyes ≈ **0.14 ms/frame** (host; scale ×3–4 for A76)

### Old-math survey (datatypes / un-accelerated operations)

- **`DISTANCE.CPP` used `sqrtl`** — on 64-bit ARM `long double` is a
  software 128-bit type, so every distance was a soft-float libc call, and
  distances run per object per frame (SORT, AMBIANCE samples, GERETRAK
  trails, BUGGY wheels).  Replaced with a hardware double sqrt plus an
  exact floor correction (bit-identical to the old `(U32)sqrtl`
  truncation; the 64-bit squares also remove a distant-object wrap).
  Verified by a 200k-sweep + edge cases against the `sqrtl` reference in
  `bench_terrain`.
- Surveyed and clean: `RegleTrois` (S64 math), `GETANG2D` (integer binary
  search over TanTab), `SQRROOT` (exact integer port), `RotTransList` /
  `ProjectList3D` (hardware double, per body), `LROT2D/3D` (hardware
  double, kept for ASM parity), `MOVE` `ldiv` (per step, rare), and
  `POLY.CPP` `volatile long double` (SW-filler only, not the GL path).

**Conclusion: the terrain math is NOT the Quest CPU bottleneck** — and the
per-eye CPU duplication is not either.  The depth readback consumes a
pixel-pack PBO filled on the *previous* frame, so when the GPU runs
behind, the CPU eye times in `StereoPerf` mirror the previous frame's GPU
execution rather than real CPU work.  The path to 72 is therefore
GPU-side, which is what items 1a (resolve-in-tile) and 5 (foveation)
address; further CPU micro-optimization of the render loop will not move
the number.

### Verification status

- Desktop build clean; host_quick suite 48/49 — the one failure
  (`test_poly_geometry`) fails at the base commit too (pre-existing).
- Quest native build (NDK, real OpenXR headers) clean; fixed APK
  (`lba2cc-0.13.0-dev-android-arm64-v8a.apk`) built. On-device 2026-10-20:
  `[GL] MSAA FBO created 1680x1760 samples=2 (resolve-in-tile)` appeared and
  the scene rendered, confirming the in-tile probe passed on the Adreno 740.
- Docker ASM↔CPP suite: builds again after #9 (it had been broken on this
  branch since 0ee18ee0). 123/143 pass. The 20 failures (test_poly,
  test_polyrec_*, test_aff_obj, test_testvuef, test_poly_geometry) are all
  in the ASM↔CPP **software polygon pipeline** — untouched by this change
  set, and largely not even compiled in the stereo/GPU configs. They are
  pre-existing branch state, newly *visible* because the suite builds again;
  they need their own investigation.
- Device findings so far: with the first build, the 3D scene rendered black
  (rain/overlay visible) → traced to #1a, reverted. Re-check `StereoPerf` /
  `GrillePerf` lines plus the init lines (`[GL] MSAA FBO created ...`,
  `[OpenXR] fixed foveation LOW enabled`, `PerfSettings GPU=boost`) on the
  fixed build. If anything still misbehaves, the remaining suspects bisect
  one line each: foveation (`s_extFoveation = 0` at resolve time), pacing
  (`OpenXR_BeginFrame`'s swapchain branch), perf level.
- The items added in #11–#13 are built for desktop and Quest but were not
  re-measured or visually checked on the headset (the owner asked for
  verification-free work); pair a future device run with `StereoPerf` and
  `GrillePerf` before treating the numbers as final.
- Resolve-in-tile v3: Linux and Quest native builds clean. Linux headless
  scene capture and `ui_display` pass on the software path. Quest 3
  (Adreno 740) confirms `[GL] MSAA FBO created 1680x1760 samples=2
  (resolve-in-tile)`; `GrillePerf` shows `resolve=0us` and a 28–95 us
  capture blit, with no black frames. Desktop has no
  `GL_EXT_multisampled_render_to_texture`, so the in-tile branch itself
  remains device-only coverage.
- Item 14 (stereo horizon capture lifetime): Quest native build clean and
  APK rebuilt. The pre-fix one-minute sample showed eye 1 redrawing the
  horizon every frame (`draw=2–7 ms`, `restore=0`); on-device A/B
  verification is pending because the headset disconnected after that
  sample.

## Remaining levers (deliberately NOT taken — behavior changes, owner's call)

- Lower render-resolution ladder step (linear CPU+GPU win, visual).
- Share the terrain pass between eyes (breaks near-geometry stereo depth).
- MSAA → FXAA default on Android (visual).
- Foveation level MEDIUM/HIGH (peripheral blur after upscale is likely
  invisible — one-enum change if LOW alone isn't enough).

## Results (build A, on-device, 2026-09-26 22:38-22:45)

Build A = everything except in-tile (bisected out: in-tile v2 caused the
graphical issues; the explicit-resolve scheme stays).  25 samples of
`StereoPerf frame` during normal play:

| | baseline | build A |
|---|---|---|
| min | 18 ms | **14 ms** (~71 FPS moments) |
| median | ~25 ms | **20 ms** |
| avg | ~24 ms | **20 ms** |
| max | 37 ms | 29 ms |

~25% median improvement and the frame floor now touches the 72 Hz budget,
but the median is still above 13.9 ms — the remaining GPU per-frame cost
(two full scene passes at 1680x1760 with MSAA×2 + composite + copy) needs
either a working resolve-in-tile, a lower ladder step, foveation MEDIUM,
or MSAA→FXAA to close the gap.  All are recorded above; the first two are
driver-behavior experiments that need on-device iteration, the last two
are one-line trade-offs.  Item 1a v3 is the resolve-in-tile candidate for
that next device run.

## One-minute sample before item 14 (Quest 3, 2026-10-20 11:54-11:55)

Resolve-in-tile v3 is active.  Seven `StereoPerf` samples (logged every
300 frames) and fourteen `GrillePerf` eye samples during normal play:

| Metric | min | median | max |
|---|---|---|---|
| `StereoPerf eye0` | 10 ms | 16 ms | 19 ms |
| `StereoPerf eye1` | 4 ms | 14 ms | 16 ms |
| `StereoPerf frame` | 14 ms | 30 ms | 35 ms |
| `GrillePerf horizon` eye 0 | 3 ms | 5 ms | 7 ms |
| `GrillePerf horizon` eye 1 | 2 ms | 4 ms | 7 ms |
| `GrillePerf terrain` | 0 ms | 1 ms | 6 ms |
| `GrillePerf skysea` | 0 ms | 0 ms | 1 ms |

Eye 1 logged `draw=` and `restore=0` on every sample: it was redrawing the
horizon that eye 0 had already captured.  The 14 ms best case coincides with
a frame where both horizon draws measured 0 ms; it is not the steady state.
The runtime line at the start of the sample read `FPS=32/72`,
`CPU&GPU=37.68ms`, so the frame is still GPU-bound.  Item 14 removes eye 1's
2–7 ms duplicate; the next sample should show `eye=1 restore=` with a small
microsecond cost.  Eye 0's horizon and the two scene passes are the next
targets after that.
