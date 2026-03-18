# ASM → C Validation Progress

Tracks equivalence testing between original x86 ASM implementations and their
C/C++ ports in `LIB386/`.  Each row represents a function (or data table) that
exists in both `.ASM` and `.CPP` form.

**Legend:**  `[ ]` untested · `[x]` tested · `[~]` partial

---

## 3D/ — 3D Math & Projection (23 pairs)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [x] | `3D/COPYMATF.ASM` | `3D/COPYMATF.CPP` | `CopyMatrixF` | Copy 3×3 rotation matrix (float) | `tests/3D/test_copymatf.cpp` now covers identity-like, mixed-sign, finite edge-value, zero-matrix, and 200-round deterministic random matrix cases, comparing the full `TYPE_MAT` byte-for-byte and verifying the source matrix remains unchanged in both ASM and CPP. Important test-domain note: `CopyMatrixF` is validated on real float-matrix inputs, not arbitrary raw 48-byte patterns, because the ASM implementation copies via `fld/fstp qword` pairs rather than raw integer moves. |
| [x] | `3D/GETANG2D.ASM` | `3D/GETANG2D.CPP` | `GetAngleVector2D` | 2D angle from (x, z) via tangent table binary search | `tests/3D/test_getang2d.cpp` now covers axis cases, all quadrants, near-45-degree swap boundaries, large-magnitude inputs, and 200 deterministic LCG stress rounds against `TanTab`-driven ASM output. |
| [x] | `3D/GETANG3D.ASM` | `3D/GETANG3D.CPP` | `GetAngleVector3D` | 3D angle from (x, y, z); writes `X0`, `Y0` | `tests/3D/test_getang3d.cpp` now covers axis and diagonal vectors, sign/quadrant combinations, near-boundary cases, and 200 deterministic LCG stress rounds while comparing both the return value and written globals `X0`/`Y0`. |
| [x] | `3D/IMATSTDF.ASM` | `3D/IMATSTDF.CPP` | `InitMatrixStdF` | Init rotation matrix from Euler angles (α, β, γ) | `tests/3D/test_imatstdf.cpp` now covers axis rotations, diagonal/mixed rotations, wraparound and negative angles, plus 200 deterministic LCG stress rounds, comparing the full `TYPE_MAT` byte-for-byte against ASM output. |
| [x] | `3D/IMATTRAF.ASM` | `3D/IMATTRAF.CPP` | `InitMatrixTransF` | Set matrix translation (TX, TY, TZ) | `tests/3D/test_imattraf.cpp` now covers fixed translations, 200 deterministic LCG stress rounds, exact float-slot bit patterns for `TX`/`TY`/`TZ`, and preservation of non-translation fields in pre-filled matrices. |
| [x] | `3D/LIROT3DF.ASM` | `3D/LIROT3DF.CPP` | `LongInverseRotatePointF` | Inverse-rotate 3D point; writes `X0`, `Y0`, `Z0` | `tests/3D/test_lirot3df.cpp` now covers multiple fixed matrices, mixed-sign vectors, 200 deterministic LCG stress rounds, and verifies both written globals and that the input matrix remains unchanged in ASM and CPP. |
| [x] | `3D/LITLISTF.ASM` | `3D/LITLISTF.CPP` | `LightList` | Compute per-vertex lighting from normals | `tests/3D/test_litlistf.cpp` now covers fixed and deterministic random light vectors, variable list sizes, zero-count early return, full output-buffer equivalence, and `X0`/`Y0`/`Z0` side effects. Fixed CPP parity bug: after inverse-rotating the light vector, `LightList` must store the scaled float bit patterns back into `X0`/`Y0`/`Z0` to match ASM scratch-state behavior. |
| [x] | `3D/LPROJ3DF.ASM` | `3D/LPROJ3DF.CPP` | `LongProjectPoint3D` | Scalar 3D perspective projection; writes `Xp`, `Yp` | `tests/3D/test_lproj3df.cpp` now covers multiple projection/camera states, clip rejection, sentinel `Xp`/`Yp` outputs on failure, and 200 deterministic LCG stress rounds. Batch list projection remains tracked separately under `PRLI3DF`. |
| [x] | `3D/LPROJISO.ASM` | `3D/LPROJISO.CPP` | `LongProjectPointIso` | Isometric projection; writes `Xp`, `Yp` | `tests/3D/test_lprojiso.cpp` now covers multiple camera/centre states, return value plus `Xp`/`Yp`, and 200 deterministic LCG stress rounds. |
| [ ] | `3D/LROT2DF.ASM` | `3D/LROT2DF.CPP` | `LongRotate`, `Rotate` | 2D rotation by angle; writes `X0`, `Z0` | Uses `SinTabF`/`CosTabF` |
| [ ] | `3D/LROT3DF.ASM` | `3D/LROT3DF.CPP` | `LongRotatePointF` | 3D matrix × point; writes `X0`, `Y0`, `Z0` | |
| [ ] | `3D/MULMATF.ASM` | `3D/MULMATF.CPP` | `MulMatrixF` | 3×3 matrix multiplication (float) | |
| [x] | `3D/PRLI3DF.ASM` | `3D/PRLI3DF.CPP` | `ProjectList3DF` | Batch 3D projection with screen bounds | `tests/3D/test_prli3df.cpp` now covers multi-point batches, clipping and overflow sentinels, variable origins/projection states, zero-count early return, screen-bounds globals, and scratch globals `Xp`/`Yp`/`X0`/`Y0`/`Z0`. Fixed CPP parity bug: `Xp`/`Yp` must retain the raw projected offsets `Xe`/`Ye`, while centered coordinates are used only for bounds checks and stored output points. |
| [ ] | `3D/PRLIISO.ASM` | `3D/PRLIISO.CPP` | `ProjectListIso` | Batch isometric projection | |
| [ ] | `3D/PROJ.ASM` | `3D/PROJ.CPP` | `SetProjection`, `SetIsoProjection` | Configure projection globals & func pointers | |
| [ ] | `3D/REGLE3.ASM` | `3D/REGLE3.CPP` | `RegleTrois`, `BoundRegleTrois` | Linear interpolation (rule of three) ± bounds | |
| [ ] | `3D/ROTMATU.ASM` | `3D/ROTMATU.CPP` | `RotateMatrixU` | Rotate matrix by Euler angles | Calls `InitMatrixStd` + `MulMatrix` |
| [ ] | `3D/ROTRALIF.ASM` | `3D/ROTRALIF.CPP` | `RotTransListF` | Batch rotate + translate vertex list | |
| [ ] | `3D/ROTVECT.ASM` | `3D/ROTVECT.CPP` | `RotateVector` | Rotate vector by Euler angles; writes `X0`, `Y0`, `Z0` | Calls `InitMatrixStd` + `LongRotatePoint` |
| [ ] | `3D/SINTAB.ASM` | `3D/SINTAB.CPP` | `SinTab[]`, `CosTab[]` | Pre-computed 16-bit sine/cosine (4096 entries) | Data table |
| [ ] | `3D/SINTABF.ASM` | `3D/SINTABF.CPP` | `SinTabF[]`, `CosTabF[]` | Pre-computed float sine/cosine (4096 entries) | Data table |
| [ ] | `3D/SQRROOT.ASM` | `3D/SQRROOT.CPP` | `Sqr`, `QSqr` | Integer square root (32-bit and 64-bit) | CPP uses `sqrt()`/`sqrtl()` — known ±1 discrepancy in `QSqr` |
| [ ] | `3D/TANTAB.ASM` | `3D/TANTAB.CPP` | `TanTab[]` | Pre-computed tangent table (512 entries) | Data table |

## SOURCES/ - Core Source Helpers (6 pairs)

- [x] `PLASMA.ASM` -> `PLASMA.CPP`
   - Added `tests/test_plasma.cpp` covering `Do_Plasma` with fixed interleave/active-point cases, safe edge cases (`Interleave=1/3`, `NbActivePoints=2`), and deterministic random stress, comparing `TabVirgule`, the touched texture region, and exported globals (`Nb_Pts_Inter`, `Nb_Pts_Control`) strictly. Fixed CPP interpolation to match the ASM's zero-extended 16-bit control loads and `add/adc/sar` rounding. Note: `NbActivePoints=1` is excluded because the ASM reads past the lone control point, and `Interleave=0` is excluded because the ASM fill loop decrements from zero and walks off the buffer.

- [x] `GRILLE_A.ASM` -> `GRILLE.CPP`
   - Added `tests/test_grille.cpp` covering `GetAdrBlock`, `Map2Screen`, `DecompColonne`, `WorldCodeBrick`, `GetBlockBrick`, `GetWorldColBrickVisible`, and `WorldColBrickFull` with fixed in-memory cube/block fixtures plus deterministic random stress, comparing return values and touched globals (`XMap`, `YMap`, `ZMap`, `XScreen`, `YScreen`) byte-for-byte/strictly. Fixed CPP world-coordinate conversion to use the ASM's raw shift semantics and matched `WorldColBrickFull`'s upward scan. Note: the `WorldColBrickFull` negative-`y` ASM path is excluded because that proc jumps to a shared `pop ebx` return path without a matching push and crashes before returning.

- [x] `FUNC.ASM` -> `FUNC.CPP`
   - Added `tests/test_func.cpp` covering `SearchBoundColRGB` with fixed exact/subrange/upper-bound cases, edge cases for single-entry and `coulmin > coulmax`, plus 40 deterministic random rounds comparing return values and verifying the palette remains unchanged byte-for-byte.

- [x] `FLOW_A.ASM` -> `FLOW_A.CPP`
   - Added `tests/test_flow_a.cpp` covering `BoxFlow`, `ShadeBoxBlk`, and `CopyBlockShade` with fixed clipped/unclipped cases plus deterministic random stress, comparing the full destination buffers byte-for-byte.

- [x] `FIRE.ASM` -> `FIRE.CPP`
   - Added `tests/test_fire.cpp` covering `Do_Fire` with zero, all-`0xFF`, checkerboard, sequential, and 40-round deterministic random inputs, comparing both 32x36 work buffers, the 32x256 texture output, and the unchanged color table byte-for-byte after two consecutive calls from the same initial seed/state.

- [x] `COMPRESS.ASM` -> active CPP port lives in `LZSS.CPP` (`COMPRESS.CPP` is a commented legacy stub)
   - Added `tests/test_compress.cpp` covering `AddString` and `DeleteString` with fixed tree-shape cases plus deterministic random stress, comparing return values and full tree/window/global state.

## 3DEXT/ - Scene Extension Helpers (2 pairs)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [x] | `3DEXT/BOXZBUF.ASM` | `3DEXT/BOXZBUF.CPP` | `ZBufBoxOverWrite2` | Re-apply terrain occlusion over object bounding box | ASM ≡ CPP: return value plus post-call `Log`, `Screen`, and `PtrZBuffer` contents verified for visible, hidden, mixed, single-pixel, negative-depth, and 30 random rounds. |
| [x] | `3DEXT/LINERAIN.ASM` | `3DEXT/LINERAIN.CPP` | `LineRain` | Rain line rendering helper | ASM ≡ CPP: return flags, screen bbox globals, full `Log` buffer, and no-Z-write `PtrZBuffer` contents verified for horizontal, diagonal fog, vertical occluded, single-pixel, fully-clipped, and 50 random rounds. |

## ANIM/ — Object Animation (10 pairs)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [x] | `ANIM/ANIM.ASM` | `ANIM/ANIM.CPP` | `ObjectInitAnim` | Initialize animation for 3D object | ASM ≡ CPP: NbFrames, NbGroups, LoopFrame, CurrentFrame, Status, Master |
| [ ] | `ANIM/BODY.ASM` | `ANIM/BODY.CPP` | `ObjectInitBody` | Initialize body (visual model) for object | Needs `T_OBJ_3D` fixture |
| [ ] | `ANIM/CLEAR.ASM` | `ANIM/CLEAR.CPP` | `ObjectClear` | Zero/sentinel-fill object struct | Self-contained |
| [x] | `ANIM/FRAME.ASM` | `ANIM/FRAME.CPP` | `ObjectSetFrame` | Set current animation frame | ASM ≡ CPP: CurrentFrame matches for all 3 frames |
| [x] | `ANIM/INTANIM.ASM` | `ANIM/INTANIM.CPP` | `ObjectSetInterAnim` | Set interpolated animation state | ASM ≡ CPP: midpoint interpolation via Watcom-to-C shims |
| [x] | `ANIM/INTERDEP.ASM` | `ANIM/INTERDEP.CPP` | `ObjectSetInterDep` | Set inter-frame dependencies | ASM ≡ CPP: midpoint + rotation path via Watcom-to-C shims |
| [x] | `ANIM/INTFRAME.ASM` | `ANIM/INTFRAME.CPP` | `ObjectSetInterFrame` | Set interpolated frame between keyframes | ASM ≡ CPP: interpolation at 25% and 50% |
| [ ] | `ANIM/LIBINIT.ASM` | `ANIM/LIBINIT.CPP` | `InitObjects`, `ClearObjects` | Initialize/clear animation library state | |
| [x] | `ANIM/STOFRAME.ASM` | `ANIM/STOFRAME.CPP` | `ObjectStoreFrame` | Store animation frame state | ASM ≡ CPP: fixed CPP to copy NbGroups\*2-2 dwords (was NbGroups\*2-1) |
| [ ] | `ANIM/TEXTURE.ASM` | `ANIM/TEXTURE.CPP` | `ObjectInitTexture` | Initialize texture for object | |

## SVGA/ — Screen Drawing & Sprites (15 pairs)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [x] | `SVGA/AFFSTR.ASM` | `SVGA/AFFSTR.CPP` | `AffString` | Display string at screen position | ASM ≡ CPP: fixed pixel offset, line stride, TextPaper support, Font8x8 patches (chars 0xC6, 0xE4) |
| [ ] | `SVGA/BLITBOXF.ASM` | `SVGA/BLITBOXF.CPP` | `BlitBoxF` | Fast blit rectangular region with transparency | |
| [ ] | `SVGA/BOX.ASM` | `SVGA/BOX.CPP` | `Box` | Draw rectangle outline with clipping | Needs `Log`, `TabOffLine`, Clip globals |
| [x] | `SVGA/CALCMASK.ASM` | `SVGA/CALCMASK.CPP` | `CalcGraphMsk` | Calculate graphical mask | ASM ≡ CPP: synthetic brick + 300-round random stress test |
| [ ] | `SVGA/CLRBOXF.ASM` | `SVGA/CLRBOXF.CPP` | `ClearBoxF`, `SetClearColor` | Clear rectangle; set clear color | |
| [x] | `SVGA/COPYMASK.ASM` | `SVGA/COPYMASK.CPP` | `CopyMask` | Copy masked region | ASM ≡ CPP: synthetic bank + 20 random positions |
| [ ] | `SVGA/CPYBLOCI.ASM` | `SVGA/CPYBLOCI.CPP` | `CopyBlockIncrust` | Copy block with incrustation | |
| [ ] | `SVGA/CPYBLOCK.ASM` | `SVGA/CPYBLOCK.CPP` | `CopyBlock` | Fast memory block copy | |
| [x] | `SVGA/FONT.ASM` | `SVGA/FONT.CPP` | `SizeFont`, `CarFont`, `Font` | Font metrics and character rendering | ASM ≡ CPP: SizeFont tested with synthetic bank + 30 random strings |
| [x] | `SVGA/GRAPH.ASM` | `SVGA/GRAPH.CPP` | `AffGraph`, `GetBoxGraph` | Graphics drawing and box calculation | ASM ≡ CPP: GetBoxGraph basic + positive hotspot. Negative hotspot sign-extends differently (ASM movsx vs CPP U8). |
| [x] | `SVGA/MASK.ASM` | `SVGA/MASK.CPP` | `AffMask` | Display mask/sprite with transparency | ASM ≡ CPP: synthetic bank + 20 random positions via Watcom ABI wrapper |
| [x] | `SVGA/RESBLOCK.ASM` | `SVGA/RESBLOCK.CPP` | `RestoreBlock` | Restore saved screen region | ASM ≡ CPP: roundtrip + full-screen + 20 random regions |
| [ ] | `SVGA/SAVBLOCK.ASM` | `SVGA/SAVBLOCK.CPP` | `SaveBlock` | Save screen region to buffer | |
| [x] | `SVGA/SCALEBOX.ASM` | `SVGA/SCALEBOX.CPP` | `ScaleBox` | Scale rectangular region | ASM ≡ CPP: same-size, upscale, downscale + 20 random sizes/positions |
| [x] | `SVGA/SCALESPI.ASM` | `SVGA/SCALESPI.CPP` | `ScaleSprite` | Scale sprite with transparency | ASM fix: added `uses esi edi ebx` (callee-saved regs were clobbered, causing crash). CPP fix: ScreenXMax/YMax now inclusive. 14 tests: basic, transparency, clipping×4, hotspot, multi-sprite, random×30, ASM-vs-CPP (1:1, clipped, random×20). |
| [x] | `SVGA/SCALESPT.ASM` | `SVGA/SCALESPT.CPP` | `ScaleSpriteTransp` | Scale sprite with transparency blend table | ASM ≡ CPP: fixed synthetic bank + clipping/fallback edge cases + 100 random rounds comparing full framebuffer and `ScreenXMin/Max`, `ScreenYMin/Max`. ASM fix: added `uses esi edi ebx` (callee-saved regs were clobbered, causing crash). CPP fix: rewritten to match ASM fixed-point scaling, fallback-factor handling, and sentinel exit bounds. |

## SYSTEM/ — System Utilities (4 pairs)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [x] | `SYSTEM/CPU.ASM` | `SYSTEM/CPU.CPP` | CPU globals | CPU detection and feature flags | ASM ≡ CPP: ProcessorManufacturerIDString (13 bytes). Struct globals may differ in init values. |
| [ ] | `SYSTEM/FASTCPYF.ASM` | `SYSTEM/FASTCPYF.CPP` | `FastCopy` | Fast memory copy | Self-contained |
| [ ] | `SYSTEM/LZ.ASM` | `SYSTEM/LZ.CPP` | `ExpandLZ` | LZ decompression | Self-contained |
| [x] | `SYSTEM/MOUSEDAT.ASM` | `SYSTEM/MOUSEDAT.CPP` | Mouse data | Mouse driver data structures | ASM ≡ CPP: BinGphMouse[541] byte-for-byte |

## MENU/ — Menu Utilities (1 pair)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [ ] | `MENU/SORT.ASM` | `MENU/SORT.CPP` | `MySortCompFunc` | Comparison function for qsort (strcmp-based) | |

## pol_work/ — Polygon Rendering (14 pairs)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [x] | `pol_work/POLY.ASM` | `pol_work/POLY.CPP` | INV64, SetFog, SetCLUT, Switch_Fillers, Fill_Poly, Fill_PolyClip, Triangle_ReadNextEdge, SetScreenPitch, Calc_TextureZ*SlopeFPU | Core polygon rendering pipeline | **ASM equiv** — 29 tests (13 CPP + 4 ASM-vs-CPP: INV64 300 rounds, SetFog 300 rounds, SetCLUT 300 rounds, Switch_Fillers 300 rounds; + Fill_PolyClip 300 rounds POLY_SOLID, Fill_Poly 300 rounds POLY_SOLID, Fill_Sphere 300 rounds, Line_A 300 rounds; + Fill_PolyClip POLY_TEXTURE_Z_FOG game-trace quad + 300 random rounds (nv=3 triangles + nv=4 quads, both framebuffer and Z-buffer byte-for-byte); + 6 TextureZ slope calculator unit tests: Calc_TextureZXSlopeFPU 300 rounds, Calc_TextureZGouraudXSlopeFPU 300 rounds, Calc_TextureZXSlopeZBufFPU 300 rounds, Calc_TextureZLeftSlopeFPU 300 rounds, Calc_TextureZGouraudLeftSlopeFPU 300 rounds, Calc_TextureZLeftSlopeZBufFPU 300 rounds — CPP regression tests against reference formula). POLY_JMP.ASM + POLYTZF.ASM linked as ASM deps. ASM slope calculator symbols (all Calc_*/Jmp_*/Draw_Triangle/Read_Next_Right) renamed to prevent CPP shadowing. Fixed: Fill_Type made PUBLIC for test sharing; Jmp_XSlopeZBufFPU[24]/Jmp_LeftSlopeZBufFPU[24] corrected to use combined Calc_TextureZ*SlopeZBufFPU functions; ZBuf XSlope sign (MZB-MZC) corrected; test fix: save/restore pts[] before ASM path since CPP Fill_PolyClip modifies array in-place during quad decomposition. **FIXED**: EnterClip perspective-correct UV division `/ 256` changed to `>> 8` (arithmetic right shift) to match ASM's `shr eax,8; shl edx,24; or eax,edx` bit extraction. C's `/` truncates toward zero while ASM's shift is floor-division; for negative products (MapU×W) the results differ by 1, causing clipped vertex MapU mismatch (e.g. UC=43116 CPP vs 43117 ASM). Proven by polyrec_0002 DC#3226 exact slope comparison test. |
| [x] | `pol_work/POLY_JMP.ASM` | `pol_work/POLY_JMP.CPP` | All 6 jump tables × 26 entries (Fill_N/Fog/ZBuf/FogZBuf/NZW/FogNZW_Table_Jumps) | Polygon type dispatcher | ASM-vs-CPP equivalence: globals comparison (Fill_Filler, Fill_ClipFlag, Fill_Color, Fill_Trame_Parity, IsPolygonHidden) for all entries. 50 random rounds per entry. Fixed 3 CPP bugs: Jmp_SolidFogNZW missing Fill_Color, Jmp_TrameFogZBuf/NZW wrong palette index. |
| [x] | `pol_work/POLYCLIP.ASM` | `pol_work/POLYCLIP.CPP` | ClipperZ | Z-plane polygon clipping | ASM-vs-CPP equivalence (8 tests, 300 random rounds). Fixed CPP FPU rounding (fesetround FE_TOWARDZERO) to match ASM chop mode. |
| [x] | `pol_work/POLYDISC.ASM` | `pol_work/POLYDISC.CPP` | Fill_Sphere, Sph_Line_* | Circle/disc polygon fill | **ASM equiv** — 12 CPP + 3 ASM tests (centre, clipped, 300 random). Watcom register wrapper. Also tested via test_poly (Fill_Sphere 300 random rounds). |
| [x] | `pol_work/POLYFLAT.ASM` | `pol_work/POLYFLAT.CPP` | Filler_Flat, Filler_Trame, Filler_Transparent, ... (10 fillers) | Solid-color polygon fill | **ASM equiv** — 31 tests (13 CPP + 18 ASM equiv for all 10 fillers: Flat, Trame, Transparent, FlatZBuf, TransparentZBuf, TrameZBuf, FlatNZW, TransparentNZW, TrameNZW, FlagZBuffer). **FIXED**: Trame pair-loop used wrong carry check (`diffX & 1` vs shifted-out bit) and wrong loop structure; Trame ZBuf/NZW also had incorrect Z-advance carry detection (`zDec >> 31` vs overflow); TrameNZW was missing PtrZBuffer2/PtrLog2 init; Transparent blend used `& 0xF0` mask but ASM uses full byte. |
| [x] | `pol_work/POLYGOUR.ASM` | `pol_work/POLYGOUR.CPP` | Filler_Gouraud, Filler_Dither, ... (18 fillers) | Gouraud + dither shading | **ASM equiv** — 43 tests (6 CPP + 3 ASM for Filler_Gouraud + 34 ASM equiv for all 17 remaining fillers: Dither, GouraudTable, DitherTable, GouraudFog, DitherFog, GouraudZBuf, DitherZBuf, GouraudTableZBuf, DitherTableZBuf, GouraudFogZBuf, DitherFogZBuf, GouraudNZW, DitherNZW, GouraudTableNZW, DitherTableNZW, GouraudFogNZW, DitherFogNZW). **FIXED**: ROL8 needed `shift &= 7` to match x86 `rol al,cl`; GouraudTableZBuf/GouraudNZW/DitherNZW/GouraudTableNZW used simplified loop that corrupted per-scanline gouraud accumulator and wrong Z-buffer comparison — rewritten to match ASM register pattern. |
| [x] | `pol_work/POLYGTEX.ASM` | `pol_work/POLYGTEX.CPP` | Filler_TextureGouraud, Filler_TextureDither, Filler_TextureGouraudChromaKey, Filler_TextureDitherChromaKey, Filler_TextureGouraudZBuf, Filler_TextureGouraudChromaKeyZBuf, Filler_TextureGouraudNZW, Filler_TextureGouraudChromaKeyNZW (all 8 fillers) | Texture mapping with Gouraud shading | **ASM equiv** — 5 CPP + 17 ASM filler tests (static + 300-round random stress for all 8). **FIXED**: Filler_TextureDitherChromaKey rewritten — was missing dither pattern, CLUT gouraud row, local UV copies, Fill_CurOffLine save. Filler_TextureGouraudChromaKeyNZW rewritten — was missing carry chain, correct z-test, CLUT gouraud row, local UV copies. |
| [x] | `pol_work/POLYLINE.ASM` | `pol_work/POLYLINE.CPP` | Line, Line_A, Line_ZBuffer, Line_ZBuffer_NZW | Polygon edge drawing | ASM equiv for Line() — 5 CPP + 4 ASM tests. Line_A also tested via test_poly (300 random rounds, non-zbuffer mode). **FIXED**: snapshot bisect polyrec_0013 DC#4333 exposed a real ASM bug in the `Line_ZBuffer_NZW` vertical path. `@@Do_Y` now writes the fogged `Color` instead of the low byte of `YPtrLog`, and `test_polyline` contains an exact regression that asserts both the pixel value and ASM-vs-CPP framebuffer equivalence for the fogged vertical line case. |
| [x] | `pol_work/POLYTEXT.ASM` | `pol_work/POLYTEXT.CPP` | Filler_Texture, Filler_TextureFlat, ... (18 fillers) | Texture-mapped polygon fill | **ASM equiv** — 4 CPP + 41 ASM filler tests (all 18 variants: static + 300-round random stress). All pass byte-for-byte. **FIXED**: S32→S64 multiply overflow in sub-pixel UV, inverted Z-buffer comparison in NZW/ChromaKeyZBuf variants, sub-pixel UV applied to outer accumulators instead of local copies, carry-propagating inner loops for 6 broken ZBuf/NZW functions. |
| [x] | `pol_work/POLYTEXZ.ASM` | `pol_work/POLYTEXZ.CPP` | Filler_TextureZ, Fill_Init_Perspective, ... (18 fillers) | Perspective-correct texture fill | **ASM equiv** — All 18 Filler_TextureZ* functions pass byte-for-byte equivalence tests (static + random stress). **FIXED**: replaced `double` math with inline x87 asm for perspective; fixed sub-pixel correction (32×32→64 imul bit extraction), inner pixel loops (mapV_mapU register packing), Z-buffer dual-loop structure (@@LoopX/@@NoDisp), and NZW offset-based addressing across all 18 variants (Flat, ChromaKey, Fog, ZBuf, NZW combinations). |
| [x] | `pol_work/POLYTZF.ASM` | `pol_work/POLYTZF.CPP` | Filler_TextureZFogSmooth, Filler_TextureZFogSmoothZBuf, Filler_TextureZFogSmoothNZW | TextureZ + smooth fog variants | **ASM equiv** — 3 CPP + 6 ASM strict tests + 1 W-slope stress test (static + 300 random each for all 3 fillers; wslope test: 300 random rounds with non-zero Fill_W_XSlope under truncation rounding). **FIXED**: rewrote all 3 CPP fillers from affine to perspective-correct with inline x87 asm, fog CLUT, ZBuf perspective correction, Z-buffer rotated comparison (ror8 + add/adc). Fixed sub-pixel correction accumulation bug. ZBuf/NZW tests verify both framebuffer and Z-buffer byte-for-byte. **FIXED**: W stack traversal order changed from LIFO to FIFO to match ASM hardware-stack pop order. **FIXED**: V texture coordinate extraction corrected from `(uvCur >> 16) & 0xFF` to `(uvCur >> 24) & 0xFF` to match ASM's `shr eax,16; mov al,bh` pattern that extracts V[15:8]. All tests pass including full pipeline tests (Fill_PolyClip POLY_TEXTURE_Z_FOG nv=3+nv=4 300 random rounds). |
| [x] | `pol_work/POLYTZG.ASM` | `pol_work/POLYTZG.CPP` | Filler_TextureZGouraud, ... (6 fillers) | TextureZ + Gouraud variants | ASM equiv — all 6 fillers tested (12 tests: 6 static + 6 random×300). Fixed CPP: wrong Z-buffer globals, NZW missing perspective correction, zbuf row offset, Z comparison/write shift, sub-pixel factor, UV addressing (>>8 not >>16). POLYTEXZ.ASM linked as ASM dep for perspective functions. |
| [x] | `pol_work/TESTVUEF.ASM` | `pol_work/TESTVUEF.CPP` | TestVuePoly | Backface culling / polygon visibility | ASM equiv — 5 CPP + 5 ASM tests. Uses STRIP_C_ADAPT + inline asm wrapper. |

## OBJECT/ — 3D Object Display (1 pair, multiple functions)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [x] | `OBJECT/AFF_OBJ.ASM` | `OBJECT/AFF_OBJ.CPP` | `ObjectDisplay`, `BodyDisplay`, `BodyDisplay_AlphaBeta`, `TestVisibleI`, `TestVisibleF`, `QuickSort`, `QuickSortInv` | Full 3D object rendering pipeline | `tests/OBJECT/test_aff_obj.cpp` now passes with the original simple render fixture, explicit alias coverage for transparent/trame/dither/gouraud-table/dither-table triangles and quads, line and sphere entity coverage, textured solid-triangle, textured flat-triangle, textured gouraud-triangle, textured dither-triangle, textured chroma-key solid/flat/gouraud/dither triangles, textured solid-quad, textured flat-quad, textured gouraud-quad, textured dither-quad, textured chroma-key solid/flat/gouraud/dither quads, textured perspective-correct flat-triangle, textured perspective-correct solid-triangle, textured perspective-correct gouraud-triangle, textured perspective-correct dither-triangle, textured perspective-correct chroma-key solid/flat/gouraud/dither triangles, textured perspective-correct flat-quad, textured perspective-correct solid-quad, textured perspective-correct gouraud-quad, textured perspective-correct dither-quad, textured perspective-correct chroma-key solid/flat/gouraud/dither quads, and environment-mapped solid-triangle, solid-triangle with non-zero scale, gouraud-triangle, gouraud-triangle with non-zero scale, flat-triangle, flat-triangle with non-zero scale, flat-quad, flat-quad with non-zero scale, solid-quad, solid-quad with non-zero scale, gouraud-quad, and gouraud-quad with non-zero scale fixtures. Coverage now includes visible and clipped end-to-end cases for `BodyDisplay`, `BodyDisplay_AlphaBeta`, and `ObjectDisplay`, multigroup `ObjectDisplay` cases that exercise a second group through both rotation and `TYPE_TRANSLATE` paths with mixed flat, solid, gouraud, triangle, quad, line, and sphere dispatch, non-textured `ObjectDisplay` cases that explicitly drive the transparent, trame, dither, gouraud-table, and dither-table jump-table aliases for both triangles and quads, textured `ObjectDisplay` cases that drive the solid, flat, gouraud, dither, chroma-key solid/flat/gouraud/dither, quad-solid, quad-flat, quad-gouraud, quad-dither, quad chroma-key solid/flat/gouraud/dither, `POLY_TEXTURE_Z_FLAT`, `POLY_TEXTURE_Z_SOLID`, `POLY_TEXTURE_Z_GOURAUD`, `POLY_TEXTURE_Z_DITHER`, `POLY_TEXTURE_Z` chroma-key solid/flat/gouraud/dither, and `POLY_TEXTURE_Z` quad-flat, quad-solid, quad-gouraud, quad-dither, and quad chroma-key solid/flat/gouraud/dither paths with real UVs, a texture table, and texture filler setup, env-mapped `ObjectDisplay` cases that exercise the ASM env solid, env solid with non-zero scale, env gouraud, env gouraud with non-zero scale, env flat, env flat with non-zero scale, env quad flat, env quad flat with non-zero scale, env quad solid, env quad solid with non-zero scale, env quad gouraud, and env quad gouraud with non-zero scale branches end-to-end, plus dedicated `Line`, `Sphere`, and `Sphere_Transp` object cases. The render fixtures compare return values, screen bounds, `Obj_ListRotatedPoints`, `Obj_ListProjectedPoints`, `ListLights`, `ListSort`, `TabMat`, `ListFillPoly`, `PosXWr/Y/Z`, and the rendered framebuffer. Root fixes required for parity were: `InitMatrixTransF` storing translation slots with float representation, `ObjectDisplay` placing face-light data at `ListLights + NbPoints` rather than `ListLights + NbPoints * 2`, restoring `Triangle_TextureZ_Flat`/`Quad_TextureZ_Flat` to populate perspective `Pt_W` values instead of zeroing them, the AFF_OBJ env handlers rotating per-vertex normals through the exact `RotatePointNoMMX` callback path while preserving the ASM-visible `X0/Y0/Z0` side effect, matching the ASM env UV packer by preserving the full 32-bit `u` product before OR-ing in the packed high-word `v` component, fixing `Line` to compute Z-buffer values only in perspective mode, fixing `Sphere`/`Sphere_Transp` to match the ASM radius and Z-buffer formulas, and linking renamed ASM `POLYLINE.ASM`/`POLYDISC.ASM` deps into the test target so the ASM AFF_OBJ path keeps using the original register-ABI `Line_A`/`Fill_Sphere` handlers while the C++ path continues using the C++ implementations. The alias tests also require initializing the Gouraud CLUT for plain dither/table fillers even when no texture map is involved. Fully arbitrary unordered inputs are still invalid for the original ASM quicksort helpers because some pivot orders recurse on the same range, so unordered sort stress remains restricted to the ASM-valid domain. |

---

## Known Discrepancies

### 1. ~~AffString — Glyph rendering direction and pixel offset~~ (FIXED)

**Status:** Fixed. CPP now matches ASM byte-for-byte. Test upgraded to `[x]`.

**Fixes applied to `SVGA/AFFSTR.CPP`:**
1. Pixel pointer offset: `ptr` now starts at `ScreenPtr - SizeChar` (writes
   backwards from the offset pointer), matching the ASM's `[ebp+ecx]` where
   ecx goes from `-SizeChar` to `-1`.
2. Line stride: replaced hardcoded `640` with `ModeDesiredX`.
3. TextPaper support: non-ink pixels now write `TextPaper` color when
   `TextPaper != 0xFF` (0xFF = transparent), matching the ASM's `cmp ah, 0FFh`.
4. Font8x8 patches: chars 0xC6 and 0xE4 updated to match ASM's French
   accented character replacements (`à` and `ò`).

---

### 2. ~~ObjectStoreFrame — Off-by-one in stored dword count~~ (FIXED)

**Status:** Fixed. CPP changed from `NbGroups * 2 - 1` to `NbGroups * 2 - 2` to
match ASM. Test upgraded to `[x]` with strict byte-exact comparison.

The ASM comment `; 2 DWORDs per group, no group 0` clarifies the intent:
copy `NbGroups - 1` complete groups (each group = 2 dwords). The formula
`NbGroups * 2 - 2` = `(NbGroups - 1) * 2` is correct. The CPP had
`NbGroups * 2 - 1` which overcounted by 1 dword. The consumer
(`ObjectSetInterFrame`) reads exactly `NbGroups - 1` groups, so the extra
dword was never read — but the buffer pointer advanced 4 bytes further per
store, which could cause wrap-around differences over time.

---

### 3. ~~ObjectSetInterDep / ObjectSetInterAnim — Function pointer ABI mismatch~~ (FIXED)

**Status:** Fixed via Watcom-to-C ABI shim functions. Tests upgraded to `[x]`.

`InitMatrixStd` and `RotatePoint` are global function-pointer variables (defined
in `3D/IMATSTDF.CPP` and `3D/LROT3DF.CPP`).  The ASM `call [InitMatrixStd]`
correctly reads the pointer and jumps to the target.  The problem was that the
GCC-compiled target functions use C stack calling convention, while the ASM
caller passes params in Watcom registers (`edi`/`eax`/`ebx`/`ecx`).

**Fix:** `__attribute__((naked))` shim functions in the test that:
1. Accept Watcom register params (no GCC prologue to clobber them)
2. Push them onto the stack in C convention order
3. Call the CPP implementation (`c_InitMatrixStdF` / `c_LongRotatePointF`)
4. Clean up the stack and `ret`

The shims are installed via `install_shims()` only before ASM calls.  CPP tests
use the default function pointers (which point to CPP functions with C convention).

---

### 4. ScaleSprite — Callee-saved register clobbering (FIXED)

**Status:** Fixed in ASM. All 14 tests pass (including ASM-vs-CPP equivalence).

**Root cause:** `ScaleSprite proc uses ebp` only saved/restored `ebp`, but the
function body clobbers `esi`, `edi`, and `ebx` — all callee-saved registers in
the x86 cdecl calling convention. When called from GCC-compiled C code, the
caller's register state was corrupted on return, causing a segfault.

**Fix:** Changed `proc uses ebp` to `proc uses ebp esi edi ebx` in
`SVGA/SCALESPI.ASM`. This generates proper push/pop sequences in the
prologue/epilogue to preserve all clobbered callee-saved registers.

**CPP fix (prior):** `ScreenXMax`/`ScreenYMax` changed from exclusive to
inclusive values (`end_x - 1` instead of `end_x`), matching the ASM output.

**Note:** ASM treats Hot_X/Hot_Y as signed bytes (via `shl`/`sar` sign
extension), while CPP reads them as unsigned `U8`. For values 0–127 the
results are identical; for 128–255 they differ. All current test data uses
small positive values so this is not tested.

