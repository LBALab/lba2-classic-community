---
type: Quirk
title: The spell keys bypass the action bitfield
description: Four of the thirty-six binding slots are read by testing the binding table directly and set no Input bit, because only thirty-two slots fit the bitfield and the spell bits share positions with movement; they look like ordinary bindings from the config screen and the cfg, and routing them through Input stops the spell firing.
status: draft
scope: "unconditional; slots 32 to 35, the penguin, the jetpack and the two spells"
equivalence: partial
asm_origin: "SOURCES/PERSO.CPP:MainLoop"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T19:00:00Z }
owner: /subsystems/input.md
verified_against:
  - /references/input-tests.md
sources:
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, SpellKeyDown and its four call sites
  - id: input-h
    resource: ../../../SOURCES/INPUT.H
    title: INPUT.H, the spell bits sharing positions with the movement bits
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, SOURCES/PERSO.CPP, the same direct CheckKey reads of slots 32 and 33
  - id: bypass-test
    resource: ../../../tests/automation/test_spell_keys_bypass.sh
    title: The fixture that counts one key rise against zero action rises
  - id: research
    resource: ../../plan/INPUT_RESEARCH.md
    title: Input system research, gap G8
---

# Scope

Unconditional. The four slots are in the config screen and in lba2.cfg like every other binding, and the player cannot tell them apart.

# Evidence

`partial`. `test_spell_keys_bypass.sh` presses a bound key and a spell key and reads the flow counters at the second boundary: a bound key is one key rise and one action rise, a spell key is one key rise and zero action rises, and the hero changes anyway. The direct reads of slots 32 and 33 are in the initial import.[^bypass-test][^original]

# Behaviour

`SpellKeyDown(n)` tests `DefKeys[n]` and `GamepadKeys[n]`, both keys each, with `CheckKey`, and returns the result; the four call sites in `MainLoop` are the penguin, the jetpack, the protection spell and the lightning spell. No `Input` bit is set, because none could be: the spell bits in INPUT.H are a second set of names on the low positions, `I_PINGOUIN` is `1 << 0`, the same bit as `I_UP`, and the bitfield has thirty-two positions for thirty-six slots.[^perso][^input-h]

# Why it is load bearing

- Route a spell through `Input` and it stops firing, or fires the movement it collides with. The bypass is the only way the four slots can work with the bitfield as it is, and the fixture counts exactly the asymmetry a reader would take for a bug.[^bypass-test]
- From the engine's side the slots are a hole in the table: a census of actions by `Input` bit undercounts by four, the second boundary's counters have to treat them as a known exception, and a recording's bindings digest covers them while the digest's `Input` never shows a spell was cast.[^research]
- A helper exists so the four reads stay one shape; the import wrote each as two `CheckKey` calls inline, and the helper added the pad's two keys when the pad became a binding source.[^perso]

# What it does not tell you

- **That a spell was cast.** `Input` never says so; the hero's behaviour change is the only trace.
- **That the table has thirty-six actions.** The bitfield has thirty-two; anything that derives the action set from the bits is four short.

[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `SpellKeyDown` and its four call sites in `MainLoop`.
[^input-h]: [SOURCES/INPUT.H](../../../SOURCES/INPUT.H), `I_PINGOUIN` and `I_UP`.
[^original]: Commit 333929ab, the initial import, SOURCES/PERSO.CPP, `CheckKey(DefKeys[33].Key1)` and the penguin's twin.
[^bypass-test]: [tests/automation/test_spell_keys_bypass.sh](../../../tests/automation/test_spell_keys_bypass.sh), the header comment.
[^research]: [docs/plan/INPUT_RESEARCH.md](../../plan/INPUT_RESEARCH.md), gap G8.
