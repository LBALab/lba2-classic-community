---
type: Quirk
title: The turn is an accumulator and the walk is a keyframe
description: Holding a direction and a rotation together walks the hero along an arc whose two halves are driven by different idioms, the rotation by a speed-times-time accumulator that carries its remainder and the walk by a keyframe step that does not, so the arc's shape, not only its length, depends on the simulation step; fixing either half alone changes the arc, and a fixture that pins today's arc at one step pins that coupling as design.
status: draft
scope: "a held direction with a held rotation, at any step other than 16 ms; the throttle holds production at 16"
equivalence: untested
asm_origin: "SOURCES/OBJECT.CPP:DoDir"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T22:00:00Z }
owner: /subsystems/movement.md
relates_to:
  - /quirks/the-keyframe-step-carries-no-remainder.md
  - /subsystems/input.md
sources:
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the manual move's two halves
  - id: move
    resource: ../../../LIB386/3D/MOVE.CPP
    title: MOVE.CPP, the accumulator the turn reads
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, SOURCES/OBJECT.CPP, the same two halves in the same case
  - id: research
    resource: ../../plan/INPUT_RESEARCH.md
    title: Input system research, open question 6
---

# Scope

A held direction together with a held rotation, which the manual move supports and always did, at any simulation step other than the reference. The throttle holds production at 16 ms, so the shape is reachable there only with the throttle off; under the harness it is reachable with any other `--fixed-dt`.

# Evidence

`untested`, derived from the two idioms and not measured in the tree. The research doc poses it as an open question; a session's measurement that the turn moves with the step while the walk stays put is not recorded anywhere re-derivable and is not cited. The two halves are in the initial import in the same case of the same routine.[^research][^original]

# Behaviour

In the manual move the left and right block is a separate test from the forward one, and its rotate branch is taken while a walk animation is playing, so the hero walks an arc. Turning is `ChangeSpeedMove` on the object's bound angle read back through `GetDeltaMove`: speed times elapsed game time with a carried remainder, independent of the step. Walking is the keyframe translation applied once per step with no carry, which is [the keyframe Quirk](/quirks/the-keyframe-step-carries-no-remainder.md) and depends on the step. Over the same game time the hero turns the same angle at any step and covers a different distance, so the arc tightens or opens with the step.[^object][^move]

# Why it is load bearing

- Fixing either half alone changes the arc. Make the walk an accumulator and the hero covers a different distance per turn than 1997 did; that is the correction [the timestep decision](/decisions/a-fixed-simulation-timestep-not-a-corrected-interpolator.md) declined, and this is one of the things it would have changed beyond distance.
- A fixture is a trap in both directions. One that pins today's arc at a step other than 16 pins a port regression as design; one that asserts the arc is step-independent asserts something the 1997 engine never did. The invariant that can be asserted is the one the throttle gives, the arc at 16 ms.
- It is why a held-direction fixture reads distance and not path. The combo fixtures assert which of two contradictory directions wins and how far the hero walks; none asserts where the walk went.

# What it does not tell you

- **The size of the effect.** Derived, not measured here. The direction is certain, the turn is independent and the walk is not; the magnitude at a given step is a measurement nobody has put in the tree.
- **That the reference arc is "right".** It is what shipped, at the step that shipped; the arc has no ideal to be compared with.

[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `DoDir`, the `MOVE_MANUAL` case: the forward and backward tests that call `InitAnim`, the separate left and right test that calls `ChangeSpeedMove`, and the `GetDeltaMove` read into `Beta`.
[^move]: [LIB386/3D/MOVE.CPP](../../../LIB386/3D/MOVE.CPP), `ChangeSpeedMove` and `GetDeltaMove`.
[^original]: Commit 333929ab, the initial import, SOURCES/OBJECT.CPP, `case MOVE_MANUAL` and its `GetDeltaMove` read into `Beta`.
[^research]: [docs/plan/INPUT_RESEARCH.md](../../plan/INPUT_RESEARCH.md), "Open questions", item 6.
