# ASM Test Coverage Audit

This document tracks the breadth-first audit of the ASM/CPP pairs listed in
[ASM_VALIDATION_PROGRESS.md](ASM_VALIDATION_PROGRESS.md). The goal is to treat existing `[x]` entries as
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
  - Added fixed non-zero input `TX/TY/TZ` cases so matrix-composition coverage now exercises the ASM's zeroed-translation/output behavior and source-preservation semantics directly.
- `RotTransListF` / `LightList`
  - Already strengthened; revisit only if higher-level suites expose new gaps.

### SOURCES

- Completed: `SearchBoundColRGB`
  - Added deterministic full 256-entry palette stress.
  - Deepened the 256-entry palette stress to 100 deterministic rounds.
  - Added wider `coulmin`/`coulmax` spans to exercise deeper search ranges.
- Completed: `AddString` / `DeleteString`
  - Added interleaved mixed-operation stress instead of testing the two operations only in isolation.
  - Compared full tree, window, and global state after every mixed step.
  - Deepened the delete-only stress to 150 deterministic rounds and the mixed
    AddString/DeleteString pipeline to 80 deterministic sequences.

### Next SOURCES Candidates

- Completed: `Do_Fire`
  - Added deterministic vertical-stripe inputs to exercise the horizontal
    wraparound averaging path explicitly.
  - Added isolated edge-impulse inputs to exercise boundary-column neighbor
    sampling and confirm byte-for-byte parity through two consecutive fire
    updates.
- `BoxFlow` / `ShadeBoxBlk` / `CopyBlockShade`
  - Added repeated-application stability sequences for all three helpers.
  - Added custom clip-window edge cases for `BoxFlow` and `ShadeBoxBlk`.

### 3DEXT

- Reviewed: `ZBufBoxOverWrite2`
  - Existing visible, hidden, mixed, single-pixel, negative-depth, and random coverage already looks strong.
- Reviewed: `LineRain`
  - Existing horizontal, diagonal fog, vertical occluded, single-pixel, fully clipped, and random coverage already looks strong.
  - Added a dedicated single-pixel depth-intersection return-flag case to isolate the `ret == 3` path.

### ANIM

- Completed: `ObjectInitAnim`
  - Added strict full-`T_OBJ_3D` ASM-vs-CPP comparison for the same-anim
    early-return path after pending visual promotion.
  - Deepened the deterministic full-state random sweep to 100 rounds.
- Completed: `ObjectSetFrame`
  - Added explicit reset-state assertions for interpolation, step accumulators, timers, and status.
  - Widened the full-struct ASM-vs-CPP comparison from the frame-1 path to the
    frame-0 and frame-2 branches as well, using non-zero preseeded object state.
  - Fixed `LIB386/ANIM/FRAME.CPP` so the last-frame loop-back branch reads the
    next timer and master state from frame 0 like the ASM, instead of from the
    anim header/end pointer.
- Completed: `ObjectSetInterDep`
  - Added explicit no-rotation branch checks to prove angle state is preserved when `Master & 1 == 0`.
  - Added ASM-vs-CPP comparison for the same no-rotation midpoint state.

### SVGA

- Completed: `BlitBoxF`
  - Raised the deterministic random full-buffer ASM-vs-CPP stress from 20 to
    100 source/destination pairs.
  - Kept the existing strict destination and source-preservation byte-for-byte
    assertions intact; this slice only deepened coverage.
- Completed: `AffGraph`
  - Added strict ASM-vs-CPP framebuffer and `ScreenX/Y` bounds coverage.
  - Added unclipped solid/pattern draws, signed-hotspot interior placement,
    clip-window and screen-edge clipping, off-screen no-op cases, and 100
    deterministic random banks.
- Completed: `GetBoxGraph`
  - Fixed signed hotspot handling in the CPP implementation.
  - Added explicit negative-hotspot tests and ASM-vs-CPP equivalence coverage for that path.
- Completed: `AffMask`
  - Added high-bit hotspot-byte interior, clip-window, and off-screen cases.
  - Verified that the CPP path keeps the ASM's raw-byte hotspot semantics for this function.
- Completed: `CalcGraphMsk`
  - Replaced the remaining synthetic-brick `size > 0` sanity check with exact
    expected mask bytes for the fixed fixture, while keeping the strict ASM-vs-CPP
    synthetic and random coverage.
- Completed: `SizeFont`
  - Deepened the deterministic synthetic-bank ASM-vs-CPP string sweep to 100
    cases while keeping the fixed width/space cases intact.
- Completed: `AffString`
  - Replaced the remaining weak single-glyph sanity check with strict ASM-vs-CPP
    framebuffer equivalence for a single `A`, plus the exact 28-pixel glyph
    footprint from the embedded `Font8x8` data.
- Completed: `ScaleSprite`
  - Replaced the placeholder and min/max-only checks with strict 1:1
    framebuffer and `ScreenX/Y` bounds equivalence for minimal, hotspot,
    clipped-edge, fully clipped, and deterministic random cases.
  - Deepened the broad 1:1 random equivalence sweep to 100 deterministic
    cases covering a wider visible/off-screen placement domain.
  - Fixed the CPP path so `ScreenXMax`/`ScreenYMax` match the ASM's exclusive
    upper-edge bounds semantics.

### Next SVGA Candidates

- `ScaleBox` / `ScaleSprite`
  - `ScaleSprite` is now tightened for 1:1 equivalence; follow up only if the
    scaled-factor path is ported.
  - `ScaleBox` now has a deeper 100-case deterministic random sweep plus fixed
    same-size exact-copy, single-pixel, 1-pixel strip, and bottom-right edge-
    placement equivalence cases.

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
    framebuffer equivalence for zero-radius, transparent, fog, and a 100-case
    deterministic mixed-type sweep, while keeping the separate 300-round
    ASM-vs-CPP random loop in place.
  - Reused the existing Watcom ABI wrapper instead of introducing any new
    inline-ASM behavior in library code.
- Completed: `Fill_Poly` solid path in `test_polyflat.cpp`
  - Replaced the remaining degenerate/random placeholder checks with
    deterministic framebuffer assertions.
  - Collinear triangles now assert an unchanged framebuffer, and 100 random
    solid triangles now assert repeatable full-framebuffer output plus
    color/bounds invariants.
- Completed: exact `Fill_Poly(POLY_SOLID/POLY_TRAME)` basic area counts in
  `test_polyflat.cpp`
  - Tightened the basic solid and trame front-end area checks so they now
    assert exact deterministic `820`-pixel and `745`-pixel footprints instead
    of only checking that a large enough area was filled.
- Completed: `Fill_Poly` Gouraud/Dither path in `test_polygour.cpp`
  - Replaced the clipped/random placeholder checks with deterministic
    framebuffer assertions.
  - The clipped case now asserts repeatable output plus clipped bounding-box
    behavior, and 100 random Gouraud/Dither rounds now assert repeatable
    full-framebuffer output plus bounding-box invariants.
- Completed: exact `Fill_Poly(POLY_GOURAUD/POLY_DITHER)` basic triangle counts
  in `test_polygour.cpp`
  - Tightened the basic Gouraud and Dither triangle smoke cases so they now
    assert the exact deterministic `3640`-pixel footprint instead of only
    checking that a large enough area was filled.
- Completed: exact uniform-light Gouraud center sample in `test_polygour.cpp`
  - Tightened the uniform-light Gouraud smoke check so it now asserts the
    exact deterministic center pixel value `11` instead of only checking that
    the pixel was non-zero.
- Completed: `Fill_Poly` textured Gouraud/Dither path in `test_polygtex.cpp`
  - Replaced the clipped/random placeholder checks with deterministic
    framebuffer assertions.
  - The clipped case now asserts repeatable output plus clipped bounding-box
    behavior, and the random textured rounds now assert repeatable
    full-framebuffer output plus bounding-box invariants.
- Completed: exact `Fill_Poly(POLY_TEXTURE_GOURAUD/POLY_TEXTURE_DITHER)` basic
  triangle counts in `test_polygtex.cpp`
  - Tightened the basic textured Gouraud and textured Dither triangle smoke
    cases so they now assert exact deterministic `3640`-pixel and
    `2840`-pixel footprints instead of only checking that a large enough area
    was filled.
- Completed: `Fill_Poly` TextureZ fog path in `test_polytzf.cpp`
  - Replaced the basic/random placeholder checks with deterministic
    framebuffer assertions.
  - The basic case now asserts repeatable output plus bounding-box behavior,
    and the random fog rounds now assert repeatable full-framebuffer output
    plus bounding-box invariants across a 100-case deterministic sweep.
- Completed: `Filler_TextureZ` fixed strip cases in `test_polytexz.cpp`
  - Replaced the remaining narrow/wide placeholder smoke checks with strict
    fixed-case ASM-vs-CPP framebuffer comparisons.
  - The new cases cover both a narrow strip and a wider strip with explicit
    non-default perspective parameters.
- Completed: remaining `Fill_Sphere` / `Fill_Poly` placeholder cleanup in
  `test_polydisc.cpp` and `test_poly.cpp`
  - Replaced the last `Fill_Sphere` zero-radius and randomized no-crash checks
    with deterministic framebuffer equivalence using the existing ASM wrapper,
    now exercised across a 100-case front-end random sweep.
  - Replaced the `Fill_Poly` random type smoke loop with repeatable full-
    framebuffer assertions plus bounding-box invariants.
  - Tightened the invalid-domain helpers so `INV64(1)` and `SetFog(0,0)` are
    explicit CPP-only checks; `SetFog(0,0)` cannot be sent through the ASM path
    because the original routine traps with a numeric exception.
- Completed: exact `Fill_Sphere` basic/front-edge counts in `test_polydisc.cpp`
  - Tightened the remaining radius-coverage, small-radius, and partially
    clipped front-end smoke cases so they now assert exact deterministic
    `770`-pixel, `6`-pixel, and `846`-pixel footprints respectively.
- Completed: exact `ClipperZ` all-behind return in `test_polyclip.cpp`
  - Tightened the CPP-only all-behind sanity case so it now asserts the exact
    deterministic return value `0` instead of accepting either `0` or
    `(U32)-1`.
- Completed: exact `SetFog` fixture outputs in `test_poly.cpp`
  - Tightened the representative `SetFog(100,1000)` and `SetFog(0,65535)`
    checks so they now assert exact deterministic
    `Fill_ZBuffer_Factor`/`Fill_ScaledFogNear`/`Fill_Fog_Factor` values
    instead of only checking that the Z-buffer factor was positive.
- Completed: exact `Fill_Poly(POLY_TEXTURE)` basic triangle count in
  `test_polytext.cpp`
  - Tightened the basic textured triangle smoke case so it now asserts the
    exact deterministic `3640`-pixel footprint for both the solid reference
    fill and the textured render, instead of only checking that some pixels
    were drawn.
- Completed: `ObjectSetInterFrame` exact-factor coverage in
  `tests/ANIM/test_intframe.cpp`
  - Expanded the fixture to cover both rotate and translate group
    interpolation.
  - Added exact 0%, 25%, 50%, 75%, and `0xFFFF` endpoint expectations.
  - Widened the deterministic ASM-vs-CPP stress coverage to compare the full
    `T_OBJ_3D` state instead of only the interpolated frame bytes.
  - Deepened the deterministic interpolator sweep to 100 rounds so this path
    stays aligned with the stronger recent strict stress coverage.
- Completed: `ObjectSetInterAnim` exact timer/interpolation coverage in
  `tests/ANIM/test_intanim.cpp`
  - Replaced the midpoint range check with exact state assertions.
  - Added deterministic timer-sweep checks for the CPP path, covering the
    no-op entry case, midpoint interpolation, near-endpoint rounding, and
    the exact frame-transition state.
  - Added full `T_OBJ_3D` ASM-vs-CPP parity for the no-change early-return
    path.
  - Kept midpoint and frame-transition ASM parity scoped to the documented
    safe domain, since broader full-state divergence is inherited from
    `INTERDEP`.
- Completed: clipped `Line` sanity tightening in `tests/pol_work/test_polyline.cpp`
  - Replaced the remaining off-screen-left no-crash comment with exact
    visible-span assertions for the clipped horizontal line case.
- Completed: `ObjectSetInterDep` exact-state CPP checks in
  `tests/ANIM/test_interdep.cpp`
  - Replaced the midpoint range check with the exact `0x8000`
    interpolator expectation.
  - Tightened frame-advance and loop coverage to assert the exact CPP
    post-call state rather than only checking for a frame bit or a changed
    next-frame index.
- Completed: `AffMask` high-bit hotspot coverage in
  `tests/SVGA/test_mask.cpp`
  - Added explicit hotspot-byte cases in the middle and upper parts of the
    high-bit range (`0x80`, `0x81`, `0x90`, `0xC0`, `0xC8`) in addition to the
    existing `0xFB..0xFF` cases.
  - Verified both full framebuffer output and `ScreenX/Y` bounds against ASM,
    plus a direct CPP sanity check that the hotspot bytes are treated as raw
    unsigned offsets rather than sign-extended values.
- Completed: `GetBoxGraph` / `AffGraph` high-bit hotspot boundary coverage in
  `tests/SVGA/test_graph.cpp`
  - Added explicit `0x80`-range hotspot cases to prove that `GetBoxGraph`
    sign-extends those bytes into negative bounds.
  - Added matching `AffGraph` framebuffer/bounds equivalence cases to verify
    the signed hotspot interpretation end-to-end at visible on-screen
    coordinates.
  - Deepened the deterministic random `AffGraph` sweep to 100 banks to keep
    clipping, hotspot, and framebuffer equivalence coverage at the same stress
    depth as the stronger SVGA front-end tests.
- Completed: `ObjectStoreFrame` full object-state equivalence in
  `tests/ANIM/test_stoframe.cpp`
  - Replaced the last field-only ASM checks with a full `T_OBJ_3D`
    byte-for-byte comparison after the store operation.
  - Kept the existing strict stored-buffer comparison, so the test now proves
    both the circular-buffer contents and the object-side bookkeeping state.
- Completed: `ObjectInitAnim` full-state and same-anim visual promotion in
  `tests/ANIM/test_anim.cpp`
  - Replaced the fixed first-init ASM check and the deterministic random batch
    with full `T_OBJ_3D` byte-for-byte comparisons.
  - Added explicit coverage for the same-anim early-return path, proving that
    pending `NextBody` and `NextTexture` still have to be promoted into
    `Body` and `Texture` before the function exits.
  - Deepened the deterministic full-state random sweep to 100 rounds.
  - Fixed `LIB386/ANIM/ANIM.CPP` so the CPP path matches the ASM's up-front
    body/texture promotion behavior.
- Completed: `AFF_OBJ` base visible render exactness in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the three base visible render helpers so `BodyDisplay`,
    `BodyDisplay_AlphaBeta`, and `ObjectDisplay` now assert the exact
    deterministic `NonZeroPixels == 233` count for the shared simple fixture,
    rather than only checking that some pixels were drawn.
- Completed: `AFF_OBJ` line and sphere exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay line render`, `ObjectDisplay sphere
    render`, and `ObjectDisplay sphere transparent render` cases so they now
    assert exact deterministic `NonZeroPixels` counts of `32`, `136`, and
    `136`, rather than only checking that pixels were drawn.
- Completed: `AFF_OBJ` shaded alias exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay dither render`,
    `ObjectDisplay gouraud table render`, and
    `ObjectDisplay dither table render` cases so they now assert the exact
    deterministic `NonZeroPixels == 233` count, rather than only checking
    that pixels were drawn.
- Completed: `AFF_OBJ` transparent and trame exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay transparent render` and
    `ObjectDisplay trame render` cases so they now assert exact deterministic
    `NonZeroPixels` counts of `233` and `111`, rather than only checking that
    pixels were drawn.
- Completed: `AFF_OBJ` textured triangle exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay textured flat render`,
    `ObjectDisplay textured solid render`, and
    `ObjectDisplay textured gouraud render` cases so they now assert the exact
    deterministic `NonZeroPixels == 514` count, rather than only checking that
    pixels were drawn.
- Completed: `AFF_OBJ` textured-Z triangle exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay textured Z flat render`,
    `ObjectDisplay textured Z solid render`, and
    `ObjectDisplay textured Z gouraud render` cases so they now assert the
    exact deterministic `NonZeroPixels == 514` count, rather than only
    checking that pixels were drawn.
- Completed: `AFF_OBJ` textured quad exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay textured quad flat render`,
    `ObjectDisplay textured quad solid render`, and
    `ObjectDisplay textured quad gouraud render` cases so they now assert the
    exact deterministic `NonZeroPixels == 1058` count, rather than only
    checking that pixels were drawn.
- Completed: `AFF_OBJ` textured-Z quad exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay textured quad Z flat render`,
    `ObjectDisplay textured quad Z solid render`, and
    `ObjectDisplay textured quad Z gouraud render` cases so they now assert
    the exact deterministic `NonZeroPixels == 1058` count, rather than only
    checking that pixels were drawn.
- Completed: `AFF_OBJ` env triangle exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay env flat render`,
    `ObjectDisplay env solid render`, and
    `ObjectDisplay env gouraud render` cases so they now assert the exact
    deterministic `NonZeroPixels == 514` count, rather than only checking
    that pixels were drawn.
- Completed: `AFF_OBJ` scaled env triangle exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay env flat scaled render`,
    `ObjectDisplay env solid scaled render`, and
    `ObjectDisplay env gouraud scaled render` cases so they now assert the
    exact deterministic `NonZeroPixels == 514` count, rather than only
    checking that pixels were drawn.
- Completed: `AFF_OBJ` env quad exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay env quad flat render`,
    `ObjectDisplay env quad flat scaled render`,
    `ObjectDisplay env quad solid render`,
    `ObjectDisplay env quad solid scaled render`,
    `ObjectDisplay env quad gouraud render`, and
    `ObjectDisplay env quad gouraud scaled render` cases so they now assert
    the exact deterministic `NonZeroPixels == 1058` count, rather than only
    checking that pixels were drawn.
- Completed: `AFF_OBJ` multigroup exact visible counts in
  `tests/OBJECT/test_aff_obj.cpp`
  - Tightened the dedicated `ObjectDisplay multigroup visible render` and
    `ObjectDisplay multigroup translate render` cases so they now assert the
    exact deterministic `NonZeroPixels == 1255` count, rather than only
    checking that pixels were drawn.
- Completed: fixed-table float-to-int placeholder cleanup in
  `tests/fpu_precision/test_fpu_precision.cpp`
  - Replaced the placeholder pass-through assertions in the deterministic
    round-to-nearest and truncation tables with exact full-match checks:
    `lrintf` and `lrintl(long double)` must match the ASM for all 19 fixed
    round cases, and the plain C cast must match the ASM for all 11 fixed
    truncation cases.
- Completed: exact isolated XSlope and TextureZ FPU parity checks in
  `tests/fpu_precision/test_fpu_precision.cpp`
  - Tightened the isolated `test_xslope_cross` and
    `test_texz_uslope_stacked_vs_stored` sections so their concrete cases now
    assert direct ASM-vs-CPP equality and their deterministic stress loops now
    require all 500 rounds to match exactly, rather than only printing the
    counts.
- Completed: broader full-match FPU precision placeholder cleanup in
  `tests/fpu_precision/test_fpu_precision.cpp`
  - Replaced the remaining placeholder assertions in the sections that already
    report exact deterministic parity: integer division round/trunc, slope
    round/trunc, interpolation, mul-add, mul-sub, dot3, perspective UV, and
    reciprocal-mul-256 now all require their exact `500/500` match counts.
- Completed: deterministic reciprocal-mul precision-gap assertion in
  `tests/fpu_precision/test_fpu_precision.cpp`
  - Tightened `test_reciprocal_mul` so it now asserts the exact current
    deterministic match counts: `float+lrintf` matches the ASM in `499/500`
    rounds, while the `double`, `long double`, `volatile long double`, and
    staged long-double variants all match in `500/500` rounds.

### Next Candidates

- Broader placeholder sweep outside `pol_work`
  - Re-run the repo-wide audit for remaining `ASSERT_TRUE(1)` / no-crash style
    tests in other ASM/CPP areas now that the visible `pol_work` backlog is
    cleared.
- Broader exactness sweep in already-tested modules
  - Look for CPP-side range checks, bit-only checks, or partial-struct
    comparisons that can be tightened to exact state or full-buffer
    assertions without overstepping known ASM-domain limitations.
