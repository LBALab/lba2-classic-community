---
type: Decision
title: Sub-steps hold the frame's input, edge on the first and steady state after
description: When a long frame runs several simulation steps, input stays the one sample the frame took; the first step sees the press or release edge against the previous frame's sample, and every later step holds both the current and the last sample at the frame's value, so a held action neither re-fires nor reads as released; the first cut masked the later steps to movement bits and cancelled every in-flight throw and attack.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T22:00:00Z }
owner: /subsystems/movement.md
relates_to:
  - /decisions/a-fixed-simulation-timestep-not-a-corrected-interpolator.md
  - /quirks/the-edge-baseline-is-the-rendered-frame.md
  - /quirks/initanim-returns-early-when-the-animation-is-active.md
  - /subsystems/input.md
sources:
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the comment and the hold at the sim_step label
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the momentary-action reset paths in the manual move
  - id: substep-test
    resource: ../../../tests/automation/test_action_substep.sh
    title: The fixture that holds an action across sub-steps
  - id: movement-doc
    resource: ../../MOVEMENT_FRAMERATE.md
    title: Movement is frame-rate dependent, "The catch, input re-entrancy"
---

# Context

Running the simulation block more than once per rendered frame re-runs the hero's object loop, and hero actions are level-triggered off held input: the jump path starts a jump whenever it runs with the action held, and combat and throwing are the same shape. Sub-stepping therefore fired a held action once per step, a double jump on a long frame. Input cannot be re-polled per step, since the frame has one sample, and the sample cannot simply be dropped after the first step, because the object loop also reads releases off the momentary bits: a throw ends when the throw bit is seen absent, and the idle reset does the same for the fire flag. The first cut masked the later steps down to the movement bits, which the loop read as the player letting go, so below 60 fps a held throw or attack was cancelled every step and never reached its action frame.[^movement-doc][^perso]

# Decision

The frame's sample is held across its steps, and the edge is consumed once. On the first step `LastInput` is still the previous frame's sample, so a press or a release is seen there and nowhere else. On every later step both `Input` and `LastInput` are set to the frame's sample: the held bits are present, so nothing reads a release, and they equal the last sample, so nothing reads a press. A held action then re-presents the same steady state each step, which is safe because starting an animation that is already running is a no-op and projectile spawns are gated on an animation frame rather than on the input. `LastInput` is restored after the loop. At one step per frame the branch never runs, so the historical path is byte for byte unchanged.[^perso][^object]

# Non-goals

- **Re-polling per step.** A frame has one sample; the steps advance time, not input.
- **A per-step input command.** Building one command per step, the way a ticker engine does, is the general answer and a different design; this keeps the frame's sample and the object loop as they are.
- **Masking.** Dropping bits between steps is a release to the code that reads releases, which is the defect this replaced.

[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the comment beginning "Input is sampled once per rendered frame" at the `sim_step` label.
[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), the `MOVE_MANUAL` case, the `LastInput` tests that end a throw and reset the idle.
[^substep-test]: [tests/automation/test_action_substep.sh](../../../tests/automation/test_action_substep.sh), the header comment.
[^movement-doc]: [docs/MOVEMENT_FRAMERATE.md](../../MOVEMENT_FRAMERATE.md), "The catch: input re-entrancy" and the Phase 2 paragraph.
