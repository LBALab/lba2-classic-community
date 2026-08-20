# ASM to C++ reference

In the original codebase (lba2-classic), many performance-critical modules were written in assembly. In lba2-classic-community, some of these have been ported to C++ and are built instead of the ASM. The original ASM files remain in the repository but are not used in the default build.

This page is a quick reference for which modules are ported (built as CPP) vs still assembly in the current community build. It may change as more ports are completed. Build configuration: `SOURCES/CMakeLists.txt` and `SOURCES/3DEXT/CMakeLists.txt`.

## SOURCES (main game)

| Original ASM module | Community build | Notes |
|---------------------|-----------------|--------|
| COMPRESS.ASM        | COMPRESS.CPP    | Ported to C++. |
| FIRE.ASM            | FIRE.CPP        | Inline assembly in .CPP file; banner and French comment preserved from ASM. |
| FLOW_A.ASM          | FLOW_A.CPP      | Ported. FLOW.CPP is the main particle/game logic (existed in original). |
| FUNC.ASM            | FUNC.CPP        | Ported to C++. |
| GRILLE_A.ASM        | GRILLE_A.CPP    | Ported. GRILLE.CPP is the high-level grid logic (existed in original). |
| PLASMA.ASM          | PLASMA.CPP      | Ported to C++. |

**Present in repo but not in current build:**

| Module       | Notes |
|--------------|-------|
| DEC_XCF.ASM  | DOS-specific decoding. Listed in `SOURCES_FILES_ASM_DOS` but not added to the executable. |
| HERCUL_A.ASM | Hercules text display. Same as above. |
| KEYB.ASM     | DOS-specific keyboard handling. Same as above. |
| DEC.ASM      | Referenced in comments only. |
| COPY.ASM     | Referenced in comments only. |

## SOURCES/3DEXT (3D extensions)

| Original ASM module | Community build | Notes |
|---------------------|-----------------|--------|
| BOXZBUF.ASM         | BOXZBUF.CPP     | Ported to C++. |
| LINERAIN.ASM        | LINERAIN.CPP    | Ported to C++. |

## LIB386 (engine libraries)

LIB386 still contains many ASM files that are used in the build (3D, ANIM, SVGA, OBJECT, pol_work, SYSTEM, etc.). A full LIB386 ASM↔CPP map is not listed here; the focus of this reference is SOURCES and 3DEXT where the community has explicitly switched to C++ in the build. LIB386 ports can be documented here later if desired.

## Known mistranslation classes

Three ways a faithful-looking port has been wrong, each found more than once. Check a port against
these before believing it, because all three compile cleanly, and two of them produce plausible
output rather than obvious damage.

### One-operand `imul` / `mul` feeding `idiv` is 64-bit

`imul r/m32`, with one operand and no comma, multiplies EAX by the operand and leaves the **64-bit**
product in EDX:EAX; a following `idiv r/m32` divides that whole 64-bit value. C's `a * b / c` in
`S32` cannot express it, because the product truncates before the divide. **The comma is the tell**:
the two-operand form (`imul eax, ebx`) really does truncate to 32 bits and maps to `*` correctly.

Correct translation: `(S32)((S64)a * b / c)`.

A sweep of every one-operand multiply with a divide within a few instructions found 95 that matter,
across five files. `LIB386/pol_work/POLYLINE.ASM` (56 sites) and `LIB386/OBJECT/AFF_OBJ.ASM` (1) had
been written 32-bit and are now `S64`; `LINERAIN.ASM`, `POLY.ASM` and `REGLE3.ASM` were already
correct. Of the 271 one-operand multiplies in the tree, only those feeding a divide matter: a
product whose high half is never read is just a 32-bit multiply.

Sort live from latent before spending effort. POLYLINE's `DZ` is a 16.16 delta, so it overflows at
ordinary scene depths; AFF_OBJ's needs a sphere radius in the millions against a focal length of
600.

### `neg` is two's complement, not `~`

`neg eax` is `-x`. Porting it as `~x` loses the `+1`, which is invisible at most magnitudes and
decisive at small ones: the interior sphere radius in `AFF_OBJ.CPP` came out exactly one pixel too
large, and on 1 to 3 pixel character eyes that is the difference between a dot and a blob (#357).
The exterior branch had a compensating second `neg`, so the symptom was interior-only.

### U32 geometry relying on 32-bit address wraparound

Watcom-era blitters "clip" a negative coordinate by letting unsigned arithmetic wrap:

```c
U32 xMin = HotX + x;                  /* x can be negative */
if (xMin < ClipXMin || xMax > ClipXMax) { /* margin clip */ }
U32 initialOffset = TabOffLine[yMin] + xMin;
U8 *screen = Log + initialOffset;     /* 32-bit: wraps back. 64-bit: Log + 4 GiB */
```

Two coupled defects. The unsigned compare hides a negative `xMin`, so the clip branch never fires;
and `U32` zero-extends to a 64-bit pointer instead of wrapping, so the write lands 4 GiB away.
Intermittent by nature, because that address is only sometimes mapped: a segfault when you are
lucky, silent corruption when you are not (#78, `LIB386/SVGA/COPYMASK.CPP`). Convert geometry locals
to `S32`. Audit anything doing RLE or sprite blits with `HotX`/`HotY` headers and screen-relative x;
`DrawOverBrick3` in `SOURCES/GRILLE.CPP` passes `colscreen = -24` for column 0.

### Why grepping for the pattern is not enough

Both misses in the 64-bit multiply sweep sat beside code that already knew the hazard, one of them
two lines under a previous fix to the same function, with a test whose reference model was correct
and whose inputs were too small to overflow. Reaching these needs inputs sized from the arithmetic:
work out what makes the product exceed 2^31 and test on both sides of that boundary. The same
applies per render mode, since an equivalence suite that only drives the perspective path never
touches the iso branch. See [BUG_HUNTING.md](BUG_HUNTING.md) for the oracle discipline this feeds.
