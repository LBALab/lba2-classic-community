---
type: Decision
title: The RNG reproduces glibc, in tree
description: Rnd draws from an in-tree reimplementation of glibc's TYPE_3 generator, so one recording replays on every platform and every Linux baseline stays valid, and the single stream is not split for the recorder's sake.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T09:00:00Z }
constrains:
  - /subsystems/recording.md
relates_to:
  - /quirks/changecube-seeds-from-the-boot-clock.md
sources:
  - id: random-h
    resource: ../../../LIB386/H/SYSTEM/RANDOM.H
    title: RANDOM.H, the generator's own account of itself
  - id: recording-doc
    resource: ../../RECORDING.md
    title: Session recording (docs/RECORDING.md), "Replaying on another platform"
  - id: review
    resource: ../../plan/RECORDER_OBSERVER_REVIEW.md
    title: The recorder as an observer, "What not to do"
---

# Context

libc `rand()` is not one function. glibc and UCRT differ in sequence and in `RAND_MAX` (2147483647 against 32767), so `rand() % n` is differently distributed as well as differently ordered. One shared stream feeds rain, ambient sound, animated textures, particle scatter, track AI and the `LF_RND` script opcode, seeded once per cube change, so drawing from libc parts two platforms within a few ticks of any scene with something moving in it.[^random-h]

# Decision

The generator is `Rnd_Next` in [RANDOM.CPP](../../../LIB386/SYSTEM/RANDOM.CPP), glibc's TYPE_3 additive-feedback generator reimplemented, and the `Rnd` macro draws from it instead of `rand()`. Reproducing glibc specifically is the whole trick: every committed baseline, corpus save and reference recording was made on Linux, so the change costs Linux nothing and puts every other platform on the sequence those artifacts already describe.[^random-h]

A recording declares what a replay has to agree with as traits, not platforms: `numeric.rng` names the generator and `numeric.long_double_bits` carries `LDBL_MANT_DIG`. `build.platform` is recorded and deliberately not compared.[^recording-doc]

The proof needs its negative control. A Linux recording replaying 301 ticks on Windows proves nothing alone, since a quiet scene passes either way; with the generator patched back to `rand()` on the Windows side the same recording fails at tick 2. `tests/random` pins both halves: a committed vector of draws runs on every platform, and a live comparison against the system `rand()` builds only on glibc and is compiled out elsewhere rather than skipped, because a skipped check exits 0 and reads like a pass.[^recording-doc]

# Non-goals

- **A better generator.** The same amount of code, and it invalidates every baseline, corpus digest and recording on one commit.
- **Doom's `P_Random` and `M_Random` split.** The shape is right, since 17 of 54 draw sites are non-simulation, and it would end the `DetailLevel` and ambience divergences at the source. It was measured not to be what unblocks the loose-clock step, and it invalidates the same artifacts. It is a bit-exactness decision to take when one is being taken anyway, not a recorder work item.[^review]
- **The retail sequence.** That was Watcom's, and no port of this engine reproduces it.
- **ARM.** `long double` is 53-bit there, so LIB386/3D's projection, rotation and distance round differently. The header names the disagreement up front; taking those paths off the host's extended precision is a separate and larger job.[^recording-doc]

[^random-h]: LIB386/H/SYSTEM/RANDOM.H, the header comment.
[^recording-doc]: docs/RECORDING.md, "Replaying on another platform" and "Checking it".
[^review]: docs/plan/RECORDER_OBSERVER_REVIEW.md, "What not to do".
