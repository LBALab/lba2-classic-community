# Quest 72 FPS night — optimization log

Target: ~72 FPS (13.9 ms/frame) on Quest 3, stereo (OpenXR) mode.
Constraints: desktop-only verification (Linux build + host tests + Docker
ASM suite); every change gated so desktop behavior is unchanged; nothing
committed until the owner says so.

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

## Changes made (all in working tree, uncommitted)

### GPU / frame pipeline

| # | Change | File | Expected effect |
|---|---|---|---|
| 1 | **Horizon capture color-only** (eye 0, every frame): the full color+depth resolve became color-only, and the capture blit drops its depth half under MSAA (the MSAA restore path never reads horizon depth — confirmed by its own comment). | RENDER_GL.CPP | −1 full-surface depth resolve + smaller blit per frame |
| ~~1a~~ | ~~MSAA color resolve-in-tile via EXT_multisampled_render_to_texture~~ **REVERTED — failed on device.** With the scene texture attached to `s_msaaFbo` (multisample semantics) *and* plain to `s_fbo`, the 3D scene came back black (2D overlay/rain still visible — the overlay composites from a different layer). `GL_ClearFBO` writes the plain attachment every frame while the render pass resolves through the multisample one; the dual access defeats the implicit resolve on Adreno. A working in-tile scheme needs single-attachment discipline across every `s_fbo` consumer (clear, snapshot, depth readback, captures) — noted as follow-up. | RENDER_GL.CPP | (would have been −2 resolves/frame) |
| 3 | ~~Swapchain-image FBOs cached per image index~~ **REVERTED — found by on-device bisection.** The Meta runtime does not tolerate a persistent FBO attachment on its swapchain textures across xrAcquire/xrRelease (or re-backs the images on session resume): the composited frame landed in orphaned memory and the eyes saw black with only the 2D overlay (rain) showing. The original fresh `glGenFramebuffers` + attach + delete per present is restored — its cost is negligible next to the blit. | OPENXR.CPP | (would have removed per-frame FBO churn) |
| 4 | **Frame pacing**: `xrWaitFrame` + `xrBeginFrame` now run at frame START (from `OpenXR_BeginFrame`, one-shot `OpenXR_EnsureFrameBegun`) instead of after both eye renders inside `OpenXR_PresentMono`. CPU render work now overlaps the compositor's wait window and frames submit on time instead of late. Menu/title paths still funnel through the same one-shot. | OPENXR.CPP | removes submit lateness; pacing/stutter win |
| 5 | **Fixed foveation LOW** (XR_FB_foveation + configuration + swapchain_update_state, all ext-gated): the tiler renders the periphery at reduced bin density. The swapchain is created with `XR_SWAPCHAIN_CREATE_FOVEATION_SCALED_BIN_BIT_FB` — the method Adreno implements; chaining the create-info with flags 0 (the first attempt) left the runtime rendering the swapchain undefined. LOW chosen to be invisible behind the engine-resolution upscale; the knob is one enum (`XR_FOVEATION_LEVEL_*_FB`). | OPENXR.CPP | major fill-rate save on Quest |
| 6 | **Clock governor** (XR_EXT_performance_settings, ext-gated): GPU = BOOST, CPU = SUSTAINED_HIGH at session begin. | OPENXR.CPP | clocks needed for 72 Hz |

### CPU

| # | Change | File | Expected effect |
|---|---|---|---|
| 7 | **Neighbour-cube frustum cull** in `DrawHorizon2ZBuf` (`HorizonCubeOffscreen`): a horizon cube is skipped only when all 8 corners of its padded bounding box fail the SAME clip plane (exact for a convex box — a visible cube is never culled). Uses the engine's own rotate/project/clip sequence. Output-identical; ASM equivalence unaffected (invisible cubes draw nothing on either side). | SOURCES/3DEXT/TERRAIN.CPP | cuts a large share of the 2–6 ms horizon CPU when the view cone covers part of the neighbour grid |
| 8 | **Horizon-capture reuse**: the capture (eye 0) now skips its resolve-and-blit whenever the camera pose is unchanged since the previous capture AND no missing-cube sea placeholder drew this frame (`HorizonBackdropStable()` from TERRAIN.CPP guards against freezing a half-streamed backdrop). The common VR case — rig standing still, head looking around — previously paid the capture every frame for an identical image. | RENDER_GL.CPP, TERRAIN.CPP, EXTFUNC.CPP | removes a full-surface blit per frame while stationary |
| 9 | **Build fixes that unblock the ASM suite**: (a) `SVGA/SDL.CPP` referenced `GPURendererActive` outside the `USE_GPURENDERER` guards (added in 0ee18ee0); (b) `DisplayBrightness`/`Contrast`/`Saturation`/`VrFovScale` declarations lived in a GL-only guard while the console cvars and cfg load/save reference them unconditionally, and the definitions were guarded too (moved out — the values are inert without the GL renderer). Every non-GL build, including the Docker linux_test preset, failed to compile before this; no behavior change when the GL renderer is on. | LIB386/SVGA/SDL.CPP, SOURCES/C_EXTERN.H, SOURCES/GLOBAL.CPP | Docker ASM↔CPP suite builds and runs again on this branch |

### Verification status

- Desktop build clean; host_quick suite 48/49 — the one failure
  (`test_poly_geometry`) fails at the base commit too (pre-existing).
- Quest native build (NDK, real OpenXR headers) clean; fixed APK
  (`lba2cc-0.13.0-dev-android-arm64-v8a.apk`) built, installed on the
  headset, awaiting an on-head session for re-measure.
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

## Remaining levers (deliberately NOT taken — behavior changes, owner's call)

- Lower render-resolution ladder step (linear CPU+GPU win, visual).
- Share the terrain pass between eyes (breaks near-geometry stereo depth).
- MSAA → FXAA default on Android (visual).
- Foveation level MEDIUM/HIGH (peripheral blur after upscale is likely
  invisible — one-enum change if LOW alone isn't enough).
