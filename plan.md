# Quest 3 72 FPS optimization plan

> This plan supersedes the previous `plan.md` at commit `48566556`. That plan's phases were implemented or superseded in the preceding sessions; its full text remains in git history (`git show 48566556:plan.md`). The phases below are the next implementation sequence.

## Phase 3 status

- Worktree: `/home/simsas/.local/share/opencode/worktree/2081ef/quest-phase3-multiview`
- Capability probe on the Quest: `GL_OVR_multiview2` extension found, `glFramebufferTextureMultiviewOVR` resolved, and a 1680×1760 two-layer color/depth FBO completed successfully.
- Implemented: the layered-FBO capability probe, single-pass stereo through the existing double-wide target, per-eye shader uniforms (`uEye`), per-eye depth for the 6dof Z shift, per-eye scissor, a full double-wide clear in the eye-0 pass, an eye-0 batch drain before eye 1 starts, and union culling margins for objects, decors and horizon cubes.
- The single-pass path draws both eye halves from the eye-0 `AffScene`. Eye 1 runs only its overlay/present stage; the screen-space overlay was already submitted into both halves by the eye-0 pass. Eye 1 skips the terrain/decor walk, object sort insertions and 3D GL calls, while the object walk keeps its state updates. The two-pass path remains the fallback/reference, including true 6dof first-person where the per-eye orientation is not a pure translation.
- The menu and an in-game Citadel rain scene render correctly in both eyes on device.
- Capture at 2% battery: the installed build with the eye-1 terrain/decor skip measured `eye0=9-10 ms`, `eye1=0-1 ms`, `frame=10 ms`; the in-game counter showed 75 FPS. An earlier 60 s capture at 4% battery / 545 MHz measured `frame=16 ms` median (eye0 12, eye1 5) and 59 FPS median before that skip.
- Phase 1/2 comparison: 48 FPS at 19% battery on the same class of scene.

Status: Phase 1 and Phase 2 are committed on `quest-good-perf`. Phase 3 single-pass stereo is implemented and verified on device; the two-pass path remains the fallback.

## Current state

- Branch: `quest-good-perf` (main checkout); Phase 3 worktree branch `quest-phase3-multiview`.
- Last Phase 1/2 commit: `1badf45f perf(gl): add resolved terrain CLUT and quad-packed array atlas`.
- Earlier commits: `48566556` (redundant array binds), `7329683e` (release mono surfaces while stereo is active), `d3571b39` (baked CLUT + empty overlay discard + band scissor).
- The earlier partial quad experiment was superseded by the completed Phase 2 quad-packed array.
- Phase 3 code: `LIB386/GL/RENDER_GL.CPP`, `LIB386/H/GL/RENDER_GL.H`, `LIB386/OBJECT/AFF_OBJ.CPP`, `SOURCES/3DEXT/DECORS.CPP`, `SOURCES/3DEXT/TERRAIN.CPP`, `SOURCES/EXTFUNC.CPP`, `SOURCES/PERSO.CPP`, `SOURCES/SORT.CPP`.
- Fixed settings: 1.5x supersampling, MSAA×2, 1680×1760 eye swapchain, 5040×2640 double-wide scene target. Do not change them.

## Measured evidence

### Same-scene A/B (heavy autosave, 36% battery, 545 MHz)

| Metric | Baseline | Current (committed) | Delta |
|---|---:|---:|---:|
| FPS app median | 45 | 47 | +4% |
| eye0 / eye1 | 14 / 8 ms | 13 / 8 ms | −1 / 0 |
| frame | 23 ms | 21 ms | −2 ms |
| present | 1.36 ms | 1.41 ms | +0.05 |
| flush | 1.225 ms | 1.205 ms | −0.02 |
| triangles | 2180 | 2091 | slightly fewer |

This shows the committed shader work helps, but the 4% is not enough, and this particular view is not GPU-bound (GPU about 50% busy).

### Good and bad on-headset runs (same Citadel night-rain save)

| Metric | Good | Bad | Why it matters |
|---|---:|---:|---|
| FPS app | 69.5 | 58.0 | target is 72 |
| eye0 / eye1 | 11 / 3 ms | 12 / 5.5 ms | eye1 grows 83% |
| frame | 14.0 ms | 17.5 ms | 3.5 ms over target |
| present | 1.65 ms | 1.62 ms | stable, not the regression |
| flush | 0.51 ms | 1.05 ms | doubles at 545 MHz |
| triangles | 1594 | 1391 | bad run is lighter |
| GPU clock | 599/640 | 545 | 9% lower |
| GPU busy | 70.5% | 61% | lower, so not a heavier workload |
| Stale | 4 | 15 | compositor repeats frames |

The bad-run regression is GPU/driver side, not app CPU. The app CPU was effectively identical (9.36 vs 9.63 ms). The biggest remaining per-frame costs are:

- Textured fragment work: the bilinear CLUT path still does 4 atlas + 4 CLUT fetches per fragment. The terrain alone is about 6604 polygons and takes about 3 ms.
- The second eye: `eye1` is 3 to 8 ms and repeats scene traversal and draw submission.
- The 1.5x full-screen 2D overlay: rain makes the content dense, so the band scissor cannot help and the present is about 1.4 ms per eye.
- Driver/flush: 23 to 28 draw batches per frame, about 1.2 ms per present.
- Heavy-scene CPU/compositor: terrain about 3 ms, object pass about 1.7 ms, app CPU about 9.4 ms, Stale up to 26 at 36% battery.
- Startup surfaces: 265.1 MiB in the bad session, including 50.8 MiB snapshot and 11.3 MiB present target that stereo never reads. The 62 MiB fix is already committed.

Power matters. The good run was 100% / full and boosted to 599/640 MHz. The bad run was 97% / discharging at 545 MHz. The current heavy test was 36% / charging and stayed at 545 MHz. The test gate for all later phases must include a 100% charge and the same save.

## Order of implementation

Ordered by the best possible performance gain, not by implementation effort.

| Order | Phase | Maximum plausible gain | Evidence | Risk |
|---:|---|---|---|---|
| 1 | One-fetch resolved CLUT atlas for textured terrain | 2 to 5 ms GPU | 8 fetches per textured fragment; terrain is the largest fill; good/bad are GPU-bound | High |
| 2 | Quad-packed atlas for the array path | 1 to 2 ms GPU | array path still does 4 atlas + 4 CLUT | Medium |
| 3 | Single-pass stereo / multiview | 3 to 5 ms CPU and driver | eye1 5.5 to 8 ms; duplicate scene traversal and flush | Very high |
| 4 | Rain/HUD overlay fill elimination | 1 to 2 ms GPU and CPU | full-screen 1.5x overlay; rain keeps bbox full; present 1.4 ms per eye | Medium |
| 5 | Draw-call, flush and present-state reduction | 0.5 to 1.5 ms | 23 to 28 flushes; flush 1.2 ms; present invalidates batch state | Medium |
| 6 | Terrain and object CPU reduction | 1 to 2 ms CPU | terrain 3 ms, object pass 1.7 ms, app CPU 9.4 ms | Medium |
| 7 | Startup surfaces, warm-up and power | 0 to about 12 FPS external | 121 MiB extra GPU memory; 545 vs 599/640 MHz; 36% vs 100% battery | Low code risk, low control |
| 8 | Compositor and system behaviour | Not app code | CPU&GPU 31.7 ms, Stale 26 | Out of scope |

The first two phases attack the measured GPU-bound 545 MHz case. Phase 3 attacks the heavy-scene CPU/driver case. Phase 7 is not a substitute for code work, but every test must control it.

## Phase 1 — One-fetch resolved CLUT atlas for the terrain

### Goal

Replace the bilinear terrain path's four atlas fetches plus four CLUT fetches with one texture fetch per fragment, while keeping the output pixel-identical.

### Why first

The terrain is the largest textured fill. The current fragment path samples 4 atlas texels and resolves each through the baked CLUT. The good and bad runs are GPU-bound at 545 to 640 MHz, and the bad run's eye0 and eye1 both grow when the GPU clock drops. This is the largest single GPU-work reduction available.

### Design

1. Build a tiled 256×256 palette-index page for each terrain RepMask used by the GPU terrain path. Reuse the tiling rule already in `GL_ArraySlotFor` so the +1 bilinear neighbours wrap exactly like the current `bilinearCLUT` code.
2. For each CLUT row 0 to 255, resolve the tiled page through the palette into one RGBA slice. Store the slices in a `GL_TEXTURE_3D` of 256×256×256 RGBA (64 MiB) or an equivalent texture array.
3. The terrain fragment path samples `texture(uResolvedCLUT, vec3(uv, row + 0.5))` with linear filtering in U and V and the row at its texel centre. That returns the exact bilinear mix of the four palette-resolved colours in one fetch.
4. Keep the existing 8-fetch path as a fallback for:
   - rows or pages not present in the table;
   - unsupported GLES configurations;
   - the array path's multi-page textures.
5. Rebuild the table only when `CLUTGeneration`, the palette generation, the atlas page or the terrain mask changes. Keep a generation counter and a surface-budget log.
6. Batch terrain draws by page/mask so one resolved table is current. If the number of masks makes 64 MiB per mask unreasonable, cache the masks actually used in the current cube and evict the rest.

### Files

- `LIB386/GL/RENDER_GL.CPP`: CLUT upload, shader sources, terrain program, texture units, fallback.
- `SOURCES/3DEXT/TERRAIN.CPP`: terrain mask/page selection, per-mask draw batches, VBO material row if needed.
- `SOURCES/EXTFUNC.CPP`: terrain draw ordering if batching changes.
- `LIB386/H/GL/RENDER_GL.H` if a new renderer state/query is needed.

### Risks

- The RepMask edge wrap is the main correctness risk. The tiled page must match the current `mod(widthMask + 1)` rule.
- The table is 64 MiB per mask and 16.7M palette lookups to build. A scene-load hitch is possible; measure it and consider a background/cached build.
- `GL_TEXTURE_3D` is GLES3 class. The terrain program currently uses the GLSL-100 source; it needs a GLSL-3 variant like the array path.
- Row addressing must be exact: the existing `gouraudRow` is an integer; sample the row centre, not between rows.
- Palette/CLUT changes can occur between cubes; stale table slices must never survive a generation change.

### Verification

- Desktop fixed-dt screenshots on cube 3, cube 5, cube 100 and the rain save; compare against the committed binary. Any pixel difference is a failure except the known nondeterministic in-world sign.
- Check the terrain edges, animated water tiles, sea/sky mode-5 path and fog rows.
- Quest build/install; the user loads the same save at 100% battery; capture 120 s and compare FPS, eye0, eye1, frame, present, flush, GPU clock and Stale.
- The phase is accepted only if the screenshot equivalence passes and the GPU-bound bad-run condition improves without a visual regression.

## Phase 2 — Quad-packed atlas for the array path

### Goal

Reduce the array path's bilinear sampling from four atlas fetches plus four CLUT fetches to one quad fetch plus four CLUT fetches.

### Why second

The array path covers full-page bodies, decors, sky and sea. It is the second-largest textured fill after the terrain and has a lower implementation risk than the 3D table because the array layers are already tiled to the 256 wrap.

### Design

1. Add a second texture array, `GL_RGBA8`, 256×256×`GL_ARR_LAYERS` (64 MiB on Android, 128 MiB on desktop).
2. For every stale array layer, after building the existing tiled R8 layer, build a quad layer where each texel stores `(x, y)`, `(x+1, y)`, `(x, y+1)`, `(x+1, y+1)` in R, G, B, A with wrap at 256.
3. Upload the quad layer to unit 4 and bind it alongside the R8 array on unit 3.
4. In `arraySample`, for the bilinear path sample the quad array once at the center texel, unpack the four indices, then keep the existing four CLUT lookups and mix.
5. Keep the nearest path on the existing R8 array. Keep the 8-fetch path as a fallback if the quad allocation or sampler is unavailable.
6. Cache the array and quad bindings. If any upload happens, clear the binding caches before re-binding unit 3 and unit 4.

### Files

- `LIB386/GL/RENDER_GL.CPP`: `s_arrayInject`, `GL_ArrayEnsureTexture`, `GL_ArrayBind`, `s_clut3Program` uniforms, texture-unit bindings.

### Risks

- 64 MiB extra texture memory. The current app uses about 280 MiB RSS; the Quest has headroom, but log the allocation.
- The upload cost for stale layers is one 256 KiB RGBA upload each. The array first bind already does R8 uploads; measure the startup.
- The binding cache must be invalidated after uploads, or unit 3 can be left with the quad texture bound and the shader will read RGBA as R8.
- The array layer must already be tiled to 256; verify against the array tiling path.

### Verification

- Desktop fixed-dt screenshots on the same cube set; the array path covers bodies and decors.
- Check chroma-key edges and tiled subrects.
- Quest capture at the same save; compare eye0/eye1 and GPU busy.
- The phase is accepted only if the image is pixel-identical and the GPU-bound condition improves.

## Phase 3 — Single-pass stereo / multiview

### Goal

Render the 3D scene once for both eyes instead of traversing and submitting it twice, while preserving the exact stereo pair.

### Why third

The heavy-scene eye1 is 8 ms and the good/bad eye1 grows from 3 to 5.5 ms. The two eyes repeat scene traversal, culling, batching and draw submission. The GPU fill remains, but the CPU and driver work is duplicated, and the heavy scene is CPU/compositor limited.

### Design

1. Detect `GL_OVR_multiview` or `GL_EXT_multiview`; add a layered scene FBO with the two eye layers (or the double-wide equivalent).
2. Add `gl_ViewID_OVR` to the stereo vertex shaders and supply the per-eye camera/projection matrix as a uniform array or `gl_ViewIndex`.
3. Change the `PERSO.CPP` stereo loop to:
   - update/track both eye poses;
   - run the scene draw once with both views;
   - keep the 2D overlay per eye, because rain/HUD content can differ;
   - keep the present per eye.
4. Preserve the existing per-eye culling result. If an object is visible in only one eye, the single-pass frustum must include it; otherwise use a conservative union frustum.
5. Keep the current two-pass path as a fallback and as the pixel-equivalence reference.

### Files

- `SOURCES/PERSO.CPP`: `PERSO_RenderStereo`, eye loop, `StereoCamShift`, `StereoProjShift`, `AffScene` calls.
- `LIB386/GL/RENDER_GL.CPP`: scene FBO, shaders, `GL_SetStereoEye`, present.
- `LIB386/SYSTEM/OPENXR.CPP`: swapchain/present.
- `LIB386/H/GL/RENDER_GL.H`: capability query.

### Risks

- Very high. The game runs logic per eye; the single pass must not duplicate or drop state changes.
- Exact stereo poses, culling and overlay ordering are hard to preserve.
- The overlay remains per eye, so the eye1 present cost does not disappear.
- Driver support and layered-FBO completeness need a runtime fallback.

### Verification

- A stereo screenshot comparison against the two-pass path with both eyes.
- Head-motion/pose test by the user; no eye swap, no culling pop, no missing overlay.
- Same-scene capture; report frame, eye0/eye1, present, flush, GPU busy.
- Accept only if the pair is visually identical and the frame time drops.

## Phase 4 — Rain and HUD overlay fill

### Goal

Cut the full-screen 1.5x overlay draw in the rain scene, where the content is dense and the band scissor cannot help.

### Why fourth

The overlay is drawn twice per frame at 1.5x. The rain makes the bounding box full, so the band scissor falls back to one full-screen draw. The `discard` already skips the palette fetch and blend for empty pixels, but the full-screen rasterization and index fetch remain.

### Options, in order of likely gain

1. **Rain as a GL line pass.** Record rain line endpoints and palette/fog colours in `RAIN.CPP`; draw them with GL lines at the 1.5x scene target; remove rain from the Log overlay. The HUD overlay then becomes small and the band scissor works. This has the largest fill win but changes the rain rasterization; require a user visual approval and a screenshot comparison.
2. **Coverage-tile scissor.** Keep the rain in the Log; add a compact dirty-tile grid (for example 16×16) to the 2D renderer and scissor the overlay to the non-empty tiles, merging neighbours and capping draw calls. This preserves rain pixels but adds draw calls and coverage bookkeeping.
3. **Static HUD plus dynamic rain split.** Keep one overlay texture for the static HUD and a second small texture/atlas for the rain; draw the static HUD with the band scissor and the rain at full screen. This helps only if the HUD dominates, which is not the current case.

### Files

- `SOURCES/RAIN.CPP`: line recording or coverage boxes.
- `LIB386/SVGA/DIRTYBOX.CPP`, `LIB386/H/SVGA/DIRTYBOX.H`: coverage/tile data if option 2.
- `LIB386/GL/RENDER_GL.CPP`: overlay draw, scissor, optional GL line program.
- `SOURCES/EXTFUNC.CPP`: draw order.

### Risks

- The rain is a visible gameplay/atmosphere element. The GL line path can change width, fog and clipping; the user must approve the comparison.
- Coverage tiling can add dozens of draw calls, making the driver cost worse.
- The 1.5x overlay is part of the current image quality; do not silently move it to 1x.

### Verification

- Rain-scene screenshot comparison; check line positions, width, fog and HUD.
- Same-scene capture; compare present and GPU busy.
- The user must explicitly approve any visible rain change.

## Phase 5 — Draw-call, flush and present-state reduction

### Goal

Reduce the 23 to 28 draw batches and the 1.2 ms per-present flush cost.

### Why fifth

The flush doubles from 0.51 to 1.05 ms when the GPU clock drops to 545. The bad run has fewer triangles and fewer batches than the good run, so this is a driver/clock effect per batch. Fewer batches and fewer state changes directly reduce it.

### Design

1. Keep the array binding cache and extend the same idea to the 2D atlas pages and terrain page bindings.
2. Move subrect terrain pages into the tiled array path where possible, so terrain stops forcing 2D page flushes.
3. Avoid `GL_InvalidateGLState()` at the end of an eye present when the next eye's first flush can reuse the batch state; restore the batch program/textures instead.
4. Merge batches that differ only in fog/alpha where the shader can select the mode from the material word.
5. Record the per-flush reason counts before and after; target the `t=...` texture-page and `n=...` reasons first.

### Files

- `LIB386/GL/RENDER_GL.CPP`: `GL_ArrayBind`, `GL_BindTexUnit`, `GL_FlushBatch`, `GL_InvalidateGLState`, `GL_PresentIndexed`.
- `SOURCES/3DEXT/TERRAIN.CPP`: terrain page/mask batching.

### Risks

- The batching state is shared across the scene, overlay and present; a stale texture or program binding can corrupt a frame.
- The terrain array path was deliberately left on the 2D path; moving it needs the snapshot/pool checks.
- Fewer batches can increase vertex buffer pressure or change draw order, which matters for blending.

### Verification

- Pixel-identical screenshots on all benchmark scenes.
- Same-scene capture; compare flush, flushes, present and FPS.
- Log the per-flush reason deltas.

## Phase 6 — Terrain and object CPU reduction

### Goal

Reduce the terrain and object CPU costs in the heavy scene: terrain about 3 ms, object pass about 1.7 ms, app CPU about 9.4 ms.

### Why sixth

The heavy scene is not GPU-bound (50% busy) and the app frame is CPU/compositor limited. The terrain VBO is already cached, but the software terrain record and object traversal still run per frame. This phase only helps the heavy/CPU-bound cases, not the GPU-bound bad run.

### Design

1. Audit `AffichageTerrainZBuf` and `GPU_TerrainRecordTile`: skip the software terrain record when the GPU terrain VBO is valid and no animated tile needs the software post-pass.
2. Cache the object draw list across eyes where the eye shift does not change visibility; use a conservative union for culling.
3. Profile the object pass `sortDraw` and the per-object matrix updates; remove redundant transforms.
4. If Phase 3 is not implemented, add an eye1 replay of the eye0 draw list for static geometry, with an eye1 camera uniform update.

### Files

- `SOURCES/3DEXT/TERRAIN.CPP`: `AffichageTerrainZBuf`, `GPU_TerrainRecordTile`, terrain cache.
- `SOURCES/OBJECT.CPP`: object pass, sort/draw.
- `SOURCES/EXTFUNC.CPP`: scene draw order.
- `SOURCES/PERSO.CPP`: eye loop if replay is used.

### Risks

- The software terrain record also feeds animated tiles and the snapshot/pool paths; skipping it can drop water/flower animation.
- Replaying a draw list across eyes can miss objects visible only in one eye.
- Object state can change between eyes; the replay must use the same frame state.

### Verification

- Check animated water, flowers, shadows and the horizon.
- Same-scene capture at the heavy autosave; compare app CPU, frame, eye0/eye1.
- Pixel comparison for static scenes.

## Phase 7 — Startup surfaces, warm-up and power

### Goal

Remove the remaining startup/session memory and establish a valid test condition for the 100% / boosted state.

### Why seventh

The bad session held 121 MiB more GPU memory than the good session. The 62 MiB snapshot/present fix is committed. The remaining gap is about 59 MiB of startup textures/driver state. This is a memory/session effect, not a per-frame shader win.

The good run was 69.5 FPS at 100% / full and 599/640 MHz. The bad run was 58 FPS at 97% / discharging and 545 MHz. The current heavy test was 47 FPS at 36% / charging and 545 MHz. Battery and DVFS can outweigh a phase's code gain.

### Design

1. Audit the remaining `surfaces(snapshot)`/`surfaces(init)` allocations and texture caches; prewarm or release at scene load.
2. Avoid creating the single/double/1.5x FBO chain during startup; create the final stereo target directly where the capability is known.
3. Add a prewarm step for the scene atlas/array/CLUT uploads at load, so the first minutes do not pay lazy upload costs.
4. Record `trans_stat`, battery status and GPU clock at the start and end of every capture.
5. Use 100% / full charge and the same save for all target measurements.

### Files

- `LIB386/GL/RENDER_GL.CPP`: FBO creation, surface budget, atlas/array prewarm.
- `LIB386/SYSTEM/OPENXR.CPP`: swapchain/session lifetime.
- `SOURCES/PERSO.CPP`: scene load/prewarm hook.
- `docs/ANDROID.md` if the test protocol changes.

### Risks

- Prewarming can add a visible load hitch.
- Releasing surfaces too early can reintroduce the shutdown/present hazards; the current shutdown crash is in `OpenXR_Shutdown` and is pre-existing, not a render-loop crash.
- Power/DVFS is outside the app's control; do not promise a code fix for it.

### Verification

- Compare `dumpsys gpu` and the surface-budget logs before/after.
- Capture at 100% and at 545 MHz; report both.
- The target is stable 72 at the default 545, not only the boosted 599/640 case.

## Phase 8 — Compositor and system behaviour

### Goal

Document and, where possible, reduce the remaining compositor cost. This is not app renderer code.

### Evidence

The heavy scene reports `CPU&GPU 31.7 ms`, `Stale 26`, `TW 0.67`, while the app itself is about 9.4 ms CPU and the GPU is about 50% busy. The compositor is repeating frames and the app is not keeping the 72 Hz cadence.

### Actions

- Record `dumpsys` GPU/compositor metrics with each test.
- Check battery/charger state, thermal status, GPU `trans_stat` and per-thread placement.
- Confirm whether the Quest runtime is throttling a not-worn or low-battery session.
- Do not change app settings or the display refresh rate.

## Test gate after every phase

Every phase stops at the same gate. Do not start the next phase before the user has tested and the result is reviewed.

1. Build the desktop binary.
2. Run fixed-dt screenshot comparisons on cube 3, cube 5, cube 100 and the rain save. Any pixel difference is a failure unless the phase explicitly changes the rain and the user approved it.
3. Build the Quest shared library and bundle the APK.
4. Install on `2G0YC1ZF9J0W5D`.
5. Ask the user to put on the headset, charge to 100%, load the Citadel night-rain save, and confirm they are ready.
6. Capture 120 s with `good_run/capture.sh`.
7. Report FPS, eye0, eye1, frame, present, flush, triangles, GPU clock, GPU busy, Stale, battery and temperature.
8. Compare against the baseline numbers above and the previous phase.
9. Do not commit. Wait for the user's decision.

If a phase is rejected, revert only that phase's changes, keep the previous committed baseline and move to the next candidate.

## Constraints

- Do not change resolution, supersampling, MSAA or menu/config values.
- Preserve the game's visual output and feel unless the user explicitly approves a change (the rain GL-line option is the only planned visible change).
- No commits until the user says so.
- One phase at a time; ask for a test after each phase.
- Record every measurement with the battery and GPU clock; a 36% battery capture is not a valid 72 FPS target test.
