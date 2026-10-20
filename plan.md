# Quest 72 FPS plan

Status: Phase 0 measured on device; Phase 1 not started. Branch
`quest-good-perf` at `04365c82`.

Target: 72 FPS (13.9 ms/frame) on Quest in stereo OpenXR mode, with no
resolution, MSAA, foveation, settings, gameplay, or timing changes.

## Executive summary

The largest remaining addressable cost is the stereo present chain, not the
horizon alone. Both eyes are composited into full-resolution eye FBOs, then
both are copied again into the double-wide OpenXR swapchain. The eye-1
direct-scene path was inactive in the measured scene (`direct=0.00` because
the overlay was non-empty), so the frame pays four full-resolution copies and
two MSAA resolves. Eye 0 costs 2--4 ms more than eye 1; the horizon accounts
for part of that gap, and the intermediate eye composites are the avoidable
part.

The second problem is GPU memory. The last telemetry run reported
`GPU=154.1/154.1 MiB (100% allocated)`. The engine keeps or has kept alive the
scene MSAA color/depth, `s_fbo`, two eye FBOs, the present FBO, the horizon
FBO, and the snapshot FBO. That is roughly 125 MB of render targets before the
texture array and atlas cache. At that point every new texture or render
target competes for the same budget.

The plan is therefore:

1. Measure the present and main-loop phases precisely.
2. Replace the per-eye scene copies with one double-wide MSAA scene target and
   one present blit.
3. Remove the dead and duplicate full-resolution surfaces.
4. Remove per-frame OpenXR validation/state queries.
5. Remove duplicate overlay, clear, flush, and horizon work.
6. If more headroom is needed, render directly into the swapchain halves.

The intent is to keep the existing per-eye fallback path until the new path is
visually verified on Iceland, the medallion, the whiskers, menus, and the
console overlay.

## Current frame budget

Phase 0 in-game sample (55 s, mixed light and heavy scenes; medians):

| Metric | Value |
|---|---|
| `StereoPerf eye0` | 14 ms (mean 14.7, p90 19, max 20) |
| `StereoPerf eye1` | 12 ms (mean 12.5, p90 17, max 18) |
| `StereoPerf frame` | 26 ms (mean 27.2, p90 37, max 37) |
| `GrillePerf horizon` | 3 ms median, 3.8 ms mean, 6.9 ms p90 |
| `GrillePerf terrain` | 3 ms median, 5 ms max |
| `GrillePerf skysea` | 0 ms |
| `GLPerf present` | 2.35 ms per present, about 4.7 ms/frame |
| `GLPerf flush` | 2.99 ms per present, about 6.0 ms/frame |
| `GLPerf flushes` | 82 per 300 presents (range 30--110) |
| `GLPerf ovlEmpty` | 0.00 in every sample |
| `GLPerf direct` | 0.00 in every sample |
| `OpenXRPerf total` | 0.56 ms per present; wait 0.00, blit 0.06 |
| `VrApi FPS` | 41 median (p90 67.6) |
| `VrApi CPU&GPU` | 29.8 ms median |
| `VrApi App` | 4.7 ms median |
| Surface estimate | 124.1 MiB after snapshot and horizon allocate |

The eye-0 gap is about 2 ms median, not horizon alone. The larger issue is
that `ovlEmpty=0` and `direct=0`: both eyes take the full scene+overlay
composite, so the eye-1 direct path is not saving a copy today. Present is
2.35 ms per present and flush is 2.99 ms per present; because present includes
any pending flush at its start, their sum is an upper bound of roughly 10.7
ms/frame before horizon and terrain. `GLPerf flush` includes `glDrawArrays`
and can block on GPU backpressure; it is not pure CPU submission time.

The current Android path is 2x MSAA at 1680x1760 per eye
(`SetAntiAlias 1 -> 2`, samples=2, resolve-in-tile). The plan preserves that.

## Why the present chain comes first

`GL_PresentIndexed` currently does this for a stereo pair:

1. Eye 0 renders the scene into the shared scene FBO.
2. Eye 0 composites the scene plus the 2D overlay into `s_stereoEyeFBO[0]`, a
   full-resolution copy.
3. Eye 1 renders into the same scene FBO and composites again into
   `s_stereoEyeFBO[1]`; the direct path is inactive because the measured
   overlay is non-empty.
4. `OpenXR_PresentMono` creates a swapchain FBO and blits both eye FBOs into
   the double-wide swapchain.

The horizon is still a 3--6 ms/frame cost, but it is already split per eye and
has no zero-cost fix. The two intermediate eye composites and the extra MSAA
resolve are the redundancy. Removing them is the highest-confidence large win.

The old conclusion that the frame is GPU-bound still holds. What changes is
the attribution: the GPU cost is not only the two scene passes and the horizon;
it includes four full-resolution copies and two resolves.

## Phase 0: measure before changing anything

Add per-phase timing and keep it low-volume:

- Simulation wall time before the stereo loop.
- `OpenXR_BeginFrame` / `xrWaitFrame` time.
- Eye 0 scene time, split into horizon, terrain, object pass, flush, present.
- Eye 1 scene time, split the same way.
- `OpenXR_PresentMono`: acquire, wait, swapchain FBO setup, eye 0 blit, eye 1
  blit, release/end-frame.
- GPU timer queries around horizon, terrain, clear/resolve, and present if
  `EXT_disjoint_timer_query` is available. Do not add a release `glFinish`.
- Surface accounting: scene color/depth, `s_fbo`, eye FBOs, present, horizon,
  snapshot, log index, texture array, atlas cache.
- Maximum `s_arrCount` and atlas cache occupancy over the run.
- Flush reason counters. The current `rsn` values are all zero, so there is no
  attribution today.

Also verify, rather than assume:

- `FpsLimit` is 0 or at least above 72.
- Anti-aliasing is the intended 2x MSAA path.
- The gameplay overlay is actually empty. The observed `overlayEmpty=0` was
  from the startup/menu window, not a confirmed gameplay frame.
- The current eye-1 direct path is actually active during gameplay.

Use the same save, camera mode, and scene for every before/after run. Record
the median, not only the average, and record `Stale2/5/10/max`.

**Implemented in the working tree:** `Perftrace` records `sim`, `openxr`,
`eye0`, `eye1`, `scene`, and `present`; `OpenXRPerf` logs frame-begin,
present-begin, acquire, wait, blit, release, end, and total once per 300
presents; `GLPerf` adds `ovlEmpty` and `direct`; surface totals are logged at
init and when snapshot/horizon targets are allocated; and `FpsLimit` is logged
at startup. GPU timer queries are not wired in this pass; `GrillePerf` and the
VrApi runtime line already provide the GPU-side totals, and no release
`glFinish` was added.

## Phase 1: one double-wide MSAA scene target and one present blit

This is the core change.

- Allocate the stereo scene MSAA color texture at `2 * eyeWidth` by
  `eyeHeight`, with one shared depth renderbuffer at the same size and the
  existing single-sample resolve view.
- Keep `s_texWidth` and `s_texHeight` at the per-eye content size so the
  projection and all existing shader math are unchanged.
- Set the scene FBO width to twice the eye width, but render eye 0 with
  `glViewport(0, 0, eyeWidth, eyeHeight)` and eye 1 with
  `glViewport(eyeWidth, 0, eyeWidth, eyeHeight)`.
- Add a small scene-viewport helper and use it in `GL_ClearFBO`, the first
  `FlushActiveBatch` of each eye, and every direct draw that currently assumes
  a full-target viewport.
- Clear with a scissor rectangle for the current eye half. Clear both halves
  once before eye 0 if that is cheaper and the right half is still clean when
  eye 1 starts.
- `GL_PresentIndexed` no longer composites the scene. For each eye it binds the
  double-wide MSAA FBO, sets that eye's viewport, and draws only the 2D overlay
  if the overlay has content. The scene is already underneath it.
- At eye 1's present, blit the one resolved double-wide scene into the
  double-wide swapchain with a single `glBlitFramebuffer`. The swapchain is
  already `2 * s_swapchainWidth` wide, so this is an exact 1:1 blit.
- Use `GL_NEAREST` for the 1:1 blit. Keep `GL_LINEAR` only when scaling.
- Remove both eye composites and the eye FBOs from the stereo fast path.
- Keep the current eye FBO + scene-composite path as a fallback for:
  - FBO incomplete or `GL_MAX_TEXTURE_SIZE`/renderbuffer size exceeded.
  - Non-neutral image adjustment.
  - FXAA or SMAA post-process.
  - Any driver rejection of the double-wide target.
- Keep the existing menu/mono path unchanged.

Expected effect: one full-screen copy instead of four, one resolve instead of
two, both intermediate eye composites gone, and about 22 MiB less
render-target accounting once the stereo snapshot and horizon surfaces are
absent. The target is 3--5 ms/frame.

Primary files: `LIB386/GL/RENDER_GL.CPP`, `LIB386/SYSTEM/OPENXR.CPP`,
`LIB386/H/GL/RENDER_GL.H`.

**Implemented in the working tree (2026-10-20).** The existing `s_fbo` and
`s_msaaFbo` are physically double-wide while `s_fboWidth`/`s_fboHeight` stay
the per-eye content size. `GL_SetSceneViewport()` selects the eye half for
batch flushes, clears, direct draws, snapshot/horizon restores, and the
present overlay; `GL_ClearFBO` scissors the current half. `GL_PresentIndexed`
and `GL_Present` bind the scene target, draw only the current eye's HUD, and
on eye 1 resolve once and mark the pair ready. `OpenXR_PresentMono` takes the
single resolved pair and performs one blit into the double-wide swapchain.

The fast path is gated on an active OpenXR session, stereo enabled, MSAA 2x
or 4x, and neutral brightness/contrast/saturation. Non-neutral image
adjustment, FXAA/SMAA, an incomplete double-wide FBO, or
`LBA2_NO_STEREO_DW=1` keep the per-eye FBO plus scene-composite fallback.
Eye FBOs are not allocated while the double-wide path is active; they are
created when the fallback is selected. `GL_CaptureHorizon` returns early in
stereo, so the horizon surface is not allocated and the dead capture is not
paid; eye 1 still draws its own horizon.

Status: host `lba2cc` builds; Quest native build and install succeeded as
`0.13.0-dev` on 2026-10-20 14:00:55. The user reports better performance and
no visual issues. The latest captured run measured medians of 12 ms (eye 0),
10 ms (eye 1), and 21 ms (frame), versus 14/12/26 ms in Phase 0. The captured
surface log still reported the per-eye totals (`124.1 MiB` after snapshot and
horizon), so Phase 2 rechecks the gate while removing the dead surfaces. The
renderer logs `[GL] Stereo scene target: double-wide` and a
`surfaces(stereo-double)` line when the double-wide path is selected.

## Phase 2: remove dead and duplicate full-resolution surfaces

- `GL_CaptureHorizon` is already disabled in stereo (Phase 1). Free any horizon
  and snapshot surfaces allocated before the session entered stereo, and
  allocate them lazily only when a fallback or mono frame needs them. The
  eye-1 image reuse stays gone: near neighbour-cube terrain has real stereo
  disparity and produced Iceland seams.
- Allocate `s_presentFBO` only for mono/menu frames. It is not used by the
  stereo eye path.
- Free the stereo snapshot and `s_fbo` surfaces when the double-wide path is
  active; allocate them lazily if a fallback frame needs them.
- Do not allocate the old eye FBOs in the double-wide path. They remain only
  for fallback (already true after Phase 1).
- Re-read the app GPU telemetry after the change. Target below 120 MB, ideally
  below 100 MB.

Expected effect: fewer driver evictions and fewer memory stalls. This may not
show up as a clean CPU-millisecond win, but it removes a hard ceiling.

## Phase 3: clean up the OpenXR present hot path

Phase 0 measured the whole OpenXR present at 0.56 ms per present (`wait` and
`acquire` 0.00, blit 0.06, end 0.43), so this is a small cleanup rather than
a top-tier win. Do it after the present-target and memory work unless a
capture shows otherwise.

Do not reintroduce the failed persistent swapchain attachment. The log records
that the Meta runtime did not tolerate a persistent FBO attachment across
`xrAcquireSwapchainImage`/`xrReleaseSwapchainImage`; the fresh-FBO-per-present
fix is intentional.

Remove the per-frame validation and state queries instead:

- Drop `glGetError` from release present paths. Keep it in a debug-only path.
- Validate the swapchain FBO once at creation or on a debug flag, not every
  frame.
- Avoid `glGetIntegerv(GL_FRAMEBUFFER_BINDING)` and
  `glGetIntegerv(GL_TEXTURE_BINDING_2D)` on every present. Save and restore the
  known state once around the present, or use a dedicated present target and
  restore explicitly.
- Keep the fresh FBO object per present if the runtime still requires it, but
  do not pay for per-frame error/status queries.
- Use `GL_NEAREST` for exact 1:1 stereo blits.

Expected effect: under 0.2 ms/frame on the measured path, plus fewer `Stale`
spikes if the validation calls interact badly with a loaded driver. This is
lower risk than the render-target change but also much lower payoff.

## Phase 4: overlay, clear, and 2D duplication

Phase 0 measured `ovlEmpty=0.00` in every sample, so the direct path never
activated. First identify which 2D content keeps `Log` non-empty, then add
dirty tracking around that writer rather than assuming gameplay is overlay-free.

- Maintain an overlay-dirty flag or dirty bounding box in the 2D draw path. If
  nothing wrote to `Log`, set `overlayEmpty=1` without scanning the full
  buffer.
- Scan the overlay once per frame, not once per eye.
- Skip the eye-1 `Log` memset and static 2D pass when the overlay is identical.
- Avoid the duplicate full-target clear when one clear covers both eye halves.
- Keep a fallback scan when the dirty tracking is uncertain.

Expected effect: 0.5--1 ms/frame and less CPU variance.

## Phase 5: horizon reuse in vertex space, not image space

Do not restore item 14's capture copy. The correct reuse is geometric:

- Cache eye 0's horizon vertices in camera space.
- For eye 1, apply the constant lateral eye translation in camera space,
  then project and clip normally.
- Re-cull per eye so a vertex that is just outside eye 0 is still considered
  for eye 1.
- Keep the eye-0 and eye-1 depth and sort behavior independent.

This preserves real stereo disparity for near neighbour-cube terrain and
avoids the image-space seams that killed the capture reuse.

Fallback: split the horizon into a far backdrop rendered once and near cubes
rendered per eye. That is less exact but still avoids the image copy.

Expected effect: 1.5--3 ms/frame in scenes where horizon is 4--6 ms/frame.

## Phase 6: submission-side cleanup

- Skip the redundant `glBindFramebuffer` on every flush. Bind only when the
  scene target changes; the first-flush path already handles the initial bind.
- Test the ring-buffer path on Adreno (`LBA2_RING=1`) and keep it only if it is
  visually stable.
- Use a VAO or avoid re-specifying unchanged attribute state.
- Add flush-reason counters and attack the top state changes rather than
  guessing. The current reason fields are all zero.
- If terrain still dominates flushes, extend the array path to sub-rect
  terrain pages or group opaque terrain by page. Do this only after the
  counters show it is the cause.

Expected effect: 1--2 ms/frame.

## Phase 7: direct-to-swapchain MSAA, only if needed

Once the double-wide path is stable, try attaching the acquired swapchain
texture directly to the MSAA FBO with
`glFramebufferTexture2DMultisampleEXT`, rendering eye 0 and eye 1 into the two
halves in place. That removes the scene color texture and the final blit.

This is the highest-risk step because the swapchain texture is runtime-owned
and the previous persistent-attachment experiment produced black frames. Keep
it behind a flag and keep the double-wide blit path as the fallback.

Expected effect: another 2--3 ms/frame and a large memory reduction. Do not
make it the first change.

## Phase 8: hitch and streaming audit

Average frame time is not sufficient if the runtime drops rate on spikes.
Track `Stale2/5/10/max` and investigate the streaming path separately:

- The atlas content version is global. An animated water or plasma page can
  force every active array layer and cached atlas page to re-upload. Move to a
  per-page dirty/version scheme.
- Check whether cube streaming or texture uploads align with the `Stale`
  spikes.
- Pre-warm or batch uploads only if the measurements show they cause hitches.

## Validation

Visual checks:

- Iceland horizon and terrain seams.
- Medallion chain and whiskers occlusion, including the body-line path.
- Stereo disparity near and far.
- Menus, subtitles, console overlay, and any eye-specific 2D UI.
- No black frame, no eye swapped, no half-frame stale content.

Numeric checks:

- `StereoPerf` eye 0 and eye 1 each at or below about 6.5 ms.
- `StereoPerf frame` at or below about 10 ms in the reference scene.
- VrApi 72 FPS, `CPU&GPU` at or below 13.9 ms, low `Stale` counts.
- GPU memory below 120 MB, ideally below 100 MB.
- No regression in the measured heavier scene; report median and max, not
  only the best sample.

Build and install:

```sh
bash scripts/dev/build-and-install-quest.sh
```

Use `--log-level info` for `StereoPerf`, `GrillePerf`, and `GLPerf`; use the
VrApi line for the runtime frame budget. Run targeted host tests for any pure
logic extracted during the change; do not run the full Docker suite unless the
owner asks for it.

## Non-goals

- No resolution-ladder change.
- No MSAA sample-count change.
- No foveation change.
- No gameplay, timing, or feel change.
- No restoration of the eye-1 horizon image capture.
- No speculative abstraction around the render targets; keep the fallback
  path explicit and removable.
