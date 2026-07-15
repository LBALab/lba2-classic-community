# Widescreen and higher-resolution support

The engine was authored for a fixed 4:3 framebuffer (640×480, 8-bit paletted). SDL scales and letterboxes that buffer to the window. This doc is the working plan for changing that — widescreen aspect ratios first, higher render resolutions later — and a record of what has been done so far.

It supplements [issue #45](https://github.com/LBALab/lba2-classic-community/issues/45). The issue sketches the goal and a rough A/B/C split; the phasing here replaces that sketch with what the Phase 0 audit surfaced.

## The core idea: two coordinate spaces

Most of the difficulty in this work comes from one fact: the 640×480 number means two unrelated things, and they have to be separated before anything can move.

| Space | What it is | Where the literals live |
|-------|-----------|-------------------------|
| **Render space** | The framebuffer the engine draws into — `ModeDesiredX × ModeDesiredY`. 3D projection, the sort tree, brick rendering, cinema bars, fullscreen clears, buffer allocations. | A small number of sites: the projection origin in `SOURCES/EXTFUNC.CPP`, the brick clips in `SOURCES/GRILLE.CPP`, the sort preclip in `SOURCES/SORT.CPP`, cinema bars and the HUD anchor in `SOURCES/OBJECT.CPP`, allocations in `SOURCES/MEM.CPP` / `SOURCES/INITADEL.C`. |
| **UI space** | The authored 2D layout — menus, inventory, holomap, dialogue, save screens, the HUD, pre-rendered HQR backgrounds, bitmap fonts. Pixel-positioned against a 640×480 canvas. | ~150 literal `320` / `639` / `240` sites across ~20 files in `SOURCES/`. |

Render space wants to *grow* with the output. UI space was *hand-tuned* to 640×480 and either has to be re-centred, re-scaled, or left alone — that is a deliberate design choice, not a mechanical edit.

A change that widens the framebuffer without addressing UI space produces a working 3D view with the 2D UI left-justified and garbage in the margin. That is expected and is the boundary between the render-space and UI-space phases below.

## What the Phase 0 audit found

The projection audit ([WIDESCREEN_PROJECTION_AUDIT.md](WIDESCREEN_PROJECTION_AUDIT.md)) mapped every 4:3 assumption in the projection path against the `RESOLUTION_X` / `RESOLUTION_Y` work in [PR #134](https://github.com/LBALab/lba2-classic-community/pull/134). It changed the plan in four ways:

- The projection origin is the primary assumption, and PR #134 did not touch it. The whole 3D world projects through one setup call, `SOURCES/EXTFUNC.CPP:93`, which passes a hardcoded screen centre of `320, 240`. With equal X and Y focal lengths, that pair *is* the 4:3 ratio. Until it derives from `ModeDesiredX / 2`, a wider framebuffer renders the world anchored left of centre rather than filling the frame.
- The brick renderer was missed entirely. `SOURCES/GRILLE.CPP` still hardcodes 640×480 in three brick clip sites (`AffBrickBlock` / `AffBrickBlockColon` / `AffBrickBlockOnly`, each `XScreen < 640 ... YScreen < 480`) and in one isometric centre offset (`Map2Screen`, `+288` / `+226`). PR #134 routed the other render-space literals but not these.
- Cube culling is not a concern. Exterior cube and decor visibility is gated in world space — camera distance and the near plane — not in screen space. It is width-independent and needs no change.
- The sort preclip is routed but mis-fed. `SOURCES/SORT.CPP:211` already preclips against `ModeDesiredX` / `ModeDesiredY`, so it is wide-aware — but it operates on coordinates produced by the mis-centred projection above. It only behaves correctly once the projection origin is fixed.

## Target behaviour

What the engine does today:

- **First launch auto-detects.** With no `--resolution` flag and no saved `lba2.cfg` resolution, the engine reads the primary display's aspect and boots a widescreen render size at the original 480 height (for example 848×480 on a 16:9 monitor), fullscreen by default (the shipped cfg template carries `DisplayFullScreen: 1`). A 4:3 display keeps 640×480.
- **The player stays in control.** The auto choice is re-derived each launch, never persisted, so it follows a monitor swap. Any explicit choice overrides it and sticks: the Display options submenu, the `resolution` console verb, or `--resolution` all write `lba2.cfg`. Reverting to Classic 640×480 is one toggle in the Display menu.
- **Precedence: `--resolution` CLI > `lba2.cfg` ResolutionX/Y > auto-detect > 640×480.** A headless/dummy driver or a failed display query falls back to 640×480, which is why the test harness (always pinned to `--resolution 640x480`) is unaffected.
- **No forced 16:9 cap.** A display wider than 16:9 renders at its true aspect up to the engine's maximum width (1920); height stays at 480. The render-space work supports arbitrary width, and SDL letterboxes/pillarboxes the framebuffer to the physical display.

> Historical note: the original goal (sketched in issue #45 and the first draft of this doc) was a no-setting auto-detector tied to fullscreen, capped at 16:9. The shipped design keeps the auto-detection but makes it the *default*, not the only option, and renders true display aspect rather than a 16:9 cap. See [Phase 4](#phase-4-fullscreen-and-runtime-resolution).

## Phases

Phase 0 is done. Phases 1–3 are the render-space core — a correct, centred wide 3D view. Phase 4 makes it reach players. Phases 5–6 are polish and lock-in.

### Phase 0 — research (done)

The projection 4:3 audit, captured in [WIDESCREEN_PROJECTION_AUDIT.md](WIDESCREEN_PROJECTION_AUDIT.md). Inventoried every render-space aspect-ratio assumption, separated render space from UI space, and established the four findings above. It is the baseline the rest of the plan is cut against.

### Phase 1 — projection capture (done)

Before changing projection math, give it a regression net. Phase 1 added a side-channel projection-capture file format that records every projection-pipeline event during a harness run — `SetProjection`, `SetIsoProjection`, per-vertex `LongProjectPoint`, batched `ProjectList`, and isometric `Map2Screen`. Output can be either full text (one event per line, large but diff-able) or a single FNV-1a 64-bit digest (60 bytes, CI-committable). See [`docs/CONTROL.md`'s Projection capture section](CONTROL.md#projection-capture) for the workflow and [`tests/projection/`](../tests/projection/) for the committed baselines.

The contract: every committed corpus hash at 640×480 must stay stable through Phase 2. The corpus has 22 saves witnessing the exterior path, 28 saves witnessing `Map2Screen`, and all 50 touching `SetIsoProjection` — see [`tests/projection/README.md`](../tests/projection/README.md) for the coverage matrix. If any hash drifts, the failing save name points at which projection path regressed.

This is independent from polyrec (which keeps recording draw calls for ASM↔CPP equivalence); the two harnesses are composable.

### Phase 2 — projection correction (done)

Routed every render-space projection constant through `ModeDesiredX/Y`. All behaviour-preserving at 640×480; validated against the Phase 1 projrec corpus + demo baselines on each PR.

The three sites the Phase 0 audit called out:

- **PR #199** — `SOURCES/EXTFUNC.CPP:93` `SetProjection(320, 240, ...)` → `SetProjection((S32)(ModeDesiredX/2), (S32)(ModeDesiredY/2), ...)`. Because `LongProjectPoint` and `ProjectList` share these globals, the one change covers the sort tree and object drawing together.
- **PR #199** — `SOURCES/GRILLE.CPP` brick clips (`XScreen < 640 ... YScreen < 480` in `AffBrickBlock` / `AffBrickBlockColon` / `AffBrickBlockOnly`) routed through `(S32)ModeDesiredX/Y`. Same shape as the existing sort preclip.
- **PR #200** — `Map2Screen` `+288` / `+226` (`GRILLE.CPP:1370`) routed through `(S32)ModeDesiredX/2 - 32` / `(S32)ModeDesiredY/2 - 14`. The `-32` and `-14` are iso-grid geometry shims (not the framebuffer centre minus a clean offset); documented inline so the next maintainer doesn't have to re-derive them. Also fixed the `test_grille` ASM-equivalence test to pin `ModeDesiredX/Y` (the ASM half still bakes in `288 / 226`).

Plus one site the original audit missed, surfaced while writing #200's inline docs:

- **PR #201** — iso projection-setup analog of `EXTFUNC.CPP:93`. `SetIsoProjection(320 - 8 - 1, 240)` in `SOURCES/GAMEMENU.CPP:515` (`Init3DView`, boot init) and `SOURCES/COMPORTE.CPP:351` (`DrawMenuComportement`, behaviour menu wheel) routed through `(S32)ModeDesiredX/2 - 9, (S32)ModeDesiredY/2`. The `-9` is the iso-grid horizontal shim that pairs with Map2Screen's `-32`; the two iso sites must move together to keep interior scene composition coherent.

Net: eight call sites across four files, ~30 lines of code change. The corpus + demo projrec hashes stayed identical at 640×480 throughout, and `test_grille` is green again.

One render-space site the original audit flagged remains open: `SOURCES/GRILLE.CPP:954-955` `xmin = 639; ymin = 479;` — bounding-box sentinel seeds in `AffOneBrick`. At 640×480 the seeds work as upper bounds; at any wider render width they'd cap `xmin` at 639 and miss bricks beyond. Two-line fix; tracked as a Phase 2 follow-up so it lands before Phase 3 widens the framebuffer.

### Phase 3 — widen the framebuffer (done)

Builds at a wider render width work end-to-end: the 3D world centres and fills the wider frame, bricks no longer clip, the sort preclip is fed correct coordinates, and `Map2Screen` lands iso bricks at the centred grid origin. The 2D UI is still 640-anchored (Phase 5 problem).

Verified at 768×480 via [How to test a wider build](#how-to-test-a-wider-build) below. The wide build also has its own projrec regression net — `tests/projection/baselines/corpus_768x480.projrec.hash` and `demo_768x480.projrec.hash`, generated by passing `LBA2_PROJREC_WIDTH=768` plus the wide binary to `scripts/dev/regen_projrec_baselines.sh`. The wide hashes are necessarily different from the 640×480 ones (different `ModeDesiredX` → different `SETPROJ`, different `M2S`, etc.), but they're deterministic across runs so any future drift in the wide path gets caught.

The wide build still requires a compile-time `RESOLUTION_X` override; runtime selection lands in Phase 4.

### Phase 4: fullscreen and runtime resolution

**Done.** Two strands combine into the [target behaviour](#target-behaviour): explicit player choice (the runtime-resolution work) plus first-launch auto-detection layered on top of it. The engine boots widescreen by default and the player can override or revert at any time.

Boot resolution resolves in `Res_LoadBootDimensions` (`SOURCES/RES_SWITCH.CPP`) with precedence **`--resolution` CLI > `lba2.cfg` ResolutionX/Y > auto-detect > 640×480**:

- **Auto-detect** (`Res_AutoDetectDimensions`) queries the primary display windowlessly (`Window_GetPrimaryDisplayDimensions`, `SDL_GetPrimaryDisplay` + `SDL_GetCurrentDisplayMode`) and snaps to a widescreen width that preserves the display aspect at 480 height (`Res_SnapWidescreenWidth`, shared with the Display submenu's catalog). It runs only when CLI and cfg are both silent, returns false on a 4:3 display or a dummy/offscreen driver, and is never persisted. The video subsystem is brought up early in `PERSO.CPP` (idempotent) so the display is queryable before the framebuffer is sized. The two boot resolves (MainBuffer in `PERSO.CPP`, Log/Screen in `INITADEL.C`) must agree, which is why the early resolve also has to see the display.
- **Fullscreen** is an independent `DisplayFullScreen` cfg flag (`Res_LoadBootFullscreen`) plus an in-game toggle; the shipped cfg template defaults it on, so a fresh install boots fullscreen. `CreateWindowSurface` brings the window up fullscreen directly, and `WINDOW.CPP` aspect-correctly letterboxes/pillarboxes the chosen framebuffer to the physical display.
- The boot log's `Display` line reports the chosen `WxH`, its source (`CLI` / `cfg` / `auto` / `default`), and the window mode.

#### Runtime resolution switching

Lets the player change render resolution from a console verb or a Display options submenu without relaunching. **Shipped.** The `resolution` console verb, keep/revert dialog, and `lba2.cfg` persistence landed in PRs #237–#241, #244; the Display options submenu (a "Resolution: <current>" cycle row over the curated Classic + display-aspect Widescreen catalog, plus a "Fullscreen" toggle, `SOURCES/GAMEMENU.CPP` `GereDisplayMenu` / `CycleResolution`) landed after. The auto-detect default reuses the same catalog math, so the booted resolution is the same Widescreen entry the menu offers. See [`RUNTIME_RESOLUTION.md`](RUNTIME_RESOLUTION.md) for the design, phasing, and risks.

### Phase 5: UI (C2) and HD groundwork

This phase had two strands. The UI strand is done; the HD strand moved to its own phase.

- UI strategy: **C2 chosen and shipped.** The re-anchor campaign (PRs #213–#228, plus #260, #289, #291, #310) routed every menu, inventory, holomap, dialogue, and save surface through `ModeDesiredX/Y`, so the 2D UI is correct in a wider frame. The [vertical re-anchor](#ui-vertical-re-anchor-tall-framebuffers) below extends the same discipline to a taller frame. See [UI strategy](#ui-strategy-c1-vs-c2) for why C2 over C1.
- HD-only concerns (filler precision, bitmap fonts, the 16-bit Z-buffer and `Fill_ZBuffer_Factor`, mouse-coordinate inversion, Smacker upscale): **moved to [Phase 7](#phase-7-native-hd).** Widescreen shipped without resolving them, so they are tracked separately as the native-HD frontier. The analysis below is the record of what the X-axis campaign settled for HD.

#### UI vertical re-anchor (tall framebuffers)

The C2 campaign above centred the 2D UI horizontally (`ModeDesiredX/2`) for a *wider* frame. It left UI height at the authored 480, so vertical positions stayed authored-pixel. This sweep (branch `fix/ui-vertical-centre`) does the Y axis, so the UI is also correct in a *taller* frame (`ModeDesiredY > 480`, reachable via `--resolution` and the `resolution` console verb even though the shipped widescreen modes stay 480-tall).

The model is one 480-tall authored canvas floated into the live framebuffer. Two macros in `SOURCES/DEFINES.H`, both read live (never cache; they must survive a runtime `Res_Switch`) and both `0` at `ModeDesiredY == 480` so every site is behaviour-preserving at the classic resolution:

- `UI_VCENTER_OFS = ((S32)ModeDesiredY - 480) / 2`, to float the canvas to the vertical middle.
- `UI_VBOTTOM_OFS = (S32)ModeDesiredY - 480`, to pin to the bottom edge.

Each surface's anchor mode mirrors how it reads on the 4:3 canvas:

| Surface | Site | Anchor |
|---|---|---|
| Menus, save screen, ask-choice list | `GAMEMENU.CPP` (`DrawGameMenu` both branches, `Y_START_CHOICE`, `SAVE_THUMB_Y`, save title, current-save preview box) | centre |
| Menu mouse hit-test | `GAMEMENU_MOUSE.CPP` (`GameMenuHitTestRow`) | tracks the draw (see note) |
| Holomap planet | `HOLOGLOB.CPP` (`PLANET_CENTREY`, the three `CENTERY` sites) | centre |
| Holomap name strip | `MESSAGE.CPP` (`HoloWinDial`) | bottom |
| Dialogue box | `MESSAGE.CPP` (`NormalWinDial`, `BigWinDial`) | bottom, already correct via `ClipWindowYMax`; no change |
| End-game / pcx-message text | `MESSAGE.CPP` (`DialLetterboxInsetY`) | confined to the centred backdrop |
| Inventory wheel, slate, found-object | `INVENT.H` (`INV_TOP_OFFSET`), `INVENT.CPP` (slate `PLAN_Y0`/`L_Y0`/`R_Y0`, `FOUND_Y*`) | centre |
| Action (behaviour) menu | `COMPORTE.CPP` (`CTRL_Y0`/`CTRL_Y1`) | centre |
| HUD (life, magic, coins) | `OBJECT.CPP` | corner/edge, already correct |

Notes:

- `INV_TOP_OFFSET` is the vertical twin of the pre-existing `INV_LEFT_OFFSET`. The slate's `PLAN_Y` overlays now track the slate art, which `DrawArdoise` already loads vertically centred via `Image_LoadCentered`; previously the art floated while the overlays stayed at the top.
- `DialLetterboxInsetY` is the vertical twin of `DialLetterboxInset`. When a centred PCX backdrop is up (`Dial_Letterbox`), the text is inset to the picture on both axes instead of spilling into the letterbox bars. The backdrop stays a 640x480 centred letterbox (the project's cinematic convention), not scaled, since a fixed 4:3 pixel-art image cannot fill a taller frame without distortion or cropping.
- Mouse hit-test: `GameMenuHitTestRow` is a separate copy of the `DrawGameMenu` layout math. It carried a hardcoded `320` X centre (a pre-existing widescreen bug the X campaign missed, so clicks missed the menu at any width other than 640) and no Y offset. Both now mirror the draw (`ModeDesiredX/2` plus `UI_VCENTER_OFS`). When floating a menu's draw, check for a duplicate hit-test doing the same math separately.

Verification: every site is `0` at 640x480 by construction and the host suite stays green. At tall res, captures confirm the menu block, holomap planet, and inventory wheel each translate by exactly `UI_VCENTER_OFS`, the holomap strip and dialogue box pin to the bottom, and the end-game text lands on its backdrop. The ask-choice list, action menu, found-object appear phase, and mouse hit-test have no headless capture path and were verified in-game.

#### HD prerequisites — what the X-axis campaign settled

The X-axis C2 campaign (PRs #213–#228) settled the structural prerequisites for HD. What's HD-ready vs what's still HD-only work:

**HD-ready** (X-axis campaign cleared these):

| Layer | Why it's ready |
|---|---|
| Memory pool | `Mem_ConfigureScreenBuffers(resX, resY)` sizes every framebuffer-dependent entry; `Mem_TeardownMainBuffer` allows re-sizing at runtime (Phase 0 of runtime-res, shipped). Z-buffer entry was the last "missing from the config sweep" bug (PR #220). |
| Save format | `MAX_PLAYER` is keyed to literal `640×480` per [save-format invariant](#what-does-not-need-to-change); saves cross resolutions without migration. Verified across every routing PR in the campaign. |
| Projection + isometric origin | Phase 2 routed all six call sites through `(S32)ModeDesiredX/Y/2`. `SetProjection` and `SetIsoProjection` are both width-correct at any resolution. |
| Per-surface centring discipline | Image-edge vs framebuffer-edge is a documented decision per-surface (see [`WIDESCREEN_HARDCODED_DIMS_AUDIT.md`](WIDESCREEN_HARDCODED_DIMS_AUDIT.md) — the per-surface table is the reusable artifact). |
| Audio panning + 2D distance | `GiveBalance` re-derived from `ModeDesiredX/Y`; pinned at the math layer by `test_ambiance_balance` (PR #228). |
| Cinematic centring | Smacker writes directly into Log at the centred origin with proper stride (PR #226). |

**Still HD-only work** (Y-axis + true high-resolution concerns):

| Concern | Notes |
|---|---|
| Y-axis clamps | **Done** (branch `fix/widescreen-y-clamp`). A4 (terrain horizon `Y=479`, 8 sites in `3DEXT/TERRAIN.CPP` `FillBlackPolyZBuf`) and A5 (holomap Z-buffer Y bounds, 4 sites in `HOLOGLOB.CPP`) now route through `(S32)ModeDesiredY - 1`. The literals were *not* the real pin, though: the 3D view stayed capped at 480 at any resolution because `LoadContexte` restored `ClipWindowYMax` verbatim from the save file (`SAVEGAME.CPP:2053`). `SaveContexte` persists the play clip window at its render height, so a 640×480 save carries `479`; the fix re-derives the full window from `ModeDesiredY` on load. All three changes are behavior-preserving at 640×480 (`ModeDesiredY-1 == 479`; projrec corpus hashes byte-identical with/without via git-stash A/B). Opening the view exposed the vertical-framing question — see [Vertical framing at HD](#vertical-framing-at-hd--open-question) below. |
| Actor shadows at Y > 480 | **Done** (branch `fix/shadow-cull-hd`, issue #408). The circular ground shadow of every actor rasterises through per-scanline span tables in `SOURCES/BEZIER.CPP` (`TabCoorPoint` / `TabNbPoint`) that were sized by `#define MAX_SCAN_LINES 480`, a separate Y-axis pin the `fix/widescreen-y-clamp` campaign didn't reach. Once the 3D view opened above 480, shadows whose screen-Y landed at or below line 480 were dropped, culling a `ModeResY - 480` band at the bottom (240 lines at 720p, 600 at 1080p). Fix points `MAX_SCAN_LINES` at `ADELINE_MAX_Y_RES`, the same constant that sizes `TabOffLine[]`; the visible clamp is already `ModeResY`-aware via `ClipYMin/ClipYMax`, so no logic changes and it's a no-op at 480. Audit entry A11 in [`WIDESCREEN_HARDCODED_DIMS_AUDIT.md`](WIDESCREEN_HARDCODED_DIMS_AUDIT.md). |
| Holomap at Y > 480 | A4/A5 done, but `ui holoplan` at `1024×768` still times out, a cinema-mode rendering bug independent of the Z-buffer bounds and the planet bitmap centring. The globe holomap (`ui holomap`, the rotating planet and its name strip) is verified at 768 via the [UI vertical re-anchor](#ui-vertical-re-anchor-tall-framebuffers) above; the `ui holoplan` island zoom is the part still blocked by this timeout. The A5 Z-buffer fix itself is correct and 640-identical but **not verified at tall res** until the timeout is resolved. Add `test_ui_holoplan_768x768.sh` once unblocked. |
| Filler precision | **Swept at 1920 — de-risked** (2026-05-29). Captured a 6-scene polyrec corpus at 1920-wide and replayed it under ASan+UBSan and ASM-vs-CPP. No overflow, no out-of-bounds across ~24k full-width draw calls (the feared accumulator overflow does not occur). ASM↔CPP equivalence degrades from bit-exact (640) to **118/1,966,080 pixels = 0.006%** (1–2px edge rounding on a few triangles through the FPU slope path — the long-double/x87 precision class, not overflow); cosmetically invisible. Practical consequence: the 640 ASM-equivalence corpus can't be extended to 1920 without tolerancing or pinning FP, but the fillers render correctly at HD width. |
| Bitmap fonts | `LIB386/SVGA/FONT.CPP` / `AFFSTR.CPP` blit at 1:1 pixel size. At 1920×1080 in-game text is tiny. Either pixel-scale the existing 8×N glyphs (cheap, blocky) or re-author a higher-DPI font set (slow, clean). |
| Z-buffer precision | 16-bit Z + `Fill_ZBuffer_Factor` is tuned for the 4:3 frustum's depth range. May Z-fight at HD aspect ratios on distant geometry; the polyrec harness catches drift here too if you log Z values. |
| Smacker upscale policy | **Shipped** (`feat/fmv-widescreen-fit`). The `VideoFullScreen` "Movie Size" flag now selects the 320×200 upscale policy in `SOURCES/VIDEO_FIT.H`: **fit-to-screen (default)** scales by the largest uniform factor that fills the frame, then pillar/letter-boxes, so it is aspect-preserving (not the distorting stretch the earlier note warned about); **original** keeps the legacy centred 2× letterbox (PR #226). Both are byte-identical at 640×480 (fit factor exactly 2.0), so the ui_video golden holds either way; pinned by `tests/video_fit`. Two findings drove the design. First, the FMV sources are genuinely full-frame 320×200 with no baked-in bars (checked INTRO plus SENDELL/BALDINO/CRASH; picture reaches source rows 0..199), so fit shows real picture edge-to-edge rather than scaling up bars. Second, engine cinema mode draws 40px top/bottom bars (`GLOBAL.CPP:62`), which *original* matches (letterbox continuity with in-game cutscenes) but *fit* does not (its bars become side pillars). Fit is the default regardless: the bigger undistorted picture was judged the better out-of-box widescreen experience, with original one toggle away for the matched-letterbox look. Do not flip the default without revisiting that call. |
| Mouse cursor inversion | Today mouse positions are framebuffer-pixel coords and SDL scales 1:1. Works at any framebuffer resolution. Only needs invert if we add a "render at 640, pixel-double in present" mode. |

The handful of remaining open audit rows (B7 done; A4/A5 done in `fix/widescreen-y-clamp`; A10 PERSO cosmetic) are all in the second column.

#### Vertical framing at HD — open question

With the Y-axis clamps and the save-clip pin fixed, the exterior 3D view fills
the full framebuffer height. That immediately raises a composition question we
have *not* resolved — captured here to revisit.

**The mechanism.** The projection (`LPROJ3DF.CPP`) is square-pixel with a fixed
field constant: `Xp = ModeDesiredX/2 + (x-cam)·ChampX/Z`, `Yp = ModeDesiredY/2 +
(y-cam)·ChampZ/Z`, with `ChampX`/`ChampZ` constant. So growing the framebuffer
*reveals more field of view* rather than scaling the image — which is exactly
why widescreen works (wider frame → more world horizontally). It's symmetric,
though: a taller frame reveals more world *vertically* too, and two things break
because of it:

- The character looks "zoomed out." He isn't smaller in pixels (pixels-per-world
  unit is resolution-independent) — there's just more frame around him, so he's a
  smaller *fraction* of it. The extra vertical FOV is spent on sky and distant
  terrain.
- The "sky" is a flat textured plane at a fixed world height (`DrawSky`,
  `3DEXT/DRAWSKY.CPP`), not a full-screen backdrop. Look up far enough and the
  camera sees past its far edge — above it there's nothing to draw, so you get the
  clear colour (fog in-game, a void band at the top).

For 16:9 1080p vs 640×480 that's roughly +200% horizontal FOV (wanted) and +125%
vertical FOV (the liability). The vertical reveal also risks exposing authoring
assumptions: distant cube boundaries, pop-in, and the known terrain cube-boundary
seam (original, currently hidden by 4:3 framing).

**Levers / options** (not yet decided):

1. **Cap vertical FOV / letterbox.** Bound the play viewport's height relative to
   width (e.g. ≤16:9) and bar the remainder. The engine already has the machinery —
   `FixeCinemaMode` draws `COUL_CINEMA` bars top/bottom from `ModeDesiredX/Y`
   (`OBJECT.CPP:4706-4707`), and the clip-window plumbing fixed above is what bounds
   it. Art-faithful, never reveals the sky edge or map seams, small. Cost: bars (or
   repurpose for HUD); the 3D view is a wide strip, not full-height.
2. **Camera distance (`VueDistance`).** Pull the follow camera in so the extra
   vertical real estate is filled by a closer, bigger view of the action instead of
   dead sky — "recompose, not crop." `VueDistance` is already a first-class, per-zone
   authored value (`DefVueDistance[VueCamera]`, overridable per camera-zone at
   `OBJECT.CPP:6216`, interpolated via `RegleTrois`, saved/loaded). Caveat: it's
   *gameplay*-tuned — a global zoom risks camera-into-geometry clipping in tight
   scenes, reduced situational awareness, and breaking scripted camera moments, so it
   carries a per-scene QA burden. Note it's **orthogonal to the sky reveal**: distance
   reframes the subject but doesn't lower the angular FOV, so a closer cam reduces but
   doesn't guarantee removal of the top void (depends on follow pitch).
3. **Extend the sky / background.** Embrace the reveal: raise/enlarge the `DrawSky`
   plane to cover the larger view and sky-colour the clear. Full-screen 3D, most
   immersive, but highest risk — reveals map edges and the terrain seam, needs
   per-scene art QA across the whole game.
4. **Constrain offered resolutions.** Derive height from width at a fixed aspect so
   HD means "wider, not taller." Sidesteps the issue with almost no rendering work,
   but isn't true native tall-HD.

**Leaning:** recompose rather than crop or reveal — keep the widescreen horizontal
FOV, and treat the extra vertical with a modest aspect-proportional `VueDistance`
adjustment (option 2) where it's safe, falling back to the letterbox cap (option 1)
where per-scene clipping/readability makes zooming in untenable. The two blend.
Decide the actual HD target first (specifically 16:9 1080p vs arbitrary tall
windows) — it bounds how much vertical reveal we ever have to handle. Cheap to
evaluate empirically: A/B Desert Island at 1920×1024 across a few `VueDistance`
values with the polyrec capture/replay harness.

#### Testing posture

What the campaign protected against, and what it didn't:

**Protected** — every routing PR ships with a 640×480 byte-identical contract:

- `host_quick` includes a unit test for every newly-extracted pure-math layer (`test_image_load` for the bitmap helpers, `test_ambiance_balance` for the pan/volume math). 12/12 host tests in CI.
- The `ui_*` capture-based regressions (`ui_holomap`, `ui_holoplan`, `ui_inventory`, `ui_video`) each pin one UI surface byte-identical to its 640×480 golden. Run via `bash tests/automation/test_ui_*.sh` with `LBA2_GAME_DIR` set. The inventory, dialog, and menu-options goldens are captured with `ui --black-bg <verb>` (CONSOLE_CMD.CPP), which clears the framebuffer to black before the UI draws, so each golden is a cleanroom UI-only frame rather than UI composited over a live (and FoV-sensitive) 3D scene. Surfaces where the scene is the test (`found-object`) keep the live render.
- Per-PR visual A/B at 768 + 1024 lives in PR descriptions as embedded screenshots.

**Partial wide-width coverage** — `test_ui_inventory_wide.sh` and
`test_ui_menu_options_wide.sh` render those two surfaces at 768×480 and assert that
the centred 640×480 crop of the wide capture is byte-identical to the existing 640
golden (`ui_compare_wide` in `lib.sh`). No per-resolution goldens; the 640 golden
stays the single source of truth, which keeps the test honest against future
projection or UI changes.

**Still not protected** at wider widths:

- Dialog. The dialog text strip is full-framebuffer-width by design (it scales with
  the render width and text reflows within it), so a fixed centred-crop test against
  a 640 golden doesn't apply. Whether to convert it to a centred 640-strip is a
  separate design call.
- `holomap`, `holoplan`, `menu-main`, `video`, `found-object`. These don't have wide
  tests yet — some have natural fade-to-black backdrops (`holomap`, `video`) and
  should be cheap to wire up; `found-object` composites the hero+item over the live
  scene by design, so its golden carries FoV-sensitive content and isn't
  centred-crop-friendly.
- The projrec harness has a `corpus_768x480.projrec.hash` baseline committed under
  `tests/projection/baselines/`, but it's not part of `make test` and was not
  exercised on any C2 routing PR in this campaign.
- B5 (game-over `SetProjection`), B6 (venetian-blind reveal), and the menu
  stretch-BG path have zero automated coverage at any width — they were re-anchored
  without a capture verb.

Routing PRs touching a covered surface (inventory, menu-options) will fail
`*_wide.sh` automatically if the centring breaks. For the other surfaces, the rule
of thumb still holds: visuals at 768 + 1024 belong in the PR body, and the next
person on the branch should re-eyeball them before merging.

### Phase 6 — regression

Run the polyrec suite and the host tests. Confirm the default 640×480 build is byte-identical to before, and check the wide build across representative scenes — exterior terrain, brick interiors, cinema bars, the HUD — for centring and clipping. Lock the result in.

### Phase 7: native HD

Widescreen shipped without true high resolution: the framebuffer grows horizontally, the UI is C2-correct, and fullscreen letterboxes the chosen resolution. Native HD, meaning crisp and legible rendering at 1080p-class resolutions rather than just wider, is the remaining frontier. It is its own phase because none of it blocks widescreen and each item ships independently. The detailed engineering notes are in [HD prerequisites and Still HD-only work](#hd-prerequisites--what-the-x-axis-campaign-settled) under Phase 5; this is the actionable, priority-ordered view.

1. **Bitmap fonts (open, highest player impact).** `LIB386/SVGA/FONT.CPP` and `AFFSTR.CPP` blit glyphs 1:1, so HUD, dialogue, and menu text is tiny at 1080p. Pixel-scale the 8×N glyphs by an integer factor derived from `ModeDesiredY`, scaling advance widths and line spacing with them. Nothing is in flight, so it is a clean slate.
2. **Iso interior sharpness (experimental).** The isometric brick layer is a 640-class blit; at HD it wants scaling or edge-upscaling (xBR / EPX) so interiors stay crisp. Five unmerged branches explore this: `feat/iso-xbr-optimized` (furthest along), `feat/iso-xbr-bricks`, `feat/iso-brick-scale`, `feat/iso-scale`, and `feat/iso-zoom-screenspace`. Findings live on-branch in `ISO_SCALE_FINDINGS.md`; the on-main groundwork is [`ISO_SPACE_AUDIT.md`](ISO_SPACE_AUDIT.md). Needs triage down to one mergeable approach.
3. **Holomap at Y > 480 (blocked).** The A4/A5 Z-buffer-bounds fix is correct and 640-identical but cannot be verified at tall resolution: `ui holoplan` at 1024×768 times out on a cinema-mode rendering bug. Resolve that, then add `test_ui_holoplan_768x768.sh`.
4. **Vertical framing (decision pending).** A taller frame reveals more vertical field of view, exposing the sky edge and map seams. The levers and the current leaning are in [Vertical framing at HD](#vertical-framing-at-hd--open-question). Cheap to A/B with the polyrec harness.
5. **Watch-items.** The 16-bit Z-buffer plus `Fill_ZBuffer_Factor` may Z-fight on distant geometry at HD aspect; filler precision is de-risked (swept at 1920, 0.006% sub-pixel edge drift, no overflow). Smacker upscale is now a player option (fit-to-screen default, aspect-preserving) and 1:1 mouse coords need no action.

## UI strategy: C1 vs C2

Phase 5's UI work has two mutually exclusive flavours. Pick based on whether higher resolution (not just widescreen) is a goal.

| Flavour | Approach | Generalises to HD? | Cost |
|---------|----------|--------------------|------|
| **C1 — centered UI** | Introduce a `UI_X0` offset; replace UI-space `320` / `639` literals with offset-aware names. The UI keeps its 640×480 layout, centered in the wider canvas. | No — at 1080p the UI becomes a small box in the middle. | ~150 mechanical edits, reversible, easy to review. |
| **C2 — scaled UI** | Render UI into its own 640×480 logical buffer, scale-blit it to the framebuffer at present time. UI literals stay untouched. | Yes — works at any output resolution. | More plumbing in `LIB386/SVGA/` to thread the target-buffer choice through the draw primitives. |

C1 is the cheaper path if widescreen-at-480 is the only target. C2 is the correct path if the engine should run at arbitrary resolutions. Committing to C1 first is a dead end for HD.

## What does not need to change

- **The assembly.** Every `.ASM` source is commented out in the active `CMakeLists.txt` files; the shipping binary is 100% C++ on every platform. The C++ blit and filler path in `LIB386/SVGA/` and `LIB386/pol_work/` already uses `ModeDesiredX` for stride. `SOURCES/COPY.CPP` is a dead reference file (entirely inside a comment block), and `SIZE_VIDEOLINE` in `LIB386/H/FILLER.H` is an ASM-only equate. The only assembly that matters is in `tests/`, for equivalence checking.
- **The save format.** `MAX_PLAYER` (`SOURCES/DEFINES.H`) is intentionally kept keyed to the literal `640*480` rather than the render resolution — it partitions the `BufSpeak` buffer, and a silent slot-count change would walk off the end of it. Widening the framebuffer must not change the save-slot count.

## The `feat/widescreen-deck` branch

There is an existing branch, `origin/feat/widescreen-deck`, that adds Steam Deck widescreen. It is **not a viable base** to build on:

- It has no merge base with `main` (the repo was re-rooted after it forked).
- It hardcodes the Steam Deck's 768×480 ratio everywhere as literal `+64` / `767` / `384` — no constants, no toggle.
- It mixes in unrelated changes that look like search-and-replace overreach: a `-0x8000 → 0x8000` projection-sentinel flip in `LIB386/OBJECT/AFF_OBJ.CPP` (which would stop off-screen geometry being culled), and a `RealShifting` change in `SOURCES/PERSO.CPP` (conveyor-belt speed, a world-space value, not a pixel value).

It is still useful as a **call-site map** — its diff touches most of the render-space and UI-space sites the phases above have to address, and the Phase 0 audit confirmed it found the same `GRILLE.CPP`, `SORT.CPP`, and cinema-bar sites. Re-implement against current `main` using the constants, do not rebase the branch.

## How to test a wider build

**Preferred (any existing build):** pass `--resolution WxH` at launch. No rebuild needed; the framebuffer is sized at startup.

```bash
./build/SOURCES/lba2cc --resolution 768x480 --game-dir /path/to/data
./build/SOURCES/lba2cc --resolution 1280x720 --game-dir /path/to/data
```

Range `320x200` – `1920x1024`, width must be a multiple of 8 (matches the historical `RESOLUTION.H` static check). Omit the flag for the compile-time default (`640x480` unless overridden at configure time). Useful for sweeping width-dependent bugs across many candidate resolutions from one binary — see [WIDESCREEN_HARDCODED_DIMS_AUDIT.md "How to sweep"](WIDESCREEN_HARDCODED_DIMS_AUDIT.md#how-to-sweep-for-alignment-class-sibling-bugs).

After Phase 2 + 3, the resulting build shows the centred 3D world filling the wider frame; the 2D UI stays left-justified at its 640-wide layout until Phase 5.

**Compile-time alternative (when you specifically want a different default):** build with an overridden `RESOLUTION_X`. Pass it to **both** the C and C++ flag sets — `SOURCES/INITADEL.C` is C and the rest of the engine is C++; overriding only one produces a build where the two halves disagree on the framebuffer size.

```bash
cmake -B build-wide -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-DRESOLUTION_X=768" \
    -DCMAKE_CXX_FLAGS="-DRESOLUTION_X=768"
cmake --build build-wide
./build-wide/SOURCES/lba2cc --game-dir /path/to/data
```

`RESOLUTION_X` must be a multiple of 8 (compile-time check in `LIB386/H/SYSTEM/RESOLUTION.H`).

Regenerate the wide projrec baseline against a wide build:

```bash
LBA2_BIN=build-wide/SOURCES/lba2cc \
    LBA2_GAME_DIR=/path/to/data \
    LBA2_PROJREC_WIDTH=768 \
    bash scripts/dev/regen_projrec_baselines.sh
```

`LBA2_PROJREC_WIDTH` must match the binary's compiled `RESOLUTION_X`; the same env var drives the test (`bash tests/automation/test_projection_corpus.sh`).

## Status

| Phase | State |
|-------|-------|
| 0 – research | Done ([WIDESCREEN_PROJECTION_AUDIT.md](WIDESCREEN_PROJECTION_AUDIT.md)) |
| 1 – projection capture | Done (#193–#198; corpus + demo baselines, see [tests/projection/](../tests/projection/)) |
| 2 – projection correction | Done (#199–#203; eight call sites routed through `ModeDesiredX/Y`) |
| 3 – widen the framebuffer | Done (#204; wide build verified, `corpus_768x480.projrec.hash` baseline committed) |
| 4 – fullscreen + runtime resolution | Done. Explicit player choice (`--resolution` / `lba2.cfg` / Display submenu / `resolution` verb) plus first-launch widescreen auto-detect; precedence CLI > cfg > auto > 640×480. Fullscreen defaults on via the cfg template. See [Phase 4](#phase-4-fullscreen-and-runtime-resolution). |
| 5 – UI (C2) | Done for the menu/UI surfaces (#213–#228 re-anchor campaign, plus #260, #289, #291, #310). HD-only concerns split out to Phase 7. |
| 6 – regression | Ongoing per-PR (projrec corpus, `host_quick`, and `ui_*` capture tests gate the 640×480 contract). No final lock-in pass yet. |
| 7 – native HD | Not started. Fonts blit 1:1, iso interior sharpness (experiment branches), holomap tall-res timeout, vertical-framing decision. See [Phase 7](#phase-7-native-hd). |

Foundation already in place: PR #134 (`RESOLUTION_X` / `RESOLUTION_Y` constants, and most render-space literals routed through them).
