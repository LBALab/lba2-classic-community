# Isometric space audit: hardcoded pixel assumptions

A survey of every place the isometric (interior) renderer bakes in a fixed
pixel scale. The iso projection draws the world at one fixed "native" scale,
and that scale is spread across roughly a dozen magic numbers in the projection
math, the brick pipeline, the clip and bucketing logic, the sphere primitive,
and the sprite sizing. This document catalogs them so the scale can be made a
parameter (zoom / HD), and so we stop discovering constants one bug at a time.

This is the **scale** axis. It is the companion to
[WIDESCREEN_HARDCODED_DIMS_AUDIT.md](WIDESCREEN_HARDCODED_DIMS_AUDIT.md), which
covers the **resolution / origin** axis (640x480 to `ModeDesiredX/Y`). The two
are orthogonal: widescreen decides where the projection is centred and how big
the buffer is; this audit decides how many pixels a brick, model, sprite, or
sphere occupies.

An in-flight branch (PR #311) adds an `IsoScale` parameter that multiplies the
group-A constants below (and a follow-up scales the brick draw). Where this doc
notes a constant as "scaled", it means that in-flight work; on `main` every
constant here is still the fixed native value. The point of the audit is the
full inventory, independent of that branch.

## The iso coordinate model

World units (both games):

- `SIZE_BRICK_XZ = 512` world units per brick step on the X and Z (ground) axes
  (`SOURCES/DEFINES.H:220`, `SOURCES/COMMON.H:188`).
- `SIZE_BRICK_Y  = 256` world units per brick step on the Y (height) axis.

The iso projection maps those world units to screen pixels at a fixed scale.
The native calibration, expressed per brick step:

| Axis | Screen pixels per brick step | Where |
|------|------------------------------|-------|
| Horizontal (x - z) | 24 px | `Map2Screen`: `24*(x-z)`; `LongProjectPointIso`: `3*(x-z)/64`, and `3*512/64 = 24` |
| Ground depth (x + z) | 12 px | `Map2Screen`: `12*(z+x)`; iso proj `6*(x+z)/256`, `6*512/256 = 12` |
| Height (y) | 15 px | `Map2Screen`: `15*y`; iso proj `15*y/256`, `15*256/256 = 15` |

So a single brick tile is about **48 px wide and 38 px tall** on screen at
native scale (the diamond is two 24 px half-columns wide; the column bucketing
calls these "48 / 2 colonne intercalée"). Every clip and bucketing constant
below is one of those tile dimensions (24, 48/47, 38) in disguise; every
projection constant is the same scale expressed as a shift.

**Key point:** there is one logical iso scale, written ~12 different ways. To
zoom or render at HD, all of the pixel constants must move together; the
world-unit constants (512, 256) stay.

## A. Projection scale (the core scale)

These encode the world to screen pixel scale. To zoom or render at HD they must
all be multiplied by one scale factor. The in-flight `IsoScale` work (PR #311)
does exactly that for this group, bit-exact at the identity factor.

| Site | Literal | Meaning |
|------|---------|---------|
| `LIB386/3D/LPROJISO.CPP` | `*3`, `>>6` (/64) | model vertex X: `3*(x-z)/64` |
| `LIB386/3D/LPROJISO.CPP` | `<<4` (*16), `*2`, `*3`, `15`, `>>8` (/256) | model vertex Y: `(6*(x+z) - 15*y)/256` |
| `LIB386/3D/LPROJISO.CPP` | `+1` | `YCentre + 1` vertical bias |
| `LIB386/3D/PRLIISO.CPP` | `*3 >>6`, `(6*(x+z)-15*y) >>8`, `+1` | batched object geometry, same scale |
| `SOURCES/GRILLE.CPP` `Map2Screen` | `24`, `12`, `15` | brick X / ground / height (brick-unit coords) |
| `LIB386/OBJECT/AFF_OBJ.CPP` `Sphere`/`Sphere_Transp` | `*34`, `>>9` (/512) | sphere screen radius = `Rayon*34/512` |

Note the sphere uses `34/512` (~0.066), not the horizontal `24/512` (~0.047):
a sphere looks the same from every angle, so its radius uses an averaged iso
foreshortening, not the X scale. It is a distinct constant and must be scaled
on its own (PR #311 does this separately from the X/Y scale).

## B. Brick pixel dimensions in clip + bucketing

These are the 48 / 24 / 38 tile dimensions used as screen-space thresholds.
They assume native brick size and must scale with the iso scale.

("scaled in PR #311?" tracks the in-flight brick-scaling branch, not `main`.)

| Site | Literal | Meaning | Scaled in PR #311? |
|------|---------|---------|--------------------|
| `SOURCES/GRILLE.CPP` `AffBrickBlock` | `XScreen >= -47` | left clip = full brick width - 1 | No |
| `SOURCES/GRILLE.CPP` `AffBrickBlock` | `YScreen >= -38` | top clip = brick height | No |
| `SOURCES/GRILLE.CPP` `AffBrickBlock` | `XScreen < -24` | column-storage guard = half brick | No |
| `SOURCES/GRILLE.CPP` `AffBrickBlock` | `col = (XScreen + 24) / 24` | column bucket = half-brick granularity | Partially (see note) |
| `SOURCES/GRILLE.CPP` bucketing | `(ModeDesiredX/2 - 32) % 24 == 0` | iso-origin alignment constraint vs the 24 grid | No |
| `SOURCES/GRILLE.CPP` `DrawOverBrick*` | `Ys + 38` | re-blit vertical overlap = brick height | Yes |
| `SOURCES/GRILLE.CPP` `DrawOverBrick` | `startcol ... - 2` | back-scan = one full brick of columns | Yes (`* isoCols`) |
| `SOURCES/GRILLE.CPP` `DrawOverBrick3/Cage` | `startcol ... - 1` | back-scan = half brick of columns | Yes (`* isoCols`) |

Note on the `24` bucket divisor: the column **index** stays bounded because
`XScreen` is screen-clamped, so storage does not overflow at zoom. But the
divisor is the native half-brick width; at large zoom the bucket granularity is
coarse relative to the scaled tile, which is why the back-scan biases had to be
widened by `isoCols`. The cleaner fix is to derive the divisor from the scaled
half-brick width. The `% 24 == 0` alignment constraint is a widescreen concern
(origin vs grid) that interacts with any change to the grid granularity.

## C. Origin offsets (track resolution, not scale)

These set where the iso grid is anchored. They scale with the framebuffer
(widescreen, already routed through `ModeDesiredX/Y`), NOT with the iso scale:
zoom happens around the anchor, so the anchor itself does not scale.

| Site | Literal | Meaning |
|------|---------|---------|
| `SOURCES/GRILLE.CPP` `Map2Screen` | `ModeDesiredX/2 - 32` | brick origin X shim |
| `SOURCES/GRILLE.CPP` `Map2Screen` | `ModeDesiredY/2 - 14` | brick origin Y shim |
| `SOURCES/GAMEMENU.CPP` `Init3DView` | `ModeDesiredX/2 - 9` | iso projection centre shim (pairs with the -32) |
| `SOURCES/COMPORTE.CPP` | `ModeDesiredX/2 - 9` | behaviour-menu iso centre |

The `-32 / -14 / -9` shims are themselves native-pixel grid offsets. They do
not scale with the zoom, but if the grid granularity (B) ever changes they must
be revisited together (the -9 and -32 are documented as a pair).

## D. Sprite sizing (two paths, only one scales)

Iso 2D sprites (extras, particles, ground items, bonuses) are sized by
`CalculeScaleFactorSprite` (`SOURCES/EXTFUNC.CPP:358`):

- **Projection-derived** (`scaleorg > 0`): projects two points 1000 world-Y
  apart and measures the screen delta via `LongProjectPoint` (= the iso
  projector). Once that projector carries an `IsoScale` (PR #311), these sprites
  **scale with the zoom** automatically.
- **Fixed native** (interior, `scaleorg == -1`): `ScaleFactorSprite =
  DEF_SCALE_FACTOR` (`65536` = 1.0, `SOURCES/COMMON.H:405`). These are pinned
  to native size and **do NOT scale** with `IsoScale`. At zoom they stay small.

Plus the per-sprite hotspot/position offset is native-pixel data, added in
screen space after projection:

| Site | Literal | Meaning | Scaled in PR #311? |
|------|---------|---------|--------------------|
| `SOURCES/OBJECT.CPP:5119-5120` | `ZV[num*8 + 0/1]` | sprite hotspot X/Y added to projected `Xp/Yp` | No |
| `SOURCES/OBJECT.CPP:5191-5192` | `ZV[num*8 + 0/1]` | extra/particle hotspot X/Y | No |
| `SOURCES/INTEXT.CPP:277-278` | `ZV[sprite*8 + 2/3]` | sprite width/height (native px) | No |
| `SOURCES/COMMON.H:405` | `65536` | `DEF_SCALE_FACTOR` native sprite scale | n/a |

So two gaps for zoom: (1) interior `scaleorg == -1` sprites stay native size,
and (2) the ZV hotspot offset is added unscaled to a scaled `Xp/Yp`, so scaled
sprites are mispositioned by up to a native hotspot. Both are screen-pixel
assumptions tied to the native iso scale.

## E. Shadows

- `ALTITUDE_MAX = 200` (`SOURCES/BEZIER.H:4`, used in `GRILLE.CPP` shadow
  clamp) is **world units**, not pixels, so it does not scale with the iso
  scale.
- The shadow shape itself is projected through the iso projector, so its size
  follows `IsoScale` like other projected geometry. (Worth a visual confirm at
  zoom; no separate pixel constant found.)

## F. Depth / z-buffer (not a screen-pixel scale)

Iso interiors use the z-buffer filler. Z values are depth, not screen pixels,
so the iso screen scale should not touch them. `Fill_ZBuffer_Factor` is used in
the sphere only on the perspective (`TYPE_3D`) branch, not the iso branch. No
iso z constant needs scaling; flagged only so a future change does not assume
otherwise.

## Status in the in-flight scaling work (PR #311)

Scaled and verified bit-exact at `ISO_SCALE_ONE`:

- All four projection-scale families (A): `LongProjectPointIso`,
  `ProjectListIso`, `Map2Screen`, sphere radius.
- Brick draw via `BlitBrickIso` (decode RLE to raw, `ScaleSprite`).
- Brick re-blit via `ReBlitBrickIso` (re-draw scaled tile instead of
  `CopyMask`), plus the `Ys + 38` overlap and the `startcol` back-scan.

Not yet scaled (the remaining base assumptions):

1. `AffBrickBlock` clip thresholds `-47`, `-38`, `-24` (B): partially-visible
   scaled bricks at the screen edges may be wrongly rejected.
2. The `24` column-bucket divisor and its `% 24` alignment constraint (B): the
   bucket granularity is native half-brick; widened back-scans are a workaround,
   not a derivation from the scaled width.
3. Interior `scaleorg == -1` sprites pinned to `DEF_SCALE_FACTOR` (D).
4. ZV sprite hotspot offsets added unscaled to scaled `Xp/Yp` (D).

## Recommendation: one scale, one source

The native iso scale is one number written ~12 ways. The robust direction is to
derive every pixel constant from a single base (for example an
`ISO_HALF_BRICK_PX = 24` from which the full width 48, the height 38, the
projection multipliers, and the bucket divisor all derive), then apply
`IsoScale` once at that source. That turns "find the next magic number" into a
single, testable scaling point, and makes the LBA1 `SetIsoProjection(scale)`
lineage (a real scale parameter) expressible without scattering it again.

Until then, the inventory above is the checklist: any iso scale or zoom work
must account for groups A, B, and D, not just the projection.

## Cross-references

- `docs/ISO_SCALE.md` (PR #311, not yet on `main`) - the `IsoScale` parameter
  and its LBA1 lineage.
- [WIDESCREEN_HARDCODED_DIMS_AUDIT.md](WIDESCREEN_HARDCODED_DIMS_AUDIT.md) - the
  resolution/origin axis; rows A7 (brick left clip) and A8 (DrawOverBrick
  col-scan) overlap with group B here but from the width angle.
- [WIDESCREEN_PROJECTION_AUDIT.md](WIDESCREEN_PROJECTION_AUDIT.md) - the
  projection-origin routing.
