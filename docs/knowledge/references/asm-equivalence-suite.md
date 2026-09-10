---
type: Reference
title: ASM equivalence suite
description: The tests that run each ported routine in its ASM and C++ forms on the same inputs and compare results and side effects byte for byte; the attester behind every equivalence of tested.
status: draft
resource: ../../TESTING.md
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T09:00:00Z }
sources:
  - id: testing
    resource: ../../TESTING.md
    title: Testing (docs/TESTING.md)
  - id: progress
    resource: ../../ASM_VALIDATION_PROGRESS.md
    title: ASM to C validation progress
  - id: audit
    resource: ../../ASM_TEST_COVERAGE_AUDIT.md
    title: ASM test coverage audit
---

# What it attests

A routine listed `[x]` in [ASM_VALIDATION_PROGRESS.md](../../ASM_VALIDATION_PROGRESS.md) has a test that links the original `.ASM` and the `.CPP` port into one binary, calls both with the same inputs, and asserts equality with `ASSERT_ASM_CPP_EQ_INT` for scalars and `ASSERT_ASM_CPP_MEM_EQ` for buffers. The legend is `[ ]` untested, `[x]` tested, `[~]` partial, and it is the vocabulary the bundle's `equivalence` field uses.[^progress]

# What covered means

[ASM_TEST_COVERAGE_AUDIT.md](../../ASM_TEST_COVERAGE_AUDIT.md) is the rubric: branches, dispatch paths, globals written, mutated buffers, source preservation, and ASM-invalid domains named rather than left implicit. A `[x]` says the pair is under test, not that its domain is explored; the audit's progress section says which pairs have been strengthened.[^audit]

# How to run

[TESTING.md](../../TESTING.md) describes the lane. The tests need the 32-bit ASM built alongside the port, which the host `ctest` run does not do.[^testing]

[^progress]: docs/ASM_VALIDATION_PROGRESS.md, the legend.
[^audit]: docs/ASM_TEST_COVERAGE_AUDIT.md, "Audit rubric".
[^testing]: docs/TESTING.md.
