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
| [~] | `ANIM/INTANIM.ASM` | `ANIM/INTANIM.CPP` | `ObjectSetInterAnim` | Set interpolated animation state | CPP-only tested; ASM calls InitMatrixStd via DWORD func ptr (segfaults) |
| [~] | `ANIM/INTERDEP.ASM` | `ANIM/INTERDEP.CPP` | `ObjectSetInterDep` | Set inter-frame dependencies | CPP-only tested; ASM calls InitMatrixStd/RotatePoint via DWORD func ptrs |
| [x] | `ANIM/INTFRAME.ASM` | `ANIM/INTFRAME.CPP` | `ObjectSetInterFrame` | Set interpolated frame between keyframes | ASM ≡ CPP: interpolation at 25% and 50% |
| [ ] | `ANIM/LIBINIT.ASM` | `ANIM/LIBINIT.CPP` | `InitObjects`, `ClearObjects` | Initialize/clear animation library state | |
| [~] | `ANIM/STOFRAME.ASM` | `ANIM/STOFRAME.CPP` | `ObjectStoreFrame` | Store animation frame state | ASM stores NbGroups\*2-2 dwords; CPP stores NbGroups\*2-1. Common portion matches. |
| [ ] | `ANIM/TEXTURE.ASM` | `ANIM/TEXTURE.CPP` | `ObjectInitTexture` | Initialize texture for object | |

## SVGA/ — Screen Drawing & Sprites (15 pairs)

| Status | ASM File | CPP File | Function(s) | Description | Notes |
|--------|----------|----------|-------------|-------------|-------|
| [x] | `SVGA/AFFSTR.ASM` | `SVGA/AFFSTR.CPP` | `AffString` | Display string at screen position | ASM ≡ CPP: fixed pixel offset, line stride, TextPaper support, Font8x8 patches (chars 0xC0, 0xCC) |
| [ ] | `SVGA/BLITBOXF.ASM` | `SVGA/BLITBOXF.CPP` | `BlitBoxF` | Fast blit rectangular region with transparency | |
| [ ] | `SVGA/BOX.ASM` | `SVGA/BOX.CPP` | `Box` | Draw rectangle outline with clipping | Needs `Log`, `TabOffLine`, Clip globals |
| [ ] | `SVGA/CALCMASK.ASM` | `SVGA/CALCMASK.CPP` | `CalcGraphMsk` | Calculate graphical mask | |
| [ ] | `SVGA/CLRBOXF.ASM` | `SVGA/CLRBOXF.CPP` | `ClearBoxF`, `SetClearColor` | Clear rectangle; set clear color | |
| [ ] | `SVGA/COPYMASK.ASM` | `SVGA/COPYMASK.CPP` | `CopyMask` | Copy masked region | |
| [ ] | `SVGA/CPYBLOCI.ASM` | `SVGA/CPYBLOCI.CPP` | `CopyBlockIncrust` | Copy block with incrustation | |
| [ ] | `SVGA/CPYBLOCK.ASM` | `SVGA/CPYBLOCK.CPP` | `CopyBlock` | Fast memory block copy | |
| [ ] | `SVGA/FONT.ASM` | `SVGA/FONT.CPP` | `SizeFont`, `CarFont`, `Font` | Font metrics and character rendering | |
| [ ] | `SVGA/GRAPH.ASM` | `SVGA/GRAPH.CPP` | `AffGraph`, `GetBoxGraph` | Graphics drawing and box calculation | |
| [ ] | `SVGA/MASK.ASM` | `SVGA/MASK.CPP` | `AffMask` | Display mask/sprite with transparency | |
| [x] | `SVGA/RESBLOCK.ASM` | `SVGA/RESBLOCK.CPP` | `RestoreBlock` | Restore saved screen region | ASM ≡ CPP: full-screen roundtrip |
| [ ] | `SVGA/SAVBLOCK.ASM` | `SVGA/SAVBLOCK.CPP` | `SaveBlock` | Save screen region to buffer | |
| [x] | `SVGA/SCALEBOX.ASM` | `SVGA/SCALEBOX.CPP` | `ScaleBox` | Scale rectangular region | ASM ≡ CPP: same-size, upscale 10×10→30×30, downscale 4×4→2×2 |
| [ ] | `SVGA/SCALESPI.ASM` | `SVGA/SCALESPI.CPP` | `ScaleSprite` | Scale sprite with transparency | |

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
| [ ] | `pol_work/POLY.ASM` | `pol_work/POLY.CPP` | Polygon data/tables | Core polygon rendering data + structures | Data + state |
| [ ] | `pol_work/POLY_JMP.ASM` | `pol_work/POLY_JMP.CPP` | Jump/dispatch tables | Polygon type dispatcher | |
| [ ] | `pol_work/POLYCLIP.ASM` | `pol_work/POLYCLIP.CPP` | Polygon clipping | Clip polygon to viewport bounds | |
| [ ] | `pol_work/POLYDISC.ASM` | `pol_work/POLYDISC.CPP` | Disc polygon rendering | Circle/disc polygon fill | |
| [ ] | `pol_work/POLYFLAT.ASM` | `pol_work/POLYFLAT.CPP` | Flat polygon fillers | Solid-color polygon fill (multiple variants) | |
| [ ] | `pol_work/POLYGOUR.ASM` | `pol_work/POLYGOUR.CPP` | Gouraud/dither fillers | Gouraud + dither shading (16+ variants) | Complex |
| [ ] | `pol_work/POLYGTEX.ASM` | `pol_work/POLYGTEX.CPP` | Gouraud + texture fillers | Texture mapping with Gouraud shading | |
| [ ] | `pol_work/POLYLINE.ASM` | `pol_work/POLYLINE.CPP` | Polygon line rendering | Polygon edge drawing | |
| [ ] | `pol_work/POLYTEXT.ASM` | `pol_work/POLYTEXT.CPP` | Textured polygon fillers | Texture-mapped polygon fill | |
| [ ] | `pol_work/POLYTEXZ.ASM` | `pol_work/POLYTEXZ.CPP` | Z-buffered texture fillers | Texture + Z-buffer polygon fill | |
| [ ] | `pol_work/POLYTZF.ASM` | `pol_work/POLYTZF.CPP` | Texture + Z-buffer variants | Additional Z-buf texture fill variants | |
| [ ] | `pol_work/POLYTZG.ASM` | `pol_work/POLYTZG.CPP` | Texture + Z-buf + Gouraud | Combined shading variants | |
| [ ] | `pol_work/TESTVUEF.ASM` | `pol_work/TESTVUEF.CPP` | Visibility testing | Backface culling / polygon visibility | |

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
4. Font8x8 patches: chars 0xC0 and 0xCC updated to match ASM's French
   accented character replacements (`à` and `ò`).

---

### 2. ObjectStoreFrame — Off-by-one in stored dword count

**Files:** `ANIM/STOFRAME.ASM` vs `ANIM/STOFRAME.CPP`

When storing the current animation frame to the circular buffer, the ASM and CPP
versions copy a different number of 32-bit words from `CurrentFrame[]`.

**ASM** (STOFRAME.ASM):
```asm
mov  ecx, [ebx].NbGroups
lea  ecx, [ecx*2-2]          ; count = NbGroups × 2 - 2
rep  movsd                    ; copy `count` dwords
```

**CPP** (STOFRAME.CPP):
```c
U32 nbGroups = obj->NbGroups;
for (U32 i = 0; i < nbGroups * 2 - 1; i++)    // count = NbGroups × 2 - 1
    *dst++ = *src++;
```

**Concrete example with `NbGroups = 3`:**

Each `T_GROUP_INFO` is 8 bytes (2 dwords: `{Type+Alpha}` and `{Beta+Gamma}`).

| Version | Formula | Dwords | Bytes | Groups stored |
|---------|---------|--------|-------|---------------|
| ASM | `3×2-2 = 4` | 4 | 16 | 2 complete groups (groups 1–2) |
| CPP | `3×2-1 = 5` | 5 | 20 | 2 complete groups + 4 extra bytes (partial group 3) |

Both versions write a 16-byte header (4 zero dwords) before the frame data.
The first 32 bytes (header + 2 groups) are **identical** between ASM and CPP.
The CPP version writes one additional dword that the ASM version does not.

**Impact:** The circular buffer pointer (`PtrLib3DBufferAnim`) advances by 4
extra bytes per store in CPP.  Over many animation frames this could cause the
CPP buffer to wrap at a different point than ASM.  The extra dword is harmless
if never read, but could theoretically shift subsequent stored frames in memory.

**Suggested fix for CPP:** Change `nbGroups * 2 - 1` to `nbGroups * 2 - 2` to
match the ASM. Update the buffer pointer advance accordingly.

---

### 3. ObjectSetInterDep / ObjectSetInterAnim — Function pointer ABI mismatch

**Files:** `ANIM/INTERDEP.ASM` vs `ANIM/INTERDEP.CPP`  
(also affects `ANIM/INTANIM.ASM` which calls `ObjectSetInterDep`)

The ASM version calls `InitMatrixStd` and `RotatePoint` through **indirect
DWORD function pointers**, while the CPP version calls `InitMatrixStdF` /
`LongRotatePointF` directly.

**ASM** (INTERDEP.ASM):
```asm
EXTRN C InitMatrixStd:  DWORD        ; declared as a DWORD *variable*
EXTRN C RotatePoint:    DWORD
; ...
call  [InitMatrixStd]                ; indirect call: read DWORD at &InitMatrixStd,
                                     ;   then CALL that address
call  [RotatePoint]
```

**CPP** (INTERDEP.CPP):
```c
extern void InitMatrixStdF(S32 alpha, S32 beta, S32 gamma);
extern void LongRotatePointF(S32 x, S32 y, S32 z);
// ...
InitMatrixStdF(alpha, beta, gamma);  // direct call
LongRotatePointF(x, y, z);
```

**Why it segfaults in test binaries:**

In the original Watcom-compiled game, `InitMatrixStd` and `RotatePoint` were
**global function-pointer variables** — 4-byte memory locations containing the
address of the actual function.  The ASM instruction `call [InitMatrixStd]`
performs an indirect call: it reads the 4-byte value stored at `InitMatrixStd`
and jumps to that address.

In our GCC/Linux test binary, `InitMatrixStd` resolves to a regular function
symbol.  Its address points to the function's **machine code**.  When the ASM
executes `call [InitMatrixStd]`, it reads the first 4 bytes of the function's
prologue (e.g., `push ebp; mov ebp, esp` = `0x8B EC 55 89`) as a memory
address and jumps there — instant segfault.

**Diagram:**

```
Original game (Watcom):
  InitMatrixStd:  dd  offset _InitMatrixStdF    ; pointer variable
  call [InitMatrixStd]  →  reads pointer  →  jumps to _InitMatrixStdF  ✓

Test binary (GCC):
  InitMatrixStd:  push ebp / mov ebp,esp / ...  ; function body
  call [InitMatrixStd]  →  reads 0x8BEC5589     →  jumps to garbage  ✗
```

**Suggested fix:** Create DWORD function-pointer shims in the test binary:
```c
void (*InitMatrixStd)(S32, S32, S32)   = InitMatrixStdF;
void (*RotatePoint)(S32, S32, S32)     = LongRotatePointF;
```
This would make the ASM's `call [InitMatrixStd]` work correctly, enabling full
ASM-vs-CPP equivalence testing for `ObjectSetInterDep` and `ObjectSetInterAnim`.
