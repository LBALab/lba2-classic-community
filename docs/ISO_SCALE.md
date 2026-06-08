# Iso view scale

A runtime scale for the isometric interior projection. It multiplies how large the
iso world is drawn on screen without touching world state, camera position, or game
logic. The default is 1.0, where the projection is bit-for-bit identical to the
original, so the faithful path and the ASM equivalence tests are unaffected.

This is the shared primitive behind three planned features. The difference between
them is only whether you also change the render buffer size:

- **scale only** gives a dynamic zoom (the framing changes, the model gets larger or
  smaller).
- **scale and buffer together, then downsample** gives supersampled anti-aliasing
  (same framing, more pixels, which removes the silhouette shimmer that the original
  integer rasterizer produces on slowly moving characters).
- **scale and buffer together at a high-resolution display** gives HD fidelity (same
  framing, more pixels on the model).

Only the first (a dev/experimental zoom) is wired up today. The rest are roadmap.

## What it touches

The iso projection has no scale argument of its own (`SetIsoProjection` takes only a
centre), so the scale lives in a single global, `IsoScale`, applied everywhere the
iso world is sized for the screen:

- Object geometry: `ProjectListIso` ([PRLIISO.CPP](../LIB386/3D/PRLIISO.CPP)), the
  batched vertex projector that draws character and object bodies.
- Single points: `LongProjectPointIso` ([LPROJISO.CPP](../LIB386/3D/LPROJISO.CPP)),
  used for object anchors, shadows, and bounding boxes.
- Brick floor and walls: `Map2Screen` ([GRILLE.CPP](../SOURCES/GRILLE.CPP)).
- Sphere primitives: the radius in `Sphere` / `Sphere_Transp`
  ([AFF_OBJ.CPP](../LIB386/OBJECT/AFF_OBJ.CPP)). Spheres (hands, joints, heads) carry
  a separate radius scalar, not just a projected centre. The first three places only
  move the centre, so without this the radii stay fixed and spheres appear to shrink
  as the body grows.

All four must scale by the same factor or the model comes apart (bodies detach from
the floor, hands shrink). The point projectors share the same visual scale in
different coordinate units (one brick step is 512 world units), so scaling all of
them by `IsoScale` keeps the model coherent.

`IsoScale` is fixed-point: `ISO_SCALE_ONE` (256) is 1.0, so 512 is 2.0 and 128 is 0.5.
At `ISO_SCALE_ONE` the multiply and shift cancel exactly (`(v * 256) >> (n + 8) == v >> n`),
which is why the default path is bit-exact. The constant and the global are declared in
[CAMERA.H](../LIB386/H/3D/CAMERA.H); set it through `SetIsoScale()`, which clamps to a
1/16x to 16x range.

Only iso interior scenes are affected. Exterior island scenes use the perspective
projection (`LongProjectPoint3D`, with its own `FRatioX` focal), so `IsoScale` is a
no-op there.

## Lineage

This restores a capability the LBA1 engine had and LBA2 dropped. In LBA1,
`SetIsoProjection` took a third `scale` argument that set a global `IsoScale` (the iso
projection divided by it; gameplay passed `SIZE_BRICK_XZ` = 512). LBA2 removed the
parameter and hardcoded the scale (the `/512` here is that value baked in), so this is
re-adding a knob the original engine evolution removed, not inventing a new one. Two
deliberate differences from LBA1:

- It lives in a separate global, not a third `SetIsoProjection` argument. Re-adding the
  argument would diverge from LBA2's original 2-argument ASM and break the
  `test_proj` equivalence test, so the scale is kept out of that signature.
- LBA1's `IsoScale` was a divisor (larger = smaller image). This one is a fixed-point
  multiplier (`ISO_SCALE_ONE` = 1.0, larger = zoom in), because the reimplemented iso
  formula uses shifts rather than a divide.

## Trying it

In the console:

```
iso scale 2      # zoom the iso interior to 2x
iso scale 1.5
iso reset        # back to 1.0
iso              # print the current scale
```

Headless, for before and after captures (the screenshot path used during development):

```
lba2cc --game-dir <data> --load "<save>" --no-audio --fixed-dt 16 \
       --exec "iso scale 2" --tick 40 --screenshot out_2x.png --exit
```

Load an interior save (for example the game-start save, Twinsen's house) to see an
effect; an exterior save will not change.

## Known limitation: bricks do not stretch yet

Bricks are fixed-resolution sprite tiles, blitted at native size by `AffBrickBlock`
(via `AffGraph`), and the brick pipeline keys off a hardcoded 24px grid (clipping,
the `(XScreen+24)/24` column bucketing, the `DrawOverBrick` re-blit). `Map2Screen`
scales the brick *positions*, so the floor layout stays correct, but the tiles are
still drawn at their original size. At scales above 1.0 this leaves gaps between
floor bricks; below 1.0 the tiles overlap.

So today the scale is coherent for geometry (characters stay on the floor) but the
brick environment cannot gain detail beyond its source art. This is expected and is
the next thing to solve before the zoom is presentable.

The brick floor is also a cached background: it is re-laid only on a full redraw
(`RefreshGrille` runs in `AffScene`'s `AFF_ALL_*` path, not the per-frame
`AFF_OBJETS_*` path). So changing the scale mid-scene re-scales only the per-frame
3D objects until a redraw happens. The `iso` console command forces one
(`FirstTime = AFF_ALL_FLIP`) so the floor re-lays at the new scale immediately. A
future dynamic zoom that changes the scale every frame would have to re-lay the
bricks every frame, which is one more reason the render-at-scale-and-sample approach
(which sidesteps brick re-lay entirely) is the likely path for smooth zoom.

## Roadmap

1. **Brick scaling strategy.** Either stretch the brick blit (and rework the 24px
   grid assumptions), or render the iso scene at a fixed higher scale and sample the
   buffer for intermediate zoom (which avoids fractional geometry and the brick-grid
   surgery, at the cost of capping maximum sharpness). The second path is the likely
   choice for smooth dynamic zoom.
2. **Dynamic zoom.** Drive `IsoScale` over time and integrate with the iso follow
   camera so the view stays centred as the visible window shrinks. Zoom is a pure
   view transform: collision is world-space (`WorldColBrick`) and is not affected,
   which is what keeps it safe for determinism.
3. **Supersampling for the silhouette shimmer.** Render the scene to a buffer scaled
   by S with `IsoScale` scaled by S, then box-downsample to the display. The
   downsample is what removes the flicker (it anti-aliases the edges). The UI is
   4:3 display-anchored and must be composited at display resolution after the
   downsample, not supersampled with the scene.

## Constraints

- The default (1.0) must stay bit-exact. The three projectors are covered by ASM
  equivalence tests ([tests/3D/test_lprojiso.cpp](../tests/3D/test_lprojiso.cpp),
  [tests/3D/test_prliiso.cpp](../tests/3D/test_prliiso.cpp),
  [tests/test_grille.cpp](../tests/test_grille.cpp)); any change here keeps scale 1.0
  matching the original ASM.
- Non-default scales change projected coordinates, so they change projection-record
  hashes and are not a faithful-path mode. Keep the scale at 1.0 for regression runs.
- The console command is a dev knob. There is no keybinding or persisted setting yet,
  and opening a menu after setting a scale may inherit it (the menus also use the iso
  projection). Scoping the scale to the gameplay view is part of wiring up a real
  zoom feature.
