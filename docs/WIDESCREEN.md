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

The product goal the phases build toward:

- Windowed mode keeps the original 4:3 behaviour. Nothing changes for windowed players.
- Fullscreen detects the display's aspect ratio automatically and renders to match. There is no player-facing aspect or resolution setting.
- 16:9 is the target. A display wider than 16:9 renders at 16:9 and pillarboxes the rest; the engine does not render arbitrarily wide.

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

### Phase 4 — SDL fullscreen detection

Wire the [target behaviour](#target-behaviour) into the present and window layer (`LIB386/SYSTEM/WINDOW.CPP`, `LIB386/SVGA/SDL.CPP`). Windowed stays 4:3; fullscreen reads the display aspect ratio and sets the render width to match, capped at 16:9 with pillarboxing beyond. No player-facing setting.

#### Runtime resolution switching

A separate strand of work running parallel to (and reusing) the Phase 4 plumbing: let the player change render resolution from a console verb or a Display options submenu, without relaunching. **Phase 0 shipped** (PRs #237–#241, #244) — `resolution` console verb, keep/revert dialog, `lba2.cfg` persistence. The Display submenu (Phase 2) is the remaining surface for menu-driven discovery. See [`RUNTIME_RESOLUTION.md`](RUNTIME_RESOLUTION.md) for the design, phasing, and risks.

### Phase 5 — polish

The deferred, larger surface:

- UI strategy — pick C1 or C2 (see [UI strategy](#ui-strategy-c1-vs-c2) below) and apply it so the 2D UI is correct in the wider frame.
- HD-only concerns, if native high resolution is later pursued: filler precision (the `LIB386/pol_work/` fillers use 16-bit fixed-point step accumulation calibrated to 640-wide spans — the polyrec harness is the tool for measuring drift), bitmap fonts (`LIB386/SVGA/FONT.CPP` / `AFFSTR.CPP` blit 1:1), the 16-bit Z-buffer and `Fill_ZBuffer_Factor` (tuned for the 4:3 frustum), mouse-coordinate inversion against any UI scale, and a Smacker cutscene upscale policy.

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
| Holomap at Y > 480 | A4/A5 done, but `ui holoplan` at `1024×768` still times out — a cinema-mode rendering bug independent of the Z-buffer bounds and the planet bitmap centring. The A5 fix is correct and 640-identical but **not yet verified at tall res** until this timeout is resolved. Add `test_ui_holoplan_768x768.sh` once unblocked. |
| Filler precision | **Swept at 1920 — de-risked** (2026-05-29). Captured a 6-scene polyrec corpus at 1920-wide and replayed it under ASan+UBSan and ASM-vs-CPP. No overflow, no out-of-bounds across ~24k full-width draw calls (the feared accumulator overflow does not occur). ASM↔CPP equivalence degrades from bit-exact (640) to **118/1,966,080 pixels = 0.006%** (1–2px edge rounding on a few triangles through the FPU slope path — the long-double/x87 precision class, not overflow); cosmetically invisible. Practical consequence: the 640 ASM-equivalence corpus can't be extended to 1920 without tolerancing or pinning FP, but the fillers render correctly at HD width. |
| Bitmap fonts | `LIB386/SVGA/FONT.CPP` / `AFFSTR.CPP` blit at 1:1 pixel size. At 1920×1080 in-game text is tiny. Either pixel-scale the existing 8×N glyphs (cheap, blocky) or re-author a higher-DPI font set (slow, clean). |
| Z-buffer precision | 16-bit Z + `Fill_ZBuffer_Factor` is tuned for the 4:3 frustum's depth range. May Z-fight at HD aspect ratios on distant geometry; the polyrec harness catches drift here too if you log Z values. |
| Smacker upscale policy | Source is 320×200. At HD widths the existing centred-letterbox path (PR #226) preserves the cinematic feel with bigger sidebars. Alternative is to fill the frame (uglier, lossy); the current default is the right one. |
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

#### Testing posture, honestly

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
| 0 — research | Done ([WIDESCREEN_PROJECTION_AUDIT.md](WIDESCREEN_PROJECTION_AUDIT.md)) |
| 1 — projection capture | Done (#193–#198; corpus + demo baselines, see [tests/projection/](../tests/projection/)) |
| 2 — projection correction | Done (#199–#203; eight call sites routed through `ModeDesiredX/Y`) |
| 3 — widen the framebuffer | Done (#204; wide build verified, `corpus_768x480.projrec.hash` baseline committed) |
| 4 — SDL fullscreen detection | Not started |
| 5 — polish (UI + HD) | Not started |
| 6 — regression | Not started |

Foundation already in place: PR #134 (`RESOLUTION_X` / `RESOLUTION_Y` constants, and most render-space literals routed through them).

The HD and zoom side of Phase 5 starts with a runtime iso view scale (`IsoScale`), the shared primitive for dynamic zoom, supersampled anti-aliasing, and HD model fidelity in interior scenes. See [ISO_SCALE.md](ISO_SCALE.md).
