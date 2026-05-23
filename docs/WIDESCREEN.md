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

### Phase 3 — widen the framebuffer

With projection corrected, build and run at a wider render width. The mechanics already exist from PR #134 — see [How to test a wider build](#how-to-test-a-wider-build). The 3D world now centres and fills the wider frame, bricks no longer clip, and the sort preclip is fed correct coordinates. The 2D UI is still 640-anchored (Phase 5). This is the first point at which the wide render path is genuinely correct.

### Phase 4 — SDL fullscreen detection

Wire the [target behaviour](#target-behaviour) into the present and window layer (`LIB386/SYSTEM/WINDOW.CPP`, `LIB386/SVGA/SDL.CPP`). Windowed stays 4:3; fullscreen reads the display aspect ratio and sets the render width to match, capped at 16:9 with pillarboxing beyond. No player-facing setting.

### Phase 5 — polish

The deferred, larger surface:

- UI strategy — pick C1 or C2 (see [UI strategy](#ui-strategy-c1-vs-c2) below) and apply it so the 2D UI is correct in the wider frame.
- HD-only concerns, if native high resolution is later pursued: filler precision (the `LIB386/pol_work/` fillers use 16-bit fixed-point step accumulation calibrated to 640-wide spans — the polyrec harness is the tool for measuring drift), bitmap fonts (`LIB386/SVGA/FONT.CPP` / `AFFSTR.CPP` blit 1:1), the 16-bit Z-buffer and `Fill_ZBuffer_Factor` (tuned for the 4:3 frustum), mouse-coordinate inversion against any UI scale, and a Smacker cutscene upscale policy.

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

With the resolution constants from PR #134 in place, build with an overridden `RESOLUTION_X`. Pass it to **both** the C and C++ flag sets — `SOURCES/INITADEL.C` is C and the rest of the engine is C++; overriding only one produces a build where the two halves disagree on the framebuffer size.

```bash
cmake -B build-wide -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-DRESOLUTION_X=768" \
    -DCMAKE_CXX_FLAGS="-DRESOLUTION_X=768"
cmake --build build-wide
./build-wide/SOURCES/lba2cc --game-dir /path/to/data
```

Before Phase 2, this build shows the mis-centred world the audit describes: cinema bars and the sort preclip track the wider edge, but the projection is still anchored at `320` and the bricks clip at x=640, so the 3D view sits left of centre. After Phase 2 the world centres and fills the frame; the 2D UI stays left-justified at its 640-wide layout until Phase 5.

## Status

| Phase | State |
|-------|-------|
| 0 — research | Done (WIDESCREEN_PROJECTION_AUDIT.md) |
| 1 — extend polyrec | Not started |
| 2 — projection correction | Not started |
| 3 — widen the framebuffer | Not started |
| 4 — SDL fullscreen detection | Not started |
| 5 — polish (UI + HD) | Not started |
| 6 — regression | Not started |

Foundation already in place: PR #134 (`RESOLUTION_X` / `RESOLUTION_Y` constants, and most render-space literals routed through them).
