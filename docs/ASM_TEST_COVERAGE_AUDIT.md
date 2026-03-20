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
