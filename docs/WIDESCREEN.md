# Widescreen and higher-resolution support

The engine was authored for a fixed 4:3 framebuffer (640×480, 8-bit paletted). SDL scales and letterboxes that buffer to the window. This doc is the working plan for changing that — widescreen aspect ratios first, higher render resolutions later — and a record of what has been done so far.

It supplements [issue #45](https://github.com/LBALab/lba2-classic-community/issues/45). The issue states the goal and a rough A/B/C phasing; this doc adds the architectural detail that informs *how* the phases should be cut.

## The core idea: two coordinate spaces

Most of the difficulty in this work comes from one fact: the 640×480 number means two unrelated things, and they have to be separated before anything can move.

| Space | What it is | Where the literals live |
|-------|-----------|-------------------------|
| **Render space** | The framebuffer the engine draws into — `ModeDesiredX × ModeDesiredY`. 3D projection, the sort tree, cinema bars, fullscreen clears, buffer allocations. | A small number of sites, mostly in `SOURCES/SORT.CPP`, `SOURCES/OBJECT.CPP`, `SOURCES/MEM.CPP`, `SOURCES/INITADEL.C`. |
| **UI space** | The authored 2D layout — menus, inventory, holomap, dialogue, save screens, the HUD, pre-rendered HQR backgrounds, bitmap fonts. Pixel-positioned against a 640×480 canvas. | ~150 literal `320` / `639` / `240` sites across ~20 files in `SOURCES/`. |

Render space wants to *grow* with the output. UI space was *hand-tuned* to 640×480 and either has to be re-centered, re-scaled, or left alone — that is a deliberate design choice, not a mechanical edit.

A change that widens the framebuffer without addressing UI space produces a working 3D view with the 2D UI left-justified and garbage in the margin. That is expected and is the boundary between the phases below.

## Phases

The work splits into five phases. A, B, and D are independently shippable. C is a design fork. E only applies if native HD is a goal.

### Phase A — present-layer scaling

User-facing options for how the existing framebuffer is shown: pillarbox, integer scale, stretch. Lives entirely in the SDL present path (`LIB386/SYSTEM/WINDOW.CPP`, `LIB386/SVGA/SDL.CPP`). No game code changes, no gameplay impact. This alone covers most "it looks wrong on my display" complaints, including Steam Deck, at native 480 height.

### Phase B — decouple resolution from the engine

Name the render-space dimensions and route the hardcoded literals through them, so the framebuffer size is no longer baked in. This is a behaviour-preserving refactor at the default 640×480.

Status: **done** (`refactor/resolution-constants`, [PR #134](https://github.com/LBALab/lba2-classic-community/pull/134)). It adds `LIB386/H/SYSTEM/RESOLUTION.H` with `#ifndef`-guarded `RESOLUTION_X` / `RESOLUTION_Y`, routes the compile-time buffer allocations and the runtime render-edge literals (sort preclip, cinema bars, logo anchor) through the constants and `ModeDesiredX` / `ModeDesiredY`. See [How to test a wider build](#how-to-test-a-wider-build).

### Phase C — UI strategy

This is the large phase and it has two mutually exclusive flavours. Pick based on whether higher resolution (not just widescreen) is a goal.

| Flavour | Approach | Generalises to HD? | Cost |
|---------|----------|--------------------|------|
| **C1 — centered UI** | Introduce a `UI_X0` offset; replace UI-space `320` / `639` literals with offset-aware names. The UI keeps its 640×480 layout, centered in the wider canvas. | No — at 1080p the UI becomes a small box in the middle. | ~150 mechanical edits, reversible, easy to review. |
| **C2 — scaled UI** | Render UI into its own 640×480 logical buffer, scale-blit it to the framebuffer at present time. UI literals stay untouched. | Yes — works at any output resolution. | More plumbing in `LIB386/SVGA/` to thread the target-buffer choice through the draw primitives. |

C1 is the cheaper path if widescreen-at-480 is the only target. C2 is the correct path if the engine should run at arbitrary resolutions. Committing to C1 first is a dead end for HD.

### Phase D — expose the knob

Make `RESOLUTION_X` (and the Phase C UI behaviour) a build option or runtime setting with a faithful 640×480 default. Document it in [CONFIG.md](CONFIG.md).

### Phase E — native HD work

Only required if the 3D world should render at native high resolution rather than being upscaled from 640×480:

- **Filler precision.** The `LIB386/pol_work/` fillers use 16-bit fixed-point step accumulation calibrated to 640-wide spans. Longer spans at HD may show texture wobble or edge bleeding. The polyrec equivalence harness in `tests/` is the tool for measuring this; it has not been run at HD.
- **Fonts.** `LIB386/SVGA/FONT.CPP` / `AFFSTR.CPP` blit a fixed-size bitmap font 1:1. HD needs a scaler or replacement glyphs.
- **Z-buffer.** 16-bit Z and `Fill_ZBuffer_Factor` were tuned for the 4:3 frustum; HD may introduce z-fighting.
- **Mouse coordinates.** Input is compared against UI rects; any UI scale must be inverted on mouse input.
- **Pre-rendered video.** Smacker cutscenes are fixed-dimension; decide an upscale policy.

## What does not need to change

- **The assembly.** Every `.ASM` source is commented out in the active `CMakeLists.txt` files; the shipping binary is 100% C++ on every platform. The C++ blit and filler path in `LIB386/SVGA/` and `LIB386/pol_work/` already uses `ModeDesiredX` for stride. `SOURCES/COPY.CPP` is a dead reference file (entirely inside a comment block), and `SIZE_VIDEOLINE` in `LIB386/H/FILLER.H` is an ASM-only equate. The only assembly that matters is in `tests/`, for equivalence checking.
- **The save format.** `MAX_PLAYER` (`SOURCES/DEFINES.H`) is intentionally kept keyed to the literal `640*480` rather than the render resolution — it partitions the `BufSpeak` buffer, and a silent slot-count change would walk off the end of it. Widening the framebuffer must not change the save-slot count.

## The `feat/widescreen-deck` branch

There is an existing branch, `origin/feat/widescreen-deck`, that adds Steam Deck widescreen. It is **not a viable base** to build on:

- It has no merge base with `main` (the repo was re-rooted after it forked).
- It hardcodes the Steam Deck's 768×480 ratio everywhere as literal `+64` / `767` / `384` — no constants, no toggle.
- It mixes in unrelated changes that look like search-and-replace overreach: a `-0x8000 → 0x8000` projection-sentinel flip in `LIB386/OBJECT/AFF_OBJ.CPP` (which would stop off-screen geometry being culled), and a `RealShifting` change in `SOURCES/PERSO.CPP` (conveyor-belt speed, a world-space value, not a pixel value).

It is still useful as a **call-site map** — its diff touches most of the render-space and UI-space sites the phases above have to address. Re-implement against current `main` using the constants, do not rebase the branch.

## How to test a wider build

With Phase B in place, build with an overridden `RESOLUTION_X`. Pass it to **both** the C and C++ flag sets — `SOURCES/INITADEL.C` is C and the rest of the engine is C++; overriding only one produces a build where the two halves disagree on the framebuffer size.

```bash
cmake -B build-wide -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-DRESOLUTION_X=768" \
    -DCMAKE_CXX_FLAGS="-DRESOLUTION_X=768"
cmake --build build-wide
./build-wide/SOURCES/lba2cc --game-dir /path/to/data
```

Expected result with only Phase B done: the 3D world and cinema bars render across the full width, the sort tree preclips correctly, and the 2D UI is left-justified at its original 640-wide layout with uninitialised content in the right margin. That is the visible boundary between Phase B and Phase C — the engine is structurally sound at the wider size, the UI layer has simply not been addressed yet.

## Status

| Phase | State |
|-------|-------|
| A — present-layer scaling | Not started |
| B — decouple resolution | Done (PR #134) |
| C — UI strategy (C1 or C2) | Not started; design fork undecided |
| D — expose the knob | Not started |
| E — native HD work | Not started |
