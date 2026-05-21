# Projection 4:3 assumption audit

A read-only inventory of every place the 3D projection path bakes in a 640×480 / 4:3 framebuffer, checked against the `RESOLUTION_X` / `RESOLUTION_Y` migration in [PR #134](https://github.com/LBALab/lba2-classic-community/pull/134) (Phase B in [WIDESCREEN.md](WIDESCREEN.md)). It complements [CAMERA.md](CAMERA.md), which describes the camera and projection model in general; this doc only tracks the resolution assumptions.

File and line references were verified against the working tree on 2026-05-21. Treat them as drift-prone — re-grep before relying on an exact line.

## Summary

- The whole 3D world projects through a single setup call, `SOURCES/EXTFUNC.CPP:93`, and it passes a hardcoded screen centre of `320, 240`. PR #134 routed the *consumers* of projection (sort preclip, cinema bars, logo anchor, present blit) through `ModeDesiredX` / `ModeDesiredY`, but not the projection origin itself.
- `SOURCES/GRILLE.CPP` (the brick renderer) is the one render subsystem PR #134 did not touch. It still clips and centres bricks against literal `640` / `480` / `639`.
- There is no screen-derived "cube culling radius." Exterior cube visibility is gated in world space (camera distance, near plane), so it does not break when the framebuffer widens.

## The projection path

Every world vertex in the sort tree (`SOURCES/SORT.CPP::TreeInsert`) and every object vertex list (`LIB386/3D/PRLI3DF.CPP::ProjectList3D`) goes through `LongProjectPoint3D` (`LIB386/3D/LPROJ3DF.CPP:14`, installed as the `LongProjectPoint` function pointer at line 34) or its batch twin, which read the same globals.

```
if (z > CameraZrClip)  ->  Xp = Yp = INT_MIN; return 0    // near-plane reject
factor = FRatioX / (CameraZr - z)                         // perspective divide (1/depth)
Xp = XCentre + round((x - CameraXr) * factor)
Yp = YCentre + round((y - CameraYr) * factor * FRatioY)
```

`round` is `lrintl` on a `long double` accumulator, matching the x87 `fistp` rounding of the original ASM.

The constants come from `LIB386/3D/PROJ.CPP::SetProjection(xcentre, ycentre, clip, factorx, factory)`:

| Constant | Origin | Main gameplay value | Resolution-routed? |
|----------|--------|---------------------|--------------------|
| `XCentre` | `SetProjection` arg, stored at `PROJ.CPP:10` | `320` literal (`EXTFUNC.CPP:93`) | No — hardcoded |
| `YCentre` | `SetProjection` arg, `PROJ.CPP:11` | `240` literal (`EXTFUNC.CPP:93`) | No — hardcoded (correct for height 480, but still a literal) |
| `FRatioX` | `PROJ.CPP:23`, = `factorx` | `ChampX` = `600` (`SOURCES/3DEXT/VAR_EXT.CPP:24`) | n/a — focal, not width |
| `FRatioY` | `PROJ.CPP:24`, = `-factory / FRatioX` | `-ChampZ / ChampX` = `-1.0` | n/a — focal |
| `CameraXr` / `Yr` / `Zr` | `LIB386/3D/CAMERA.CPP` (`SetCamera` / `SetFollowCamera`) | camera world reference | n/a — depth/world |
| `CameraZrClip` | `CAMERA.CPP:68`, = `Z0 - NearClip` | `NearClip` = `CLIP_NEAR` = `3000` (`SOURCES/COMMON.H:42`) | n/a — depth plane |

### How the 4:3 ratio is encoded

On the gameplay path `factorx == factory` (`ChampX == ChampZ == 600`), so the X and Y focal magnitudes are equal. The horizontal-to-vertical *visible-extent* ratio is therefore set entirely by `XCentre : YCentre = 320 : 240 = 4:3`.

Widen the framebuffer to, say, 768 without moving `XCentre` and the world stays optically anchored at x=320: the vanishing point sits left of the framebuffer centre (384), the view is not symmetric, and content only spills into the right margin instead of being centred in it.

The corrective change is to derive `XCentre` from `ModeDesiredX / 2`. `YCentre` stays at half the (unchanged) height and the focal stays fixed; widening then reveals more horizontal field of view rather than stretching. Because `LongProjectPoint3D` and `ProjectList3D` share these globals, fixing the one `SetProjection` call covers both the sort tree and object drawing.

### Other `SetProjection` / `SetIsoProjection` callers

These configure the same globals for non-gameplay views. Most are UI-space and out of scope for render-space widescreen, listed for completeness.

| Site | Centre passed | Notes |
|------|---------------|-------|
| `SOURCES/EXTFUNC.CPP:93` | `320, 240` | The 3D world view. The gating site above. |
| `SOURCES/OBJECT.CPP:6004` | `30 + ClipXMin, 30 + ClipYMin` | INCRUST_OBJ 3D preview panel; centre derived from the clip region. |
| `SOURCES/CREDITS.CPP:200` | box midpoint | Focal derived from `ModeDesiredX` / `ModeDesiredY` — the only fully resolution-aware caller. |
| `SOURCES/GAMEMENU.CPP:3828` | `320, 240` | Menu 3D. Hardcoded. |
| `SOURCES/HOLOPLAN.CPP:468` | `320, 240` | Holomap. Hardcoded. |
| `SOURCES/HOLOGLOB.CPP:835` | `CENTERX, CENTERY` = `320, 220` (`SOURCES/HOLO.H:77`) | Holomap globe. Hardcoded via named constants. |
| `SOURCES/INVENT.CPP:567` | item-slot `x, y` | Inventory item preview; derived. |
| `SOURCES/COMPORTE.CPP:351`, `SOURCES/GAMEMENU.CPP:514` | `320 - 8 - 1, 240` | Iso character display; hardcoded. The iso path adds fixed scale shifts in `LIB386/3D/PRLIISO.CPP:36`. |

## Culling and preclip

| Site | What it bounds | Derivation |
|------|----------------|------------|
| `SOURCES/SORT.CPP:211` (`TreeInsert` preclip) | `(x1 < 0) OR (x0 >= (S32)ModeDesiredX) OR (y1 < 0) OR (y0 >= (S32)ModeDesiredY)` | Routed through `ModeDesiredX` / `ModeDesiredY`. Wide-aware, but fed by the 320-centred projection above, so it preclips mis-centred coordinates until the projection origin is fixed. |
| `LIB386/3D/LPROJ3DF.CPP:16` | near-plane reject `z > CameraZrClip` | Depth (`CLIP_NEAR` = 3000). Width-independent. |
| `SOURCES/3DEXT/MAPTOOLS.CPP:283` | camera march `n = 1000 .. VueDistance` step 512, cube bounds `0 .. 32768` | World space (`VueDistance` = 30000 default, `SOURCES/3DEXT/VAR_EXT.CPP:27`; per-view `DefVueDistance {10500, 17000}`, `SOURCES/GLOBAL.CPP:312`). Width-independent. |
| `LIB386/OBJECT/AFF_OBJ.CPP` (`Line`, `TestVisible`, `InsertQuad`) | off-screen sentinel test `== -0x8000` | Marker value, width-independent. |

There is no screen-space "neighbour cube radius." Cube and decor visibility comes from the world-space camera distance, the near plane, and per-primitive screen clips (the GRILLE clips below, plus the already-fixed sort preclip).

## Render-space sites still hardcoded

`SOURCES/GRILLE.CPP` is the render subsystem PR #134 did not cover. None of these route through the resolution constants:

- `GRILLE.CPP:738`, `:819`, `:900` (`AffBrickBlock` / `AffBrickBlockColon` / `AffBrickBlockOnly`): `if (XScreen >= -24 AND XScreen < 640 AND YScreen >= -38 AND YScreen < 480)`. Bricks projecting to screen-X ≥ 640 are dropped, clipping grid scenery at x=640 on a wider canvas.
- `GRILLE.CPP:953`: `xmin = 639; ymin = 479;` — seeds a bounding-box min-finder at the 4:3 edges.
- `GRILLE.CPP:1370` (`Map2Screen`): `XScreen = 24 * (x - z) + 288; YScreen = 12 * (z + x) - 15 * y + 226;` — the isometric brick-to-screen centre offsets `288` / `226` are baked in.

The projection origin (`EXTFUNC.CPP:93`) is the other render-space gap; see above.

## Render-space sites already routed (PR #134)

For contrast, these were correctly migrated and are listed so a future change does not redo them:

- `SOURCES/OBJECT.CPP:4696`–`4767` cinema bars → `(S32)ModeDesiredX - 1` / `(S32)ModeDesiredY - 1`.
- `SOURCES/OBJECT.CPP:6058`, `:6067`, `:6085` top-right logo anchor → `(S32)ModeDesiredX - 1 - 103` / `- 110`.
- `LIB386/SVGA/AFFSTR.CPP:299` text row stride → `ModeDesiredX`.
- `LIB386/SVGA/SDL.CPP:209` present loop → `ModeDesiredX` / `ModeDesiredY`.
- `SOURCES/MEM.CPP:33`–`35` buffer allocations → `RESOLUTION_X * RESOLUTION_Y`.

## Relation to Phase B

[WIDESCREEN.md](WIDESCREEN.md) records Phase B as done and describes the expected result of a wider build as a 3D world that "render[s] across the full width" with "the sort tree preclip[ping] correctly." The preclip part holds. The 3D world part is only partly true: 3D content does spill into the wider margin, but the view is optically anchored at x=320 rather than centred, and the GRILLE brick path clips at x=640. Centring the 3D world at a wider resolution needs the `EXTFUNC.CPP:93` projection origin and the GRILLE clips addressed — both still open after Phase B.

## The `feat/widescreen-deck` branch

The branch's diff (its two widescreen commits, `7ae12dd..b22a491` — it has no merge base with `main`) touches the same render-space sites found here: the `EXTFUNC` projection is left alone, but it bumps `SORT.CPP` preclip, the three `GRILLE.CPP` brick clips, `OBJECT.CPP` cinema bars and logo anchor, and `AFFSTR.CPP` / `SDL.CPP` to a literal 768. It is a call-site map only, for the reasons in [WIDESCREEN.md](WIDESCREEN.md#the-featwidescreen-deck-branch).
