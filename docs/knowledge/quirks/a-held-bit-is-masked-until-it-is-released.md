---
type: Quirk
title: A held bit is masked until it is released
description: The funnel's one static, NoRepeatInput, masks an action bit a caller asked not to repeat for as long as the key stays down and drops the mask the poll the key comes up; the 1997 menu and dialogue layer is built on that latch at a hundred sites, and the pad honours it only because its bindings were folded into the same table.
status: draft
scope: "unconditional; every device that reaches the combined table"
equivalence: partial
asm_origin: "LIB386/SYSTEM/INPUT.CPP:GetInput"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T19:00:00Z }
owner: /subsystems/input.md
verified_against:
  - /references/input-tests.md
relates_to:
  - /decisions/the-pad-is-a-first-class-binding.md
sources:
  - id: funnel
    resource: ../../../LIB386/SYSTEM/INPUT.CPP
    title: INPUT.CPP, GetInput and ClearNoRepeatInput
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, LIB386/SYSTEM/INPUT.CPP, the same latch with its 1997 comment
  - id: research
    resource: ../../plan/INPUT_RESEARCH.md
    title: Input system research, "What retail parity means here" and gap G7
  - id: funnel-test
    resource: ../../../tests/input_funnel/test_input_funnel.cpp
    title: The funnel test, on a synthetic table
---

# Scope

Every poll, every device. The latch is applied after the combined table is walked, so a pad button, a touch button and a replayed key are all subject to it; the pad was not until its bindings moved into the table.

# Evidence

`partial`, and the partial is specific. `tests/input_funnel` pins the latch's rules on a synthetic table it builds for itself, including the regression where a mid-frame rebuild dropped held movement, and pins that a pad button is masked like a key. It pins nothing about the retail layout, so the funnel's semantics are under test and the table it is meant to be walked with is not. The latch is in the initial import with its comment dated 6 June 1997.[^funnel-test][^original]

# Behaviour

`GetInput(norepeat)` does five things in order: ORs the argument into `NoRepeatInput`; rebuilds `Input` from the table; computes the bits that are both masked and still held; masks `Input` by `NoRepeatInput`; and keeps only the still-held bits as the next mask. So a bit a caller asked to suppress stays suppressed while its key is down and is released on the first poll the key is up, without the caller doing anything. `ClearNoRepeatInput` zeroes the mask outright. The wrappers, `InitWaitNoInput`, `InitWaitNoKey`, `ClearWaitNoInput` and the rest, are macros over those two calls; 109 sites use them at `as_of`.[^funnel]

# Why it is load bearing

- The whole 1997 menu and dialogue layer rests on it. A dialogue opens on a press and must not close on the same press; a modal that consumed a key must not hand it to the screen behind it. The latch is what makes "the key that opened this is not the key that closes it" true, and a rewrite of the funnel that kept the table and lost the latch would pass every screenshot and fail every menu.[^research]
- It is why the pad had to be folded into the table rather than ORed in afterwards. The earlier bolt-on added the pad's bits after `GetInput` had masked `Input`, so a pad button bypassed the latch and repeated; [the decision](/decisions/the-pad-is-a-first-class-binding.md) moved it inside.[^funnel-test]
- It has one owner and a hundred writers. The mask is one static and any of the 109 sites can clear a suppression another one set, with nothing recording who did. A screen that works alone and misbehaves after another screen is the shape that produces.[^research]

# What it does not tell you

- **Who set the mask.** A masked bit and an absent bit both read as zero in `Input`; the latch leaves no trace of which.
- **Whether a release happened between polls.** A press and release inside one poll interval never existed to the sampler, so the latch never sees the release and the bit stays masked until the next press is sampled down.

[^funnel]: [LIB386/SYSTEM/INPUT.CPP](../../../LIB386/SYSTEM/INPUT.CPP), `GetInput` and `ClearNoRepeatInput`.
[^original]: Commit 333929ab, the initial import, LIB386/SYSTEM/INPUT.CPP, the same five steps and the comment "Added le 06/06/97".
[^research]: [docs/plan/INPUT_RESEARCH.md](../../plan/INPUT_RESEARCH.md), "What retail parity means here, concretely" and gap G7.
[^funnel-test]: [tests/input_funnel/test_input_funnel.cpp](../../../tests/input_funnel/test_input_funnel.cpp), the file comment.
