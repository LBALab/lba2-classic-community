# Iso view scale: implementation findings

Notes from making the whole isometric interior view scale by a runtime factor
(`iso scale 2` in the console). This is a record of what the work taught us about
the interior render pipeline, not a description of a finished feature. Treat it as
exploratory: the knob is a dev tool, and whether any of it ships is undecided.

For the underlying primitive (the `IsoScale` global, `ISO_PX`, lineage to LBA1)
see [ISO_SCALE.md](ISO_SCALE.md). For the pre-implementation survey of hardcoded
iso-space pixel assumptions see ISO_SPACE_AUDIT.md. This doc is the
post-implementation companion: what actually broke, why, and how each piece was
fixed and checked.

## Status

- Experimental dev knob only. `iso scale <f>` / `iso reset` in the console. Not
  keybound, not persisted, not integrated with the follow camera.
- Bit-exact at scale 1.0. Every change below collapses to the original code when
  `IsoScale == ISO_SCALE_ONE` (the `ISO_PX(n)` macro is the identity there), so the
  faithful path and the ASM equivalence tests are untouched.
- Coherent at a static scale: at `iso scale 2` the floor, walls, characters,
  doors, items, and particles all grow together and stay aligned.
- Not done: smooth dynamic zoom (driving the scale per frame and keeping the view
  centred), supersampling, an HD render buffer, scoping the scale away from the
  UI, and any performance work. See "What it would take to ship".

## The main lesson: an interior scene is drawn by several independent render classes

The single biggest finding is that there is no one "draw the iso scene" path to
scale. An interior frame is assembled from separate subsystems, each with its own
projection, its own sizing, and its own anchor convention. Scaling the view means
finding and scaling every one of them, and they do not share code. Missing one
leaves that class native-size while everything else grows, which is how each gap
was discovered (the model floated off the floor, the door looked open, the fan was
tiny, the item drops did not grow).

| Class | What it draws | Path | How it is scaled | Checked with |
| --- | --- | --- | --- | --- |
| Bricks | Floor, walls, architecture | `Map2Screen` + `AffGraph`/`ScaleSprite` ([GRILLE.CPP](../SOURCES/GRILLE.CPP)) | `Map2Screen` positions via `ISO_PX`; `BlitBrickIso` decodes the RLE tile and `ScaleSprite`s it under zoom (native `AffGraph` at 1.0) | static captures |
| Foreground re-blit | Repaints foreground bricks over moving objects | `DrawOverBrick` to `CopyMask` / `ScaledReBlitFromScreen` ([GRILLE.CPP](../SOURCES/GRILLE.CPP)) | copy the composited `Screen` masked by the brick silhouette, at the scaled footprint | door-behind-railing scene |
| 3D models | Characters, dart and projectile bodies, impact effects | `ProjectListIso` / `LongProjectPointIso` ([PRLIISO.CPP](../LIB386/3D/PRLIISO.CPP), [LPROJISO.CPP](../LIB386/3D/LPROJISO.CPP)) | `IsoScale` inside the projectors, plus sphere radius in `Sphere`/`Sphere_Transp` | user-confirmed, projector probe |
| `TYPE_OBJ_ANIM_3DS` sprites | Fans, vents, animated sprite decor | `AffGraph` via `BlitAnim3DSIso` ([OBJECT.CPP](../SOURCES/OBJECT.CPP), [GRILLE.CPP](../SOURCES/GRILLE.CPP)) | decode the RLE sprite, `ScaleSprite` it; `ISO_PX` on the ZV hotspot | EM2.LBA fan, isolation diff |
| `TYPE_OBJ_SPRITE` sprites | Doors, goodie sprites | `PtrAffGraph` via `BlitSpriteIso` ([OBJECT.CPP](../SOURCES/OBJECT.CPP)) | scaled only at the gameplay call site; `ISO_PX` on hotspot and the `SPRITE_CLIP` window | current.lba door, isolation diff |
| `TYPE_EXTRA` sprites | Magic ball, item drops, flowers, effects | `CalculeScaleFactorSprite` + `ScaleSprite` ([OBJECT.CPP](../SOURCES/OBJECT.CPP), [EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP)) | `ISO_PX` on hotspot; multiply `ScaleFactorSprite` by `IsoScale` only on the fixed-scale branch | Francos.LBA flowers, isolation diff |
| Particle flows | Fountains, fire, magic streams | `BoxFlow` dots via `BoxFlowIso` ([FLOW.CPP](../SOURCES/FLOW.CPP)) | scale the dot size to `ISO_PX(2)` and re-centre it | Dark Monk.LBA stream, isolation diff |

A separate placement fix sits underneath all of this (below).

## Two projection insights

**The pivot has to scale, not just the step.** A projector maps the world to the
screen as `origin + step * worldOffset`. The per-brick step already scaled
correctly once `LongProjectPointIso` and `Map2Screen` were scaled (a probe showed
both moving 24px per brick at 1.0 and 48px at 2.0). But the two grids zoom about
different fixed points: the object grid pivots at the camera point (`ModeDesiredX/2
- 9`) and the brick grid at the cube corner (`ModeDesiredX/2 - 32`), a 23px gap in
X and 15px in Y. Those offsets did not scale, so objects and bricks drifted apart
by about 23px per scale step (the "model is a bit off the tile" symptom). The fix
derives the iso centre from the brick pivot plus the scaled object-to-brick offset
in `Init3DView`, re-applied on a scale change ([GAMEMENU.CPP](../SOURCES/GAMEMENU.CPP),
[CONSOLE_CMD.CPP](../SOURCES/CONSOLE/CONSOLE_CMD.CPP)). At 1.0 it equals the
original `(ModeDesiredX/2 - 9, ModeDesiredY/2)`.

**Projection-derived sizes scale for free.** `CalculeScaleFactorSprite` sizes a
sprite by projecting two points 1000 world units apart and measuring the screen
gap. Because that measurement goes through the now-scaled `LongProjectPoint`, any
sprite using it (the `Scale > 0` branch: many effects) already grows with the zoom.
Only the fixed branch (`CubeMode == CUBE_INTERIEUR && Scale == -1`, which returns a
constant `DEF_SCALE_FACTOR`) needed an explicit multiply. The corollary bites: you
must not multiply the projection-derived branch as well, or it double-scales to 4x
at 2x.

## Bugs found and fixes

1. **Screenshots captured `Screen`, not `Log`.** `Log` is the buffer presented to
   the display; `Screen` is a clean background copy used for re-blits. The
   screenshot path saved `Screen`, so 3D models never appeared in captures. This
   silently invalidated headless validation for most of the session. Fixed
   (separate change, PR #313): capture `Log`. Lesson: confirm the capture path
   shows what the display shows before trusting any A/B.

2. **The scaled re-blit re-drew a single brick instead of copying the composite.**
   The original `CopyMask` copies the composited `Screen` (where overlapping bricks
   already resolved front to back) over a moving object. The first scaled version
   re-drew one decoded brick, which loses the compositing, so where bricks overlap
   the wrong tile showed through. `ScaledReBlitFromScreen` mirrors `CopyMask`: copy
   `Screen` masked by the brick silhouette, at the scaled footprint.

3. **Object-to-brick placement drift.** The pivot issue above.

4. **Three sprite paths blit native.** Doors, fans, and extras are three different
   subsystems, none of which used the model projector, so each stayed native size.
   The door path (`PtrAffGraph`) is shared with the HUD, inventory, and menus, so
   the fix is at the gameplay call site only, never in the shared primitive.

5. **Particle dots: fixed size and off-centre growth.** Each dot is a fixed 2x2
   block (`BoxFlow`). With only the positions scaled, the cloud spread doubled
   while dots stayed 2x2, so effects thinned out. Scaling the dot to `ISO_PX(2)`
   fixes the size; because `BoxFlow` anchors the block at its top-left, the grown
   dot also has to be offset by `(n-2)/2` to keep its centre on the particle, or
   the whole cloud creeps down-right at zoom.

## Methodology and gotchas

- **The harness mutates the save you load.** `--load <save> --tick N` autosaves to
  the live slot (`current.lba`, `autosave.lba`) as ticks advance, so the file you
  loaded changes between runs and A/B diffs show whole-frame differences that are
  really the game state advancing. Work from a copy: `cp current.lba /tmp/x.lba`
  and load that. Autosave still writes the real slot, leaving the copy stable.
- **Finding a scene with a given render class.** The `cube N` console verb warps
  within the current island; named saves in the save dir cover other locations.
  Concretely: EM2.LBA has a `SPRITE_3D` fan, current.lba (Twinsen's house) has a
  `TYPE_OBJ_SPRITE` door, Francos.LBA has `Scale == -1` flower extras, Dark Monk.LBA
  has particle flows. Island 0 (the house) has no anim sprites at all, which is why
  the first door search came up empty.
- **Isolation diff to verify a fix.** For each class, capture the scene with the
  fix and again with the scaled branch forced back to native, then diff. A non-zero
  diff localised to the object proves the fix is what changed it (and nothing
  else); a zero diff means the class is not present in that scene, not that the fix
  works. Force-native via a temporary `if (1)` on the scale-1 gate.
- **ASM-tested primitives stay untouched; scale at the call site.** `BoxFlow`
  (FLOW_A), `Map2Screen`, `LongProjectPointIso`, and `ProjectListIso` are covered
  by ASM equivalence tests. Where the primitive is shared or tested, add a scaled
  wrapper used only by the gameplay caller (`BoxFlowIso`, `BlitSpriteIso`) rather
  than editing the primitive.
- **The bit-exact-at-1.0 invariant is the safety net.** Every wrapper gates to the
  original call at `IsoScale == ISO_SCALE_ONE`, and every offset uses `ISO_PX`,
  which is the identity at 1.0. That is what keeps the default path and the tests
  unaffected, and it makes "did the default change" a one-line check (diff a
  scale-1 capture against a known good one).

## What is verified, and what is not

Verified by static capture and isolation diff: bricks, the foreground re-blit, 3D
model scaling, the fan, the door, item-style extras (flowers), particle dot size
and centring, and the placement pivot (via the projector probe).

Not verified, because they are transient and cannot be triggered headlessly: the
magic ball in flight, an item dropping, a door opening and closing (the
`SPRITE_CLIP` sliding window is scaled but the animation was not driven), and live
particle bursts. These need an in-game pass at `iso scale 2`.

## What it would take to ship

- **Scope the scale to the gameplay view.** `IsoScale` is a global. The sprite
  fixes are at gameplay call sites so the HUD and inventory are safe today, but if a
  menu re-uses the iso projection while a scale is set, it would inherit it. A real
  feature would save and restore the scale around UI, or carry it as view state
  rather than a global.
- **Dynamic zoom.** Drive `IsoScale` over time and keep the iso follow camera
  centred as the visible window shrinks. The brick floor is a cached background
  re-laid only on a full redraw, so per-frame zoom re-lays bricks every frame; the
  render-at-scale-and-sample approach sidesteps that and is the likely path.
- **Fractional and below-1.0 scales.** Everything here was exercised at integer
  scales (2x, 3x). Nearest-neighbour tile and dot scaling will look blocky, and
  odd `ISO_PX` results introduce sub-pixel centring error. Smooth zoom probably
  wants the sample-the-buffer approach rather than per-element fractional scaling.
- **Performance.** Untested. Decoding and `ScaleSprite`-ing bricks and sprites per
  frame is more work than the native blit; the cached-background design assumes the
  floor is laid once.
- **Determinism.** A non-1.0 scale changes projected coordinates, so it changes
  projection-record hashes. It is a view transform (collision is world-space and
  unaffected), but it is not a faithful-path mode. Keep regression runs at 1.0.
