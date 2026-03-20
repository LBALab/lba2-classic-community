# ASM Test Coverage Audit

This document tracks the breadth-first audit of the ASM/CPP pairs listed in
ASM_VALIDATION_PROGRESS.md. The goal is to treat existing `[x]` entries as
"already under test" rather than "fully explored", then strengthen coverage so
missing branches, edge inputs, side effects, and valid-domain boundaries are
explicitly exercised.

## Audit Rubric

For each listed ASM/CPP pair:

1. Map the implementation branches, dispatch paths, globals written, mutated
   buffers, and source-preservation expectations.
2. Verify fixed cases cover at least:
   - normal path
   - sign and quadrant transitions where applicable
   - zero, one, min, max, or boundary inputs in the valid ASM domain
   - clipping, overflow, sentinel, or early-return paths where applicable
3. Verify randomized stress still compares full ASM vs CPP outputs/state, but
   do not use extra randomness as a substitute for deterministic edge cases.
4. Document ASM-invalid domains explicitly rather than leaving them implicit.
5. If new coverage exposes a mismatch, keep the failing test and fix the CPP
   implementation portably and idiomatically.

## Sweep Order

1. 3D
2. SOURCES
3. 3DEXT
4. ANIM
5. SVGA
6. SYSTEM
7. MENU
8. pol_work
9. OBJECT

## Progress

### 3D

- Completed: `LightList`
  - Added fixed matrix diversity beyond identity.
  - Added directional and backlit cases that force clamp-to-zero behavior.
  - Added wider `FactorLight` coverage and larger fixed/random batches.
- Completed: `RotTransListF`
  - Added identity, translation-only, asymmetric-scale, and near-singular
    matrix cases.
  - Added deterministic 16-point batches.
  - Kept new fixed asymmetric-scale coverage inside the valid 16-bit output
    domain to avoid overflow-only storage differences.
- Completed: `ProjectList3DF`
  - Added exact near-clip boundary coverage.
  - Added all-clipped batches.
  - Added mixed visible, clipped, and screen-overflow sentinel coverage.
- Completed: `GetAngleVector2D`
  - Added exact and near-45-degree crossover cases with small signed pairs.
  - Added extra quadrant and swap-boundary coverage beyond the large fixed inputs.
- Completed: `GetAngleVector3D`
  - Added vertical and near-vertical cases where XZ length collapses to zero or one.
  - Added wrap-sensitive XZ combinations that stress the `QSqr` + 2D-angle path.
- Completed: `LongProjectPoint3D`
  - Added exact clip-plane boundary and just-inside/just-outside cases.
  - Added camera-centred visible and clipped cases.
- Completed: `LongProjectPointIso`
  - Added arithmetic right-shift boundary cases around small negative and positive values.
  - Added camera-cancellation cases to confirm centre-relative behavior.
- Completed: `ProjectListIso`
  - Added explicit sort-key overwrite cases with non-zero `Grp` values.
  - Added deterministic mixed-overflow sentinel coverage.
- Completed: `RegleTrois` / `BoundRegleTrois`
  - Added exact clamp and extrapolation boundary cases around `step=-1/0/N-1/N/N+1`.
  - Added explicit negative-`NbStep` boundary cases.

### Next 3D Candidates

- `LightList` / `RotTransListF`
  - Revisit only if higher-level suites expose precision or accumulation gaps.
- `MulMatrixF` / `RotateMatrixU`
  - Review matrix-composition coverage for identity, zeroed translation, and source-preservation edge cases.
- `RotTransListF` / `LightList`
  - Already strengthened; revisit only if higher-level suites expose new gaps.

### SOURCES

- Completed: `SearchBoundColRGB`
  - Added deterministic full 256-entry palette stress.
  - Added wider `coulmin`/`coulmax` spans to exercise deeper search ranges.
- Completed: `AddString` / `DeleteString`
  - Added interleaved mixed-operation stress instead of testing the two operations only in isolation.
  - Compared full tree, window, and global state after every mixed step.

### Next SOURCES Candidates

- `Do_Fire`
  - Re-check whether additional deterministic edge patterns uncover propagation differences beyond the existing zero, checkerboard, and sequential cases.
- `BoxFlow` / `ShadeBoxBlk` / `CopyBlockShade`
  - Review clipped-edge combinations and repeated-application stability.

### 3DEXT

- Reviewed: `ZBufBoxOverWrite2`
  - Existing visible, hidden, mixed, single-pixel, negative-depth, and random coverage already looks strong.
- Reviewed: `LineRain`
  - Existing horizontal, diagonal fog, vertical occluded, single-pixel, fully clipped, and random coverage already looks strong.
  - A dedicated depth-intersection return-flag case could still be added later, but this is lower priority than the remaining modules.

### ANIM

- Completed: `ObjectSetFrame`
  - Added explicit reset-state assertions for interpolation, step accumulators, timers, and status.
  - Added full-struct ASM-vs-CPP comparison for a non-zero preseeded object state.
- Completed: `ObjectSetInterDep`
  - Added explicit no-rotation branch checks to prove angle state is preserved when `Master & 1 == 0`.
  - Added ASM-vs-CPP comparison for the same no-rotation midpoint state.

### SVGA

- Completed: `AffGraph`
  - Added strict ASM-vs-CPP framebuffer and `ScreenX/Y` bounds coverage.
  - Added unclipped solid/pattern draws, signed-hotspot interior placement,
    clip-window and screen-edge clipping, off-screen no-op cases, and 50
    deterministic random banks.
- Completed: `GetBoxGraph`
  - Fixed signed hotspot handling in the CPP implementation.
  - Added explicit negative-hotspot tests and ASM-vs-CPP equivalence coverage for that path.
- Completed: `AffMask`
  - Added high-bit hotspot-byte interior, clip-window, and off-screen cases.
  - Verified that the CPP path keeps the ASM's raw-byte hotspot semantics for this function.
- Completed: `ScaleSprite`
  - Replaced the placeholder and min/max-only checks with strict 1:1
    framebuffer and `ScreenX/Y` bounds equivalence for minimal, hotspot,
    clipped-edge, fully clipped, and deterministic random cases.
  - Fixed the CPP path so `ScreenXMax`/`ScreenYMax` match the ASM's exclusive
    upper-edge bounds semantics.

### Next SVGA Candidates

- `ScaleBox` / `ScaleSprite`
  - `ScaleSprite` is now tightened for 1:1 equivalence; follow up only if the
    scaled-factor path is ported.
  - Add deterministic clipped-edge and degenerate-dimension cases for
    `ScaleBox`, whose current coverage is still mostly size-shape focused.

### SYSTEM

- Completed: `CPU` globals
  - Replaced the placeholder existence test with byte-for-byte comparison of all exported globals.
  - Fixed the CPP static initializers to match the ASM data segment, including `ProcessorSignature = 0x400`.
- Completed: `Mouse data`
  - Replaced placeholder link/nonzero assertions with fixed-byte sanity checks.
  - Kept the strict 541-byte ASM-vs-CPP comparison for `BinGphMouse`.

### pol_work

- Completed: `Fill_Sphere`
  - Replaced the remaining placeholder no-crash checks with strict
    framebuffer equivalence for zero-radius, transparent, fog, and 300 random
    mixed-type rounds.
  - Reused the existing Watcom ABI wrapper instead of introducing any new
    inline-ASM behavior in library code.
- Completed: `Fill_Poly` solid path in `test_polyflat.cpp`
  - Replaced the remaining degenerate/random placeholder checks with
    deterministic framebuffer assertions.
  - Collinear triangles now assert an unchanged framebuffer, and 30 random
    solid triangles now assert repeatable full-framebuffer output plus
    color/bounds invariants.
- Completed: `Fill_Poly` Gouraud/Dither path in `test_polygour.cpp`
  - Replaced the clipped/random placeholder checks with deterministic
    framebuffer assertions.
  - The clipped case now asserts repeatable output plus clipped bounding-box
    behavior, and 30 random Gouraud/Dither rounds now assert repeatable
    full-framebuffer output plus bounding-box invariants.

### Next pol_work Candidates

- `Filler_TextureGouraud`
  - Tighten the clipped and random `Fill_Poly`-level tests that still only
    assert “no crash” in `test_polygtex.cpp`.
