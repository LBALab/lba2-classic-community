# Bit-exactness: what it means and when it's required

"Bit exact" and "byte identical" appear across ~20 docs in this repo, doing three
different jobs, with no shared definition. That is how the phrase starts driving
decisions by assumption: someone invokes "it must stay bit exact" to justify or
block a change without checking which kind of bit-exactness they mean, or whether
it was ever the point here.

This doc names the three kinds and gives one rule. The short version:
bit-exactness is a means, not an end, everywhere except where an external consumer
reads the bytes.

Truth hierarchy: code > this document > external sources.

## Terminology

- **The original** = Adeline's original 1996-1997 source, publicly released as
  "Code Reborn" (https://github.com/2point21/lba2-classic), and the DOS/Watcom
  binary it built. This community fork descends from that release.
- **Retail** = the shipped game and the assets/saves it produced.
- **Oracle** = a reference we test against: the ASM routine, the retail binary, or
  a captured baseline.

## The three kinds

### 1. Format contract - bit-exactness is the goal

When bytes cross a boundary to another implementation (the retail binary, LBALab
tools, a save that must round-trip, an on-disk asset we parse), byte-identity
*is* the correctness property, not a stand-in for it. It is not negotiable and it
does not expire.

- Examples: the save wire format, on-disk asset formats (HQR, body, anim, sprite,
  samples), disc-image reads, the 32-bit data ABI.
- Docs: [SAVE_WIRE_PLAN.md](SAVE_WIRE_PLAN.md), [SAVEGAME.md](SAVEGAME.md),
  [ENGINE_FILE_FORMATS.md](ENGINE_FILE_FORMATS.md), [GAME_DATA.md](GAME_DATA.md),
  [DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md), [ABI.md](ABI.md).
- Test: read retail, write ours, the diff must be empty; or round-trip a file
  through the retail binary.

### 2. ASM-parity oracle - bit-exactness tests a faithful port

When porting ASM to C++, the ASM is ground truth, so bit-exact output against the
ASM oracle is the strongest cheap test that the port is faithful. Required up to a
documented platform-math tolerance. Two caveats live here:

- **Tolerance is already real.** x87 80-bit long double vs 64-bit, and x87
  rounding, mean we already accept characterized sub-pixel drift as
  not-a-regression. So even where we care most, "bit exact" means "bit exact
  within a stated tolerance," not absolute. See [PLATFORM.md](PLATFORM.md) and
  [COMPILER_NOTES.md](COMPILER_NOTES.md).
- **Faithful is not the same as correct.** A faithful port can reproduce an
  original defect: the 1997 `ManageTime` bug ([TIMING.md](TIMING.md)) is one the
  SDL port resurrected by porting the original shape too literally. When the
  original is wrong, decide whether you want parity or the fix, and say which in
  the change.
- Examples: fillers, `ScaleSprite`, the interior `Sphere` radius.
- Docs: [ASM_TO_CPP_REFERENCE.md](ASM_TO_CPP_REFERENCE.md),
  [ASM_VALIDATION_PROGRESS.md](ASM_VALIDATION_PROGRESS.md),
  [ASM_TEST_COVERAGE_AUDIT.md](ASM_TEST_COVERAGE_AUDIT.md).
- Test: the Docker 32-bit ASM-equivalence suite and host oracle tests
  ([TESTING.md](TESTING.md)).

### 3. Regression tripwire - bit-exactness is a proxy for "behaviour unchanged"

Hash an output before and after a change we believe is a no-op (a refactor, a
centralization, a rename) and flag any diff. Cheap and strict, and this is how we
use the term most of the time day to day. But it is a proxy for "behaviour
unchanged," and it has two failure modes:

- **Necessary, not sufficient.** A green hash proves one artifact did not move; it
  does not prove the system is correct. projrec hashes catch projection-pipeline
  drift but miss buffer-stride and fixed-offset bugs that a downstream consumer
  carries. A passing tripwire is not a proof of correctness.
- **Wrong tool when the change is meant to alter output.** HD rendering,
  iso-scale, and widescreen intentionally change pixels. Insisting on
  byte-identity there is a category error. The pattern we already use is to keep
  bit-exactness as a guard on the *unchanged* path ("bit-exact at 1.0") while
  allowing divergence in the new regime, and to verify the new regime
  behaviourally.
- Docs: [POLYREC.md](POLYREC.md), [CONTROL.md](CONTROL.md) (harness determinism),
  [WIDESCREEN.md](WIDESCREEN.md), [TIMING.md](TIMING.md).

## The rule

**Default: keep the tripwire.** For a change that claims to be a no-op, a
bit-exact before/after check is the cheapest strong evidence you have, so keep it
unless you have something better.

**Exit: you may accept a byte diff (or drop the byte check) only when both hold:**

1. The property that actually matters here is faithful behaviour or no regression,
   not the bytes themselves. That is: this is kind 2 or kind 3, never kind 1.
2. You have replaced the byte check with a test that demonstrates that property
   directly: a behavioural test, a characterization capture, or an oracle
   comparison within a stated tolerance.

Dropping byte-identity is not free. The price is a test that proves the real
property. Do not drop the tripwire and prove nothing. And do not invoke
bit-exactness to block a kind-2 or kind-3 change when the property that matters is
already demonstrated another way.

## Before you say "it must be bit exact"

Name the kind first:

- Does another implementation read these bytes? Kind 1: byte-identity is the goal,
  stop here.
- Am I porting ASM and comparing to the ASM output? Kind 2: byte-identity within
  platform-math tolerance, and also ask "was the original correct?"
- Am I checking that a no-op change did not drift? Kind 3: byte-identity is a cheap
  proxy. A diff is a signal to investigate, not automatically a failure, and a
  match is necessary but not sufficient.

If you cannot name the kind, you are assuming the term rather than using it.

## At a glance

| Kind | Bytes read by | Byte-identity is | Exit allowed? | Exemplar docs |
|------|---------------|------------------|---------------|---------------|
| 1. Format contract | Another implementation | The goal | No | SAVE_WIRE_PLAN, ENGINE_FILE_FORMATS, GAME_DATA, ABI |
| 2. ASM-parity oracle | The test, vs ASM | A faithful-port test, within tolerance | Only within characterized tolerance | ASM_TO_CPP_REFERENCE, ASM_VALIDATION_PROGRESS, PLATFORM |
| 3. Regression tripwire | The test, vs baseline | A proxy for "unchanged" | Yes, if replaced by a behavioural proof | POLYREC, CONTROL, WIDESCREEN |
