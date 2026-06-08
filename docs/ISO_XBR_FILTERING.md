# Iso brick filtering (xBR / EPX): experiment notes

Experimental. Off by default. Branch `feat/iso-xbr-bricks`, built on the iso
view-scale foundation (see [ISO_SCALE_FINDINGS.md](ISO_SCALE_FINDINGS.md)). This
note records how it works and what was tried, so the work can be picked up later.

## What it does

At an iso zoom (`iso scale > 1`) the brick background is blocky: the tiles are
nearest-stretched. This filters that background with an edge-directed upscaler
(EPX-blend or xBR) and composites the result over the unscaled, crisp 3D models,
so the floor/walls smooth out while the characters and objects stay sharp.

Console toggle (interior only):

```
iso scale 2     # zoom first (the filter keys off the zoom)
iso epx 0       # off (nearest bricks)
iso epx 1       # EPX-blend (cheap, mild)
iso epx 2       # xBR (edge-directed)
iso epx 3       # xBR supersampled (SSAA, smoothest)
```

## Pipeline

The filter runs as a scene-level present callback, not in the engine draw path:

1. **Recover the native bricks.** `Screen` holds the brick background already
   drawn at the zoom (nearest-stretched), so downscaling it by the zoom factor
   recovers the original native-resolution composed bricks. This avoids a second
   `AffGrille` pass, which corrupts shared render state and makes the moving
   objects draw black.
2. **Filter** the recovered native image back up to 2x native with EPX-blend or
   xBR (the *composed* image, so there are no per-tile transparent borders and
   therefore no seams between tiles), then bilinear-resample that 2x-native layer
   to the display. At zoom == 2 the 2x-native layer already is the display, so the
   resample is skipped and the filter writes straight into the composite buffer
   (zoom 2 stays bit-identical to the original 2x-only path).
3. **Composite at present time** (`IsoXbrPresent`, a new `SetScenePresentCallback`
   slot that runs under the console/touch overlays). For each finished-frame
   pixel: where `Log == Screen` it is a brick (no model on top) and takes the
   filtered RGB layer; otherwise it is a model pixel and is re-converted from
   `Log` through `PtrPal`.

The composite is at present time (not in `Log`) because an interpolating filter
makes new colours the 8-bit palette cannot hold, so the layer is RGB.

## The palette gotcha (important)

Going RGB-at-present exposed a real, pre-existing engine quirk that will bite
anything else that does the same.

The SDL present palette (`paletteLUT` in `LIB386/SVGA/SDL.CPP`) is **missing the
216-255 range** in iso scenes: those entries are black, while the game palette
`PtrPal` holds the real colours (a shading ramp plus specials, e.g. index 246 =
`(87,87,255)`, the ghost's blue). The screen palette only updates via
`PaletteSync(...)` and the high range simply is not pushed in these scenes.

Consequence: any object using a high-range index presents **black** even though
it is correct in `Log`. Screenshots hide this entirely because `SavePNG` uses
`PtrPal` directly, not `paletteLUT`. That is why the bricks looked fine but the
ghost and Twinsen went black, and why headless `--screenshot` could not see it.

Fix in the composite: re-convert **model** pixels from `Log` through `PtrPal`
(built into `s_palLut`) instead of leaving the stale `paletteLUT` value, so the
whole iso view presents from the authoritative palette.

## Diagnosing present-time effects headlessly

`--screenshot` saves `Log` (8-bit, pre-composite), so it cannot see a present-time
composite. The dummy video driver *does* run the present path, so a temporary PPM
dump of the staging buffer inside the present callback works for headless A/B (the
staging dump can be flaky though, so treat it as advisory and confirm live). The
palette diff that found the bug compared the callback's `pal_rgb` argument (the SDL
palette) against `PtrPal` directly.

## Optimisation (preserved on `feat/iso-xbr-optimized`)

The proof-of-concept rebuilds the layer on every full redraw. That was slow at the
higher filter levels, so a perf pass was done and then set aside (re-optimise once
the design settles). It lives on branch **`feat/iso-xbr-optimized`** (commits
`441cf265`, `b574930b`, `ce64f987` on top of this one) and covers:

- **Skip-when-unchanged.** The layer is a pure function of `Screen` + dims + mode;
  hash those and reuse the cached layer otherwise. Idle scenes rebuild zero times.
- **Luma-only edge distance.** Bricks are low-chroma, so dropping U/V from the xBR
  metric costs nothing visible and cuts the hot path (~40 calls/pixel) to a third.
- **Threaded filter.** Each thread owns a disjoint band of output rows (read-only
  source, disjoint writes, no locks), via `SDL_CreateThread` like `AIL/SDL/STREAM`.
  Race-free and deterministic.
- **LRU layer cache.** Keep the last N built layers keyed by the `Screen`+mode
  hash, so snapping back to an already-built camera view is a cache hit (no
  rebuild). The camera snaps between fixed iso views, so revisits are common.
- **Rebuild-cost log** on cache miss, to measure the filter cost against the
  game's own camera-transition delay.

Measured rebuild cost at 1280x960 (one camera view), before and after:

| mode | before | after (wall) |
|------|--------|--------------|
| 1 EPX-blend | 10 ms | 11 ms |
| 2 xBR | 311 ms | 61 ms |
| 3 xBR SSAA | 1470 ms | 189 ms |

Memory: ~5 MB per built layer; SSAA adds ~25 MB of shared scratch. The LRU cache
multiplies the layer cost by its slot count (lazy, grows only with views visited).

To resume the optimisation: `git cherry-pick 441cf265 b574930b ce64f987` (or rebase
that branch) once the scale generalisation below is settled.

## Findings / gotchas worth keeping

- The bricks turned out to be **brick decor**, not objects, so they live in
  `Screen` and the downscale-recovery sees them. Moving entities (Twinsen, the
  ghost) are `Log != Screen` and stay crisp.
- The camera **snaps** between fixed views with its own transition delay, so the
  filter rebuild lands inside an existing pause rather than adding a new hitch.
  Walking within a room does not re-lay bricks (verified: one build over 300 idle
  ticks), so steady state is just the composite.
- xBR and its YUV edge metric here are written from the published algorithm, not
  ported, so there is no licensing entanglement.

## Known limits / future

- SSAA is the smoothest but heaviest; the per-cut cost is hidden in the camera
  transition but is the first thing to profile if cuts feel slow.
- The brick layer detail is capped at 2x of native, so very high zoom stretches it
  (a deeper supersample or per-scale filter level would address that).
- An async rebuild (present nearest immediately, swap the filtered layer in a few
  frames later) would fully decouple the filter from the camera-cut feel.
