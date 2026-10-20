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

`eye0 > eye1` was attributed here to the horizon path: full MSAA resolve +
full-surface blit + 8 neighbour cubes worth of vertex transforms. The
2026-10-20 re-audit below shows the eye-0 present composite is a second,
independent contributor. Frame time must drop ~2× to reach 13.9 ms — and the
measured CPU times include GL stalls from the GPU being the bottleneck, so the
GPU-side cuts below should also collapse the stall component of those numbers.

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
| 14 | **Stereo horizon capture lifetime (reverted)**: `GL_BeginFrame` runs at every eye's present, so clearing `s_horizonValid` there discarded eye 0's capture between the two eyes; eye 1 always redrew the horizon (`GrillePerf eye=1 draw=2–7 ms, restore=0`). Moving the flag to `GL_SetStereoEye` made eye 1 restore it (`restore=41–53 us`) and cut the one-minute frame median from 30 ms to 17 ms, but Iceland showed terrain seams: near neighbour-cube terrain has real stereo disparity, so eye 0's capture is not valid for eye 1. Reverted to per-eye drawing. | RENDER_GL.CPP | no net effect; a correct reuse needs parallax-aware compositing or a neutral far-horizon prepass |

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
- Item 14 (stereo horizon capture lifetime): the lifetime fix worked as
  measured on 2026-10-20 12:06–12:07 — every `GrillePerf eye=1` sample
  logged `horizon=0ms draw=0ms restore=41–53us`, and ten `StereoPerf`
  samples ran 11–25 ms (median 17 ms). But Iceland showed terrain split
  into misaligned parts: adjacent neighbour-cube terrain is often close
  enough to have visible stereo disparity, so eye 0's capture cannot stand
  in for eye 1. The code change was reverted; the pre-fix per-eye draw is
  the correct behavior. The numbers remain useful as the ceiling for a
  parallax-aware reuse.

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
(two full scene passes at 1680×1760 with MSAA×2, the eye-0 composite, and the
two OpenXR swapchain blits) needs either a working resolve-in-tile, a lower
ladder step, foveation MEDIUM, or MSAA→FXAA to close the gap.  All are
recorded above; the first two are driver-behavior experiments that need
on-device iteration, the last two are one-line trade-offs.  Item 1a v3 is the
resolve-in-tile candidate for that next device run.

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
`CPU&GPU=37.68ms`, so the frame is still GPU-bound.

Item 14 reproduced the predicted win (eye 1 `restore=41–53 us`, frame median
17 ms) but failed visually in Iceland, so it is reverted.  Eye 0's horizon,
the two scene passes, and the eye-0 composite/OpenXR blits remain the biggest
costs.  A future horizon reuse needs to handle near neighbour-cube parallax —
for example a depth-aware composite that restores only the far backdrop, or a
neutral-camera horizon prepass — rather than treating the whole capture as
eye-independent.

## 2026-10-20 re-audit: present chain and GPU memory

Current branch `quest-good-perf` at `04365c82` (body-line depth fix on top of
the reverted horizon reuse and resolve-in-tile v3). This audit was read-only;
the implementation plan is in [plan.md](plan.md).

### Measurements

Latest in-game sample:

| Metric | Value |
|---|---|
| `StereoPerf eye0` | 10 ms (14–15 ms in heavier scenes) |
| `StereoPerf eye1` | 6 ms (11–12 ms in heavier scenes) |
| `StereoPerf frame` | 16 ms (25–27 ms in heavier scenes) |
| `GrillePerf horizon` | 4 ms eye 0, 2 ms eye 1 |
| `GrillePerf terrain` | 1 ms per eye |
| `GrillePerf skysea` | 0–1 ms |
| `GLPerf present` | about 2.3–2.6 ms per present |
| `GLPerf flush` | about 1.3 ms per present |
| `GLPerf flushes` | 53–78 per sample |

The device log also confirms the current Android path is **2x MSAA** at
1680×1760 (`SetAntiAlias: 1 -> 2`, `MSAA FBO created 1680x1760 samples=2
(resolve-in-tile)`). The target remains 72 FPS; no quality setting changes are
part of this plan.

The last captured app telemetry reported `GPU=154.1/154.1 MiB (100%
allocated)`. That sample predates the body-line commit, but the set of
full-resolution surfaces has not been reduced since. The accounting is on the
order of 150 MiB: scene MSAA color (~22.6 MiB), scene depth (~11.3 MiB),
`s_fbo` (~17.7 MiB), two eye FBOs (~23.6 MiB), present FBO (~11.8 MiB),
horizon FBO (~17.7 MiB), snapshot FBO (~17.7 MiB), log index (~3 MiB), texture
array (16 MiB), and atlas cache (~8 MiB). Not every surface is live at every
instant, but the engine is operating with no GPU memory headroom.

### Findings

1. **The eye-0 composite is a large avoidable asymmetry.** Phase 0 measured a
   2–4 ms gap between the eyes. Part of the extra is the horizon; the rest is
   `GL_PresentIndexed` compositing the full scene into `s_stereoEyeFBO[0]`.
2. **The present chain makes four full-resolution copies and two resolves
   per frame.** Both eye composites run, then both eye FBOs are blitted into
   the swapchain. The eye-1 direct path was inactive in the measured scene
   because the overlay was non-empty. Present is 2.35 ms per present and
   flush is 2.99 ms per present; their sum is an upper bound of roughly 10.7
   ms/frame because present includes any pending flush at its start.
3. **The horizon capture is dead in stereo.** `GL_CaptureHorizon` still runs
   for eye 0 and still allocates a full-resolution color/depth surface, but
   `GL_BeginFrame` clears `s_horizonValid` before eye 1 and item 14's restore
   is reverted. Eye 1 redraws the horizon correctly. The capture can be
   disabled without changing the current image.
4. **Several full-resolution surfaces are allocated but unused in the stereo
   fast path.** `s_stereoEyeFBO[1]` is not read when eye 1 is direct.
   `s_presentFBO` is a mono/menu target. The snapshot and `s_fbo` surfaces are
   not used by full-redraw stereo eye passes. They can be lazy or freed when
   the stereo double-wide target is active.
5. **OpenXR present performs per-frame validation and state queries.** A
   fresh swapchain FBO per present is intentional (item 3 records the black
   frames from persistent attachments), but `glGetError`,
   `glCheckFramebufferStatus`, and `glGetIntegerv` do not need to run on every
   release frame. They can be debug-only, while keeping the fresh FBO object.
6. **The overlay path scans and clears full-resolution buffers per eye.** The
   indexed present does a row-by-row `memcmp` of the `Log` buffer for each eye
   to detect an empty overlay, and `ExtFunc` memsets both `Screen` and `Log`
   for each eye. Phase 0 measured `ovlEmpty=0.00` in every gameplay sample,
   so the direct path never activated. Identify the 2D writer that keeps `Log`
   non-empty, then drive the scan from a dirty flag instead of assuming an
   empty overlay.
7. **The atlas content version is global.** Any animated water, plasma, or
   terrain mutation invalidates every array layer and every cached atlas page,
   so one changed page can re-upload many unchanged pages. This is a likely
   source of `Stale` spikes and should be replaced by per-page dirty tracking.
8. **The batch flush rebinds the framebuffer on every flush.** The first-flush
   path already binds the scene FBO; the unconditional rebind later in
   `FlushActiveBatch` is redundant. The flush reason fields are also all zero,
   so there is no current attribution for the 30–110 flushes per 300 presents.
9. **The 1:1 stereo blits use `GL_LINEAR`.** At exact source and destination
   sizes, `GL_NEAREST` is visually identical and cheaper.
10. **The direct eye-1 path is inactive in gameplay.** Phase 0 measured
    `ovlEmpty=0.00` and `direct=0.00` in all 21 consecutive 300-present
    windows (about 55 s of play). The overlay is not empty, so eye 1 takes the
    full composite path. This is no longer a startup/menu-only observation.

### What this changes

The 2026-09-25 conclusion that the frame is GPU-bound still holds, but the
attribution needs to include the present chain. The horizon measured 3–6
ms/frame in Phase 0; it is not the only large cost, and optimizing it alone
cannot reach 13.9 ms. Both eye composites plus the two swapchain blits are a
comparable, largely avoidable bucket.

Item 12 is only a partial fix: it removes the eye-1 composite when the direct
path can activate, and Phase 0 shows that condition is false in the measured
gameplay. Item 14 stays reverted; future horizon reuse must be vertex-space or
depth-aware, not an image copy. The eye-0/eye-1 cost gap is not proof that the
horizon is the only problem.

### Phase 0 measurement (2026-10-20)

55 s mixed-scene sample on the Quest 3: 11 `StereoPerf` windows, 21 `GLPerf`
windows, and 93 VrApi samples. Medians unless noted:

| Signal | Measurement |
|---|---|
| `StereoPerf eye0` | 14 ms (mean 14.7, p90 19, max 20) |
| `StereoPerf eye1` | 12 ms (mean 12.5, p90 17, max 18) |
| `StereoPerf frame` | 26 ms (mean 27.2, p90 37, max 37) |
| `GrillePerf horizon` | 3 ms (mean 3.8, p90 6.9) |
| `GrillePerf terrain` | 3 ms (max 5) |
| `GLPerf present` | 2.35 ms per present |
| `GLPerf flush` | 2.99 ms per present |
| `GLPerf flushes` | 82 per 300 presents (30–110) |
| `OpenXRPerf total` | 0.56 ms per present; wait 0.00, blit 0.06 |
| `VrApi FPS` | 41 median (p90 67.6) |
| `VrApi CPU&GPU` | 29.8 ms median |
| `VrApi App` | 4.7 ms median |

Key results:

- **OpenXR is not the bottleneck.** `frameBegin` is 0.11 ms, `wait` and
  `acquire` are 0.00, `blit` is 0.06, and `end` is 0.43. The frame time is in
  scene rendering and the GL present/flush path.
- **Both eyes composite.** `ovlEmpty=0.00` and `direct=0.00` in every sample.
  The eye-1 direct path never activated, so the frame pays two eye composites,
  two resolves, and two swapchain blits.
- **Flush time tracks scene load.** `GLPerf flush` rises from about 1.1 ms per
  present at 30 flushes to 4.7 ms at 110 flushes. `FlushActiveBatch` wraps
  `glDrawArrays`, so this includes GPU backpressure, not just CPU submission.
- **Memory is tight.** The engine surface estimate is 79.0 MiB at init, 101.5
  MiB after the snapshot target, and 124.1 MiB after the horizon target,
  before the texture array and atlas cache.
- **Configuration is as expected.** `FpsLimit=0`; `SetAntiAlias` is 2x MSAA;
  `truepose=0` in `StereoPerf`.

### Plan

The full phased plan is in [plan.md](plan.md). The order is deliberate:

1. Measure simulation, OpenXR wait, eye phases, presents, and surface bytes.
2. Replace the per-eye scene copies with one double-wide MSAA scene target and
   one present blit. This removes both eye composites and two full-screen
   copies while preserving per-eye resolution and 2x MSAA.
3. Disable the dead horizon capture and lazy/free the unused full-resolution
   surfaces.
4. Remove per-frame OpenXR validation/state queries while keeping the fresh
   swapchain FBO lifecycle that item 3 requires.
5. Remove duplicate overlay scans/clears and redundant per-flush framebuffer
   binds.
6. Reuse eye 0's horizon vertices in camera space for eye 1; never restore the
   image capture.
7. Only if more headroom is needed, attach the swapchain texture directly to
   the MSAA FBO and remove the final blit.

Expected combined effect is larger than the eye-0 composite alone: 3–5 ms from
the double-wide present path, 0.5–2 ms from OpenXR cleanup, 0.5–1 ms from
overlay/clear, 1.5–3 ms from vertex-space horizon reuse, and 1–2 ms from the
flush path. The first two phases are the ones to build and verify first.

### Open questions

- Confirmed: `FpsLimit=0`, 2x MSAA is active, and the gameplay overlay is
  non-empty, so the direct path is inactive.
- Add GPU timer queries before treating `GLPerf present` or `GLPerf flush` as
  pure CPU time; the wall-clock numbers include driver stalls.
- Verify whether the Adreno 740 accepts a double-wide in-tile MSAA texture and
  resolve view at 3360×1760. If not, fall back to the current per-eye path.
- Re-check `VrApi` `App`, `CPU&GPU`, and `GPU%` after the present change; the
  current fields are not sufficient to separate CPU submission from GPU
  memory stalls.

## Phase 1 report: double-wide stereo scene target (2026-10-20)

Status: implemented, built for Quest, installed as `0.13.0-dev`, and
committed as `542ef99a`. The user reported better performance and no visual
issues after the first device run. The captured run still selected the
per-eye fallback, so the double-wide path itself was not exercised on device.

### Design

- `s_fbo` and `s_msaaFbo` are physically double-wide (`2 * eyeWidth` by
  `eyeHeight`). `s_fboWidth`/`s_fboHeight` stay the per-eye content size, so
  projection, object culling, HUD coordinates, snapshots, and the mono path
  keep their existing math.
- Eye 0 renders at viewport `(0, 0, eyeWidth, eyeHeight)`; eye 1 renders at
  `(eyeWidth, 0, eyeWidth, eyeHeight)`. `GL_SetSceneViewport()` is used by
  every scene bind (batch flush, shadow, sphere, line, shade box, sprite,
  snapshot restore, horizon restore) and by the present overlay.
- `GL_ClearFBO` clears only the current eye half with a scissor. A full-target
  clear would erase the first eye before the second one renders.
- `GL_PresentIndexed` and `GL_Present` no longer composite the scene. They bind
  the scene target, draw only the current eye's HUD into its half, and on
  eye 1 resolve the target once and mark the stereo pair ready.
- `OpenXR_PresentMono` reads the resolved pair through the new
  `GL_GetStereoPairFBO/Width/Height` accessors and performs one blit from
  `(0,0,2*eyeWidth,eyeHeight)` to the full double-wide swapchain. The blit uses
  `GL_NEAREST` at matching sizes and `GL_LINEAR` only when scaling.
- `GL_CaptureHorizon` returns early during stereo passes. Eye 1 still draws its
  own horizon, so the image is unchanged; the capture surface is not allocated
  and the dead capture blit is not paid. The early return still flushes the
  current eye's horizon batch before the next eye starts.
- Eye FBOs are not allocated while the double-wide path is active. They are
  created when the renderer switches back to the per-eye fallback.

### Fallback gates

The double-wide path is used only when all of these hold:

- OpenXR is active and stereo is enabled.
- Anti-aliasing is MSAA 2x or 4x.
- Brightness, contrast, and saturation are neutral.

Non-neutral image adjustment and FXAA/SMAA need the per-eye scene composite,
so they use the old eye-FBO path. An incomplete double-wide FBO also falls
back. `LBA2_NO_STEREO_DW=1` forces the old path for an on-device A/B without a
rebuild. The fallback still uses `s_sceneEyeX=0`, so its behavior is the same
as before.

### Memory

The renderer logs `surfaces(stereo-double)` when the target is created and
`surfaces(stereo-per-eye)` when it falls back. With no snapshot or horizon
surface, the existing accounting estimate is about 101.5 MiB versus the
Phase 0 124.1 MiB figure; the logger counts depth as 4 bytes, so the real
depth-16 total is lower. Phase 2 now drops the mono snapshot when stereo
starts; the per-eye fallback still allocates the horizon surface because it
consumes the capture.

### What to verify on device

1. Same Iceland exterior save, same camera mode, MSAA 2x, stereo on.
2. Run for at least one minute with `perftrace start`.
3. Check that the startup log contains:
   - `[GL] Stereo scene target: double-wide`
   - `[GL] Scene FBO 3360x1760 (eye 1680x1760, double-wide)`
   - `[GL] MSAA FBO created 3360x1760 samples=2 (resolve-in-tile)`
   - `[Perf] surfaces(stereo-double): ...`
   - `[OpenXR] PresentMono stereo: double-wide ...` once
4. Compare `StereoPerf eye0`, `eye1`, and `frame`, `GLPerf present`,
   `flush`, `ovlEmpty`, and `direct`, `OpenXRPerf blit`, `GrillePerf horizon`,
   and the VrApi `FPS`, `CPU&GPU`, and `App` lines against the Phase 0 table.
5. Visual checks: terrain seams in Iceland, horizon stability, HUD alignment
   (both eyes), medallion chain and whiskers occlusion, console overlay, and a
   short interior check.

### Device run (2026-10-20 14:04–14:05)

User report: performance is better, with no visual issues detected. The
captured run is 13 `StereoPerf` windows, 30 `GLPerf` windows, and 17
`OpenXRPerf` windows. Medians unless noted:

| Signal | Phase 0 | Phase 1 run |
|---|---|---|
| `StereoPerf eye0` | 14 ms | 12 ms |
| `StereoPerf eye1` | 12 ms | 10 ms |
| `StereoPerf frame` | 26 ms | 21 ms |
| `GLPerf present` | 2.35 ms | 1.50 ms |
| `GLPerf flush` | 2.99 ms | 2.33 ms |
| `GrillePerf horizon` | 3 ms | 2.5 ms |
| `OpenXRPerf blit` | 0.06 ms | 0.06 ms |

The captured surface log still reported the per-eye totals (`79.0 MiB` at
init, `124.1 MiB` after snapshot and horizon), and `PresentMono stereo:
per-eye`; the `stereo-double` line was not present in this capture. The gate
state is left for the Phase 2 memory pass to recheck, per the request to stop
the immediate investigation.

### Risks and rollback

- The main risk is Adreno rejecting or mis-resolving the 3360x1760 in-tile
  MSAA target. The existing probe and FBO-complete checks fall back to the
  explicit MSAA path, and an incomplete double-wide FBO falls back to the old
  eye-FBO path. If the device shows black or torn eye halves, set
  `LBA2_NO_STEREO_DW=1` and report the first `[GL]`/`[OpenXR]` warning.
- HUD alignment is the other risk: the overlay viewport and scissor must match
  the eye half. A horizontal offset or duplicated HUD means the eye viewport
  did not follow `s_sceneEyeX`.
- The horizon capture is intentionally off in stereo. Eye 1 drawing its own
  horizon is the known-good behavior from the reverted image-reuse attempt; do
  not restore the capture copy.
- No resolution, MSAA, foveation, or settings change is part of this phase.

## Phase 2 report: surface teardown (2026-10-20)

Status: implemented, built, installed as `0.13.0-dev`, and committed as the
Phase 2 memory change. The latest device run still selected the per-eye path,
so only the snapshot teardown was exercised on device.

### Changes

- `GL_SetStereoEye(0)` drops any snapshot left over from a mono frame before
  the stereo target switch. Stereo eye passes are full redraws and do not call
  `GL_SnapshotTerrain`, so the surface is dead.
- `GL_SetStereoTargetMode(1)` drops the snapshot, horizon capture, and mono
  present target before allocating the double-wide scene. This avoids an
  extra full-resolution peak at the transition.
- The mono present target is created lazily by `GL_EnsurePresentFBO()` when a
  mono or fallback present needs it.
- `GL_Renderer_Resize` keeps the present target absent while the double-wide
  path is active.
- The horizon surface is now destroyed on renderer shutdown instead of being
  leaked.

### Device run (2026-10-20 14:23–14:26)

The run was single-wide throughout: `Scene FBO 1680x1760`, no
`[GL] Stereo scene target: double-wide`, and no `surfaces(stereo-double)`
line. The snapshot teardown did fire: `surfaces(snapshot)` reported 79.0 MiB
with the snapshot, then `surfaces(horizon)` reported 79.0 MiB with
`snapshot=0` and the per-eye horizon surface at 22.6 MiB. The 2x MSAA target
was created later in the run, so those samples exclude it; with the same
accounting it would be about 101.7 MiB, roughly 22 MiB below the Phase 1
124.1 MiB figure.

| Signal | Phase 1 run | Phase 2 run |
|---|---|---|
| `StereoPerf eye0` | 12 ms | 13 ms |
| `StereoPerf eye1` | 10 ms | 9 ms |
| `StereoPerf frame` | 21 ms | 21 ms |
| `GLPerf present` | 1.50 ms | 1.53 ms |
| `GLPerf flush` | 2.33 ms | 2.62 ms |
| `GrillePerf horizon` | 2.5 ms | 2.0 ms |
| `OpenXRPerf total` | 0.56 ms | 0.56 ms |

Performance is within noise of Phase 1. The double-wide-only frees (horizon
and present) were not exercised, and the memory target cannot be confirmed
until that path activates.

### Next priority (resolved)

The per-eye fallback was the blocking issue. The double-wide gate still
rejected the run because the display brightness, contrast, or saturation was
non-neutral: the double-wide present had no display-transform pass. Phase 1b
below applies that transform in the final pair-to-swapchain pass, so the
fallback no longer depends on the user's display settings.

## Phase 1b report: double-wide present transform (2026-10-20)

Status: implemented, built, installed as `0.13.0-dev`, device-verified, and
committed as the Phase 1b change. The headset user confirmed that the double-
wide path has no graphical issues.

### Root cause

After Phase 2, the per-eye fallback was still selected. Once the MSAA and
OpenXR conditions were true, the remaining gate was the display transform:
the per-eye composite applied brightness/contrast/saturation, while the
double-wide present was a plain blit. Removing that condition immediately
selected the double-wide target on device, confirming that non-neutral display
settings were keeping the fallback alive. The correct fix was to move the
transform into the final pair pass, not to ask the user to reset the sliders.

### Changes

- `GL_StereoDoubleWideAllowed` no longer rejects non-neutral brightness,
  contrast, or saturation. It now logs one line per distinct gate failure with
  the live values, so a future fallback is diagnosable from a device run.
- `GL_StereoPairNeedsPresentShader` selects the transform pass when the
  display sliders are non-neutral. Neutral settings keep the cheaper plain
  `glBlitFramebuffer` path.
- `GL_BlitStereoPairToCurrent` draws the resolved double-wide pair into the
  already-bound swapchain framebuffer through the existing present shader.
  That pass applies the same display transform as the per-eye composite. It
  uses nearest sampling at exact 1:1 sizes and linear sampling when scaling.
- The HUD drawn into the double-wide scene skips per-layer image adjustment.
  The final pair pass applies the affine transform once to the combined scene
  and overlay, which is equivalent through alpha blending.
- FXAA/SMAA and double-wide FBO failure still use the per-eye fallback. The
  fresh per-present swapchain FBO remains unchanged.
- If the swapchain FBO attach is rejected, the copy fallback cannot apply the
  display transform. The current Quest path completes the FBO and uses the
  shader; a future portable fallback should composite through a temporary
  target instead of silently dropping the sliders.

### Device run (2026-10-20 14:51–14:52)

The path was active from startup:

```
Scene FBO 3360x1760 (eye 1680x1760, double-wide)
MSAA FBO created 3360x1760 samples=2 (resolve-in-tile)
surfaces(stereo-double): fbo=45.1 msaa=45.1 ... total=90.2 MiB
PresentMono stereo: double-wide src=3360x1760 ... glError=0x0
```

The double-wide surface line has no snapshot, horizon, or present target.
A later mono/menu frame allocated the snapshot and present target
(`surfaces(snapshot)` showed 124.1 MiB); the next stereo entry frees the
snapshot, but the present target can remain allocated because the target mode
is already double-wide. That residual is a small follow-up, not a visual
issue.

The startup screenshot showed distinct left/right eye views with correct
orientation. The user then confirmed in the headset that both eyes, the
display transform, and the previously sensitive terrain/body/HUD cases look
correct.

| Signal | Phase 2 run | Phase 1b run |
|---|---|---|
| `StereoPerf eye0` | 13 ms | 13 ms |
| `StereoPerf eye1` | 9 ms | 10 ms |
| `StereoPerf frame` | 21 ms | 23 ms |
| `GLPerf present` | 1.53 ms | 1.40 ms |
| `GLPerf flush` | 2.62 ms | 2.86 ms |
| `GrillePerf horizon` | 2.0 ms | 3.0 ms |
| `OpenXRPerf total` | 0.56 ms | 0.51 ms |
| `OpenXRPerf blit` | 0.06 ms | 0.05 ms |

The frame time ranged from 14 to 30 ms depending on the scene, with a median
of 23 ms. The memory win is clear and the path is now real, but this does not
yet reach the 72 FPS target. The next work should attack the steady frame
cost rather than the activation gate.

## Phase 3 report: OpenXR present cleanup (2026-10-20)

Status: implemented and compile-verified against the Quest preset. The headset
was disconnected for this phase, so the owner deferred the on-device visual
check to the morning; the change is committed on that basis.

### Changes

- The release present path no longer calls `glGetError` on every frame. A
  `LBA2_OXR_GLDEBUG=1` environment flag enables the checks and logs the first
  error at each present stage.
- `glGetIntegerv(GL_FRAMEBUFFER_BINDING)` and
  `glGetIntegerv(GL_TEXTURE_BINDING_2D)` are gone. The present restores the
  framebuffer to 0 and the texture to 0; the renderer invalidates its state
  cache at the end of every present, so the next frame rebinds explicitly.
- Each swapchain image's FBO completeness is checked on first use, not every
  present. The result is cached per session and reset when the swapchain is
  recreated. An incomplete image takes the existing copy fallback; the fresh
  FBO object per present is unchanged.
- Exact 1:1 per-eye and mono blits use `GL_NEAREST`; scaled blits keep
  `GL_LINEAR`.
- The stereo pair still uses the shader when display adjustment is
  non-neutral, and the plain blit otherwise.

### Expected effect and verification

The measured OpenXR present was 0.51 ms with a 0.05 ms blit, so the expected
win is under 0.2 ms/frame plus fewer driver-query spikes. No new metrics were
captured because no device was attached. Morning verification should confirm
both eyes, the double-wide display transform, and the mono/menu path.

## Phase 4 report: overlay and 2D duplication (2026-10-20)

Status: implemented and compile-verified against the Quest preset. Device
verification is deferred to the morning with the rest of the night's work.

### Changes

- `BoxHadMovingWrites()` records whether the 2D pass added a moving dirty box.
  Moving boxes are the world sprites, dialogue, counters and erase operations
  whose projection can differ between the eyes; static boxes are the
  screen-space HUD.
- Eye 0 still scans and uploads the overlay.  Eye 1 skips that scan and the
  upload only when neither eye wrote a moving box.  Any moving content takes
  the original per-eye diff and upload path.
- Eye 1 also keeps eye 0's `Log` buffer, instead of clearing it, under the
  same static-overlay condition.  The static HUD pass redraws the same
  pixels; a dynamic overlay still gets the clear so old sprite positions
  cannot ghost.
- `GLPerf` now reports `ovlReuse`, the fraction of presents that reused the
  eye 0 overlay texture.
- The duplicate full-target clear was already removed by the double-wide
  clear's scissor rectangle; no new clear change was needed.

### Deliberate limit

The 2D drawing is interleaved with the object/incrust pass, not a separate
static pass, so eye 1 still redraws it.  Freezing those writers would require
a writer-level overlay flag across the game code; the dirty-box classification
gets the safe part of the win without that risk.  A frame with moving 2D
content keeps the original scan and upload.

The expected effect is roughly 0.2--0.5 ms on static-HUD frames, with no
change on dynamic frames.  Morning verification should include subtitles,
counter text, the medallion chain, world sprite disparity, and the F6 FPS
counter.  `ovlReuse` near 0.5 means the static path is active; a value near
zero means the scene kept moving boxes and the fallback is doing the work.
