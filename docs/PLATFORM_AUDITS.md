# Platform audits

Per-file audit logs hung off [PLATFORM.md](PLATFORM.md). Each section is one class of platform hazard; rows are individual files inspected against that class.

These tables grow as sweeps land. The class definition and worked example live in PLATFORM.md; the granular per-file verdicts live here so PLATFORM.md stays a high-level map.

---

## U32 wrap — renderer-side address-space wraparound

Class definition: [PLATFORM.md §1 "Renderer-side wraparound"](PLATFORM.md#renderer-side-wraparound). Worked example: [PR #84](https://github.com/LBALab/lba2-classic-community/pull/84) (CopyMask). Investigation runbook: [CRASH_INVESTIGATION.md](CRASH_INVESTIGATION.md).

Verdicts:

- **fixed** — bug was present; PR landed.
- **safe** — defensive clipping precedes the pointer math in this function. Negative inputs cannot reach the U32 conversion.
- **safe (convention)** — no defensive clipping, but every caller in the tree passes non-negative coordinates. Latent trap if a future caller ever passes negative — worth a comment in-source pointing at this audit if you touch one of these.

| File | Function | Verdict | Notes |
|---|---|---|---|
| `LIB386/SVGA/AFFSTR.CPP` | `AffString`, `AffStringToBuffer` | safe (convention) | Pointer math uses `Log + TabOffLine[y] + x`; signed `+ x` is correct, but `TabOffLine[y]` would OOB-read for negative `y`. All callers are UI/menu code with non-negative coords. |
| `LIB386/SVGA/BLITBOXF.CPP` | `BlitBoxF` | safe | Fixed coordinates (160, 140); no signed input. |
| `LIB386/SVGA/BOX.CPP` | `Box` | safe | Signed clipping clamps `x0/y0/x1/y1` into the clip rect before any U32 math. |
| `LIB386/SVGA/CALCMASK.CPP` | `CalcGraphMsk` | safe | Operates on bank data; no screen-pointer math. |
| `LIB386/SVGA/CLRBOXF.CPP` | `ClearBox` | safe (convention) | Indexes `TabOffDst[y]` with `S16` from a `T_BOX`. All callers populate the box with non-negative bounds. |
| `LIB386/SVGA/COPYMASK.CPP` | `CopyMask` | fixed | PR #84. |
| `LIB386/SVGA/CPYBLOCI.CPP` | `CopyBlockIncrust` | safe | Signed clipping precedes pointer math; both source and destination clipped. |
| `LIB386/SVGA/CPYBLOCK.CPP` | `CopyBlock` | safe | Same pattern as `CopyBlockIncrust`. |
| `LIB386/SVGA/FONT.CPP` | `Font`, `CarFont`, `SizeFont` | safe | Delegates to `AffMask` (in `MASK.CPP`). |
| `LIB386/SVGA/GRAPH.CPP` | `AffGraph`, `ClippingGraph` | safe | Fast-path `AffGraph` dispatches negative-or-overhanging cases to `ClippingGraph`. Both compute pointers as `Log + TabOffLine[y] + x` (pointer + S32, not pointer + U32 — the failing pattern). |
| `LIB386/SVGA/MASK.CPP` | `AffMask`, `ClippingMask` | safe | Geometry locals already `S32`; clipping explicit before pointer math. |
| `LIB386/SVGA/PLOT.CPP` | `Plot`, `GetPlot` | safe | Hard signed clip-rect check at entry; returns early on out-of-range. |
| `LIB386/SVGA/RESBLOCK.CPP` | `RestoreBlock` | safe (convention) | No defensive clipping; mirrored with `SaveBlock` so callers pair the two with the same coords. All call sites use non-negative menu/UI coords. |
| `LIB386/SVGA/SAVBLOCK.CPP` | `SaveBlock` | safe (convention) | Same pattern and caller set as `RestoreBlock`. |
| `LIB386/SVGA/SCALEBOX.CPP` | `ScaleBox` | safe (convention) | No defensive clipping. All call sites pass full-screen or hard-coded non-negative source rects. |
| `LIB386/SVGA/SCALESPI.CPP` | `ScaleSprite` | safe | All-S32 clipping clamps `sx/sy/end_x/end_y` to the clip rect before pointer math. |
| `LIB386/SVGA/SCALESPT.CPP` | `ScaleSpriteTransp` | safe | Both fast (1:1) and scaled paths clip with signed compares before pointer math. |
| `LIB386/pol_work/POLY.CPP` | `Fill_Poly`, `Fill_PolyClip`, `Draw_Triangle`, `Fill_Clip*` | safe | `Fill_PolyClip` computes the polygon bounding box, returns early if it doesn't overlap the clip rect, and dispatches to `Fill_ClipXMin/XMax/YMin/YMax` (Sutherland–Hodgman clippers) before any vertex reaches `Draw_Triangle`'s pointer math. By construction `Pt_XE/Pt_YE` are non-negative in the rasteriser. |
| `LIB386/pol_work/POLYCLIP.CPP` | `ClipperZ`, `Clipping_ZFPU` | safe | Operates in 3D vertex space (`STRUC_CLIPVERTEX::V_X0/V_Y0/V_Z0`); no screen-pointer math. |
| `LIB386/pol_work/POLYDISC.CPP` | `Fill_Sphere`, `Sph_Line_*` | safe | `Fill_Sphere` computes `Sph_XMin/XMax/YMin/YMax` with signed clipping against the clip rect before invoking any `Sph_Line_*`. The line fillers take a `U32 screenY` whose value is bounded by clipped `Sph_YMin/YMax`. |
| `LIB386/pol_work/POLYFLAT.CPP`, `POLYGOUR.CPP`, `POLYTEXT.CPP`, `POLYTEXZ.CPP`, `POLYTZF.CPP`, `POLYTZG.CPP`, `POLYGTEX.CPP` | `Filler_*` family | safe | All take `(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)`. By construction reached only from the `Fill_PolyClip → Draw_Triangle` pipeline after polygon vertices have been clipped against the screen rect. No way for a negative coordinate to enter. |
| `LIB386/pol_work/POLYLINE.CPP` | `Line`, `Line_Entry`, `Line_A`, `Line_ZBuffer`, `Line_ZBuffer_NZW` | safe | Heavy upfront signed clipping (DX-zero, DY-zero, and general edge cases all clamp `x0/x1/y0/y1` to the clip rect via `continue`/`return`) before any pointer math. The post-clip `U32 offset = PTR_TabOffLine[y0] + x0` and pointer-walk `dst += incrX/incrY` operate on values guaranteed non-negative. |
| `LIB386/pol_work/POLY_JMP.CPP` | `Jmp_Solid`, `Jmp_Transparent`, `Jmp_Trame*`, etc. | safe | Pure dispatch tables; no screen-pointer math. |
| `LIB386/pol_work/TESTVUEF.CPP` | `Test_VueF` | safe | Visibility-test helper; no screen-pointer math. |
| `LIB386/3D/` (entire directory, 33 files) | math/projection/rotation routines | safe (no-op) | Pure math — matrices, sin/cos, rotation, projection, distance, light. Zero references to `Log` / `Screen` / `TabOffLine` across the entire group. The hazard class doesn't apply. |
| `SOURCES/3DEXT/BOXZBUF.CPP` | `ZBufBoxOverWrite2` | safe (convention) | Same `U32 startOffset = TabOffLine[ymin] + xmin + deltaX` pattern as CopyMask. All current callers (`DrawRecover`, `DrawRecover3` in `SOURCES/INTEXT.CPP`) pass `ClipXMin/ClipYMin/ClipXMax/ClipYMax` (the global clip rect, non-negative). Latent trap if a future caller passes per-object screen-bbox coords with a negative edge — the bug would reproduce identically to #78. |
| `SOURCES/3DEXT/DECORS.CPP` | `DrawCubeNoBox` | safe | Wireframe debug rendering; projects 3D vertices to screen and dispatches to `Line()` / `Rect()` (POLYLINE, safe). |
| `SOURCES/3DEXT/DRAWSKY.CPP` | sky rendering | safe | Dispatches to `Fill_Poly` (gated by `Fill_PolyClip`, safe). |
| `SOURCES/3DEXT/LINERAIN.CPP` | `LineRain` | safe | Heavy upfront signed clipping for every code path before pointer math; same pattern as POLYLINE. |
| `SOURCES/3DEXT/TERRAIN.CPP` | terrain rendering | safe | Dispatches to `Fill_Poly` (gated, safe). |
| `SOURCES/3DEXT/MAPTOOLS.CPP`, `LOADISLE.CPP`, `GLOBEXT.CPP`, `LBA_EXT.CPP`, `VAR_EXT.CPP` | island data loading + globals | safe (no-op) | Asset loading and global storage; no rendering. |

### Architecture insight

pol_work is safe *by construction* — `Fill_PolyClip` is the unconditional gateway that clips polygon bounding boxes against the clip rect before any vertex reaches the rasteriser, so fillers can legitimately use `U32` for `nbLines` / `fillCurXMin` / `fillCurXMax` without hazard. The same is true for `Fill_Sphere` and every `Line_*` variant. SVGA was riskier because its functions are entry points called from arbitrary UI/HUD code without a single gateway equivalent.

### Sweep status

| Group | Status | PR |
|---|---|---|
| `LIB386/SVGA/` | swept — 1 fixed, 16 safe | #84, #86 |
| `LIB386/pol_work/` | swept — 0 new bugs | #87 |
| `LIB386/3D/` + `SOURCES/3DEXT/` | swept — 0 new bugs (1 latent trap recorded) | #89 |
| `SOURCES/GRILLE.CPP` + `SOURCES/INTEXT.CPP` | not yet swept | — |

**Findings from 3D + 3DEXT:** `LIB386/3D/` is entirely off-topic for the class (pure math, zero screen pointers). 3DEXT mostly delegates to gated rendering (`Fill_Poly` via POLY.CPP's `Fill_PolyClip`, or `Line()` via POLYLINE.CPP). The only direct screen-pointer math is `BOXZBUF.CPP::ZBufBoxOverWrite2`, which has the same `U32 + TabOffLine[ymin] + xmin` pattern that caused #78 — currently safe only because every caller passes the global clip rect (non-negative). If a future caller ever passes a per-object screen bbox with a negative edge, the bug reproduces identically. Worth a comment in-source if anyone changes those call sites.

**Next:** Sweep `SOURCES/GRILLE.CPP` + `SOURCES/INTEXT.CPP` (interior recover pass — caller side of the original `CopyMask` bug; worth checking for off-by-one writes that corrupt `ListBrickColon` boundaries).
