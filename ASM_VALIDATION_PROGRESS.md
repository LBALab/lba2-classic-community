# ASM → C Validation Progress

Tracks equivalence testing between original x86 ASM implementations and their
C/C++ ports in `LIB386/`.  Each row represents a function (or data table) that
exists in both `.ASM` and `.CPP` form.

**Legend:**  `[ ]` untested · `[x]` tested · `[~]` partial

---

## 3D/ — 3D Math & Projection (23 pairs)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [ ] | `3D/COPYMATF.ASM` | `3D/COPYMATF.CPP` | `CopyMatrixF` | Copy 3×3 rotation matrix (float) | |
| [ ] | `3D/GETANG2D.ASM` | `3D/GETANG2D.CPP` | `GetAngleVector2D` | 2D angle from (x, z) via tangent table binary search | Depends on `TanTab` |
| [ ] | `3D/GETANG3D.ASM` | `3D/GETANG3D.CPP` | `GetAngleVector3D` | 3D angle from (x, y, z); writes `X0`, `Y0` | Depends on `GetAngleVector2D`, `QSqr` |
| [ ] | `3D/IMATSTDF.ASM` | `3D/IMATSTDF.CPP` | `InitMatrixStdF` | Init rotation matrix from Euler angles (α, β, γ) | Uses `SinTabF`/`CosTabF` |
| [ ] | `3D/IMATTRAF.ASM` | `3D/IMATTRAF.CPP` | `InitMatrixTransF` | Set matrix translation (TX, TY, TZ) | |
| [ ] | `3D/LIROT3DF.ASM` | `3D/LIROT3DF.CPP` | `LongInverseRotatePointF` | Inverse-rotate 3D point; writes `X0`, `Y0`, `Z0` | |
| [ ] | `3D/LITLISTF.ASM` | `3D/LITLISTF.CPP` | `LightList` | Compute per-vertex lighting from normals | Depends on `LongInverseRotatePoint`, light globals |
| [ ] | `3D/LPROJ3DF.ASM` | `3D/LPROJ3DF.CPP` | `ProjectList3DF` | Batch 3D perspective projection | Complex — screen bounds, `NearClip` |
| [ ] | `3D/LPROJISO.ASM` | `3D/LPROJISO.CPP` | `LongProjectPointIso` | Isometric projection; writes `Xp`, `Yp` | |
| [ ] | `3D/LROT2DF.ASM` | `3D/LROT2DF.CPP` | `LongRotate`, `Rotate` | 2D rotation by angle; writes `X0`, `Z0` | Uses `SinTabF`/`CosTabF` |
| [ ] | `3D/LROT3DF.ASM` | `3D/LROT3DF.CPP` | `LongRotatePointF` | 3D matrix × point; writes `X0`, `Y0`, `Z0` | |
| [ ] | `3D/MULMATF.ASM` | `3D/MULMATF.CPP` | `MulMatrixF` | 3×3 matrix multiplication (float) | |
| [ ] | `3D/PRLI3DF.ASM` | `3D/PRLI3DF.CPP` | `ProjectList3DF` | Batch 3D projection with screen bounds | Same function as LPROJ3DF |
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
| [~] | `SVGA/CALCMASK.ASM` | `SVGA/CALCMASK.CPP` | `CalcGraphMsk` | Calculate graphical mask | ASM ≡ CPP: synthetic brick → mask conversion |
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
| [~] | `pol_work/POLY.ASM` | `pol_work/POLY.CPP` | Fill_Poly, SetFog, INV64, Switch_Fillers, SetScreenPitch | Core polygon rendering pipeline | CPP tested via Fill_Poly integration (13 tests). No ASM equiv yet. |
| [~] | `pol_work/POLY_JMP.ASM` | `pol_work/POLY_JMP.CPP` | Jmp_Solid, Jmp_Trame, Jmp_Transparent, Jmp_Gouraud, ... (80+ dispatch functions) | Polygon type dispatcher | CPP tested via Fill_Poly dispatch (7 tests). No ASM equiv yet. |
| [~] | `pol_work/POLYCLIP.ASM` | `pol_work/POLYCLIP.CPP` | ClipperZ | Z-plane polygon clipping | CPP-only tests (3). ASM tests disabled — no ASM_SOURCE configured. |
| [~] | `pol_work/POLYDISC.ASM` | `pol_work/POLYDISC.CPP` | Fill_Sphere, Sph_Line_* | Circle/disc polygon fill | CPP tested (9 tests): center, radius, clipping, transparency, fog, random. |
| [~] | `pol_work/POLYFLAT.ASM` | `pol_work/POLYFLAT.CPP` | Filler_Flat, Filler_Trame, Filler_Transparent, ... (10 fillers) | Solid-color polygon fill | CPP tested via Fill_Poly (10 tests). No ASM equiv yet. |
| [~] | `pol_work/POLYGOUR.ASM` | `pol_work/POLYGOUR.CPP` | Filler_Gouraud, Filler_Dither, ... (18 fillers) | Gouraud + dither shading | CPP tested via Fill_Poly (6 tests). Requires PtrCLUTGouraud. |
| [~] | `pol_work/POLYGTEX.ASM` | `pol_work/POLYGTEX.CPP` | Filler_TextureGouraud, Filler_TextureDither, ... (8 fillers) | Texture mapping with Gouraud shading | CPP tested via Fill_Poly (5 tests). |
| [x] | `pol_work/POLYLINE.ASM` | `pol_work/POLYLINE.CPP` | Line, Line_A, Line_ZBuffer, Line_ZBuffer_NZW | Polygon edge drawing | ASM equiv for Line() — 5 CPP + 4 ASM tests. |
| [~] | `pol_work/POLYTEXT.ASM` | `pol_work/POLYTEXT.CPP` | Filler_Texture, Filler_TextureFlat, ... (18 fillers) | Texture-mapped polygon fill | Linkage + offscreen test only — on-screen rendering SEGFAULTs (texture pipeline init issue). |
| [~] | `pol_work/POLYTEXZ.ASM` | `pol_work/POLYTEXZ.CPP` | Filler_TextureZ, Fill_Init_Perspective, ... (20+ funcs) | Perspective-correct texture fill | Linkage + offscreen test only — same pipeline issue as POLYTEXT. |
| [~] | `pol_work/POLYTZF.ASM` | `pol_work/POLYTZF.CPP` | Filler_TextureZFogSmooth, ... (3 fillers) | TextureZ + smooth fog variants | CPP tested via Fill_Poly (3 tests). |
| [~] | `pol_work/POLYTZG.ASM` | `pol_work/POLYTZG.CPP` | Filler_TextureZGouraud, ... (6 fillers) | TextureZ + Gouraud variants | CPP tested via Fill_Poly (3 tests). |
| [x] | `pol_work/TESTVUEF.ASM` | `pol_work/TESTVUEF.CPP` | TestVuePoly | Backface culling / polygon visibility | ASM equiv — 5 CPP + 5 ASM tests. Uses STRIP_C_ADAPT + inline asm wrapper. |

## OBJECT/ — 3D Object Display (1 pair, multiple functions)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [ ] | `OBJECT/AFF_OBJ.ASM` | `OBJECT/AFF_OBJ.CPP` | `ObjectDisplay`, `BodyDisplay`, `TestVisibleI`, `TestVisibleF`, `QuickSort`, `QuickSortInv` | Full 3D object rendering pipeline | Very complex — heavy dependencies |

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

