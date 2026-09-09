---
type: Quirk
title: MulMatrixF zeroes the destination translation
description: The 3x3 float matrix multiply writes zero into the destination's TX, TY and TZ, which a port written from the arithmetic alone leaves untouched.
status: draft
scope: "unconditional; every call, every configuration"
equivalence: tested
asm_origin: "LIB386/3D/MULMATF.ASM:MulMatrixF"
ports: /porting/3d.md#mulmatrixf
verified_against:
  - /references/asm-equivalence-suite.md
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T09:00:00Z }
sources:
  - id: asm
    resource: ../../../LIB386/3D/MULMATF.ASM
    title: MULMATF.ASM, the routine
  - id: cpp
    resource: ../../../LIB386/3D/MULMATF.CPP
    title: MULMATF.CPP, the port
  - id: test
    resource: ../../../tests/3D/test_mulmatf.cpp
    title: The equivalence test
  - id: progress
    resource: ../../ASM_VALIDATION_PROGRESS.md
    title: ASM to C validation progress, the MULMATF row
---

# Scope

Unconditional. The stores run on every call, whatever the sources hold and whatever the build or the clock is doing.

# Evidence

`tested`: [test_mulmatf.cpp](../../../tests/3D/test_mulmatf.cpp) calls `MulMatrixF` in both forms on the same inputs and compares the whole `TYPE_MAT` byte for byte, with source matrices that carry non-zero translations and a destination pre-filled with `0xCC` so an unwritten field shows.[^test] The port shipped without the three stores; the test found it, and the fix is commit b8b51e27, "fix: validate mulmatf parity".[^progress]

# Behaviour

The routine clears `eax` on entry, computes the nine products on the x87 stack and stores them, then stores `eax` into the destination's `MAT_MTX`, `MAT_MTY` and `MAT_MTZ`. The destination's translation is therefore always zero, whatever either source holds, and neither source is written.[^asm] The port ends with the same three stores as `0.0f`.[^cpp]

# Why it is load bearing

- A 3x3 product looks as if it has nothing to say about translation, so a reader tidying [MULMATF.CPP](../../../LIB386/3D/MULMATF.CPP) would be removing behaviour, not noise.
- `RotateMatrixU` is the only caller. It builds a rotation in `MatrixLib2` and multiplies it into the destination, so a rotated matrix carries no translation of its own, whatever the destination held before.
- The test's destination is pre-filled, so the three stores are 12 bytes of the comparison; without them the fixed cases and the random rounds both fail.

[^asm]: LIB386/3D/MULMATF.ASM, the stores after the last `fstp`.
[^cpp]: LIB386/3D/MULMATF.CPP.
[^test]: tests/3D/test_mulmatf.cpp, `assert_mul_case`.
[^progress]: docs/ASM_VALIDATION_PROGRESS.md, the `3D/MULMATF.ASM` row.
