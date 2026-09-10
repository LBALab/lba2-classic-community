---
type: Decision
title: The harness meters injected input in simulated ticks
description: A held action mask from the input timeline counts down once per simulated step and is ORed into Input on every rendered frame, so a hold means the same number of consumptions at any frame cadence; a frame-metered variant exists to place a press on a skipped frame, and the harness never forces a step because it injected.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T19:00:00Z }
owner: /subsystems/control.md
relates_to:
  - /subsystems/input.md
  - /quirks/the-edge-baseline-is-the-rendered-frame.md
  - /decisions/the-force-step-whitelist-stays.md
sources:
  - id: sim-plan
    resource: ../../plan/INPUT_SIM_PLAN.md
    title: Input simulation and the throttle-drop class, "Layer B" and the metering decision
  - id: control-h
    resource: ../../../SOURCES/CONTROL.H
    title: CONTROL.H, the timeline's surface
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the injection block after MyGetInput
  - id: hold-test
    resource: ../../../tests/automation/test_input_hold_throttle.sh
    title: The invariance fixture
  - id: pr
    resource: https://github.com/LBALab/lba2-classic-community/pull/438
    title: feat(input), the sim-tick input timeline
---

# Context

The first injector ORed a debug mask into `Input` after the poll and counted its duration down in rendered frames. Under the fixed-timestep throttle a rendered frame and a simulated step are different counts, so a hold metered in frames could expire before any step consumed it. Measured on the Desert Island save at an 8 ms step: a thirty-frame forward hold walked the hero about 129 units with the throttle off, 123 at a 16 ms timestep, and zero at 100, where the hold ran out before a step ran. The scene was not the variable; the baseline with no input was identical at all three. The injector had the failure it was built to test for.[^sim-plan]

# Decision

Meter in simulated ticks. `input <flags> [ticks]` and `input seq` hold a mask whose countdown advances once per frame the simulation runs, while the mask is ORed into `Input` on every rendered frame so whichever step lands sees it; a skipped frame consumes nothing. This is the one unit that guarantees a given number of consumptions at any cadence. The invariant it buys, and the property a test asserts, is that a hold over the same game time moves the hero the same distance at any throttle: thirty seconds of forward glide on the high-altitude save came to 8612 units at a 16 ms timestep and 8515 at 100, where the frame-metered count had collapsed to zero. `input fseq` is the deliberate exception, metered in rendered frames on its own clock, so that a press can be placed on a frame the simulation skips; it exists to reproduce the edge drop, not to drive the hero. And the harness does not force a step because it injected: it injects and lets the throttle do its worst, because a harness that stepped for its own presses would pass while a player still dropped.[^sim-plan][^control-h][^perso]

# Non-goals

- **Game-time metering.** Distance per tick scales with the timestep, so a hold of thirty ticks walks farther at a coarser throttle. That is the throttle's real coarseness, not a metering error, and not a property to assert.
- **Forcing a step on injection.** The engine's gate decides what promotes a frame; the harness stays a player.
- **The engine-side cure.** Whether a dropped edge is carried to the next step is [a decision of its own](/decisions/the-force-step-whitelist-stays.md).
- **Modal-crossing timelines.** A hold that spans a menu needs the modal driven from outside, which the timeline does not do.

[^sim-plan]: [docs/plan/INPUT_SIM_PLAN.md](../../plan/INPUT_SIM_PLAN.md), "Phase 0 findings", "Layer B" and the paragraph beginning "Why sim ticks and not game-time".
[^control-h]: [SOURCES/CONTROL.H](../../../SOURCES/CONTROL.H), `Control_InputSchedule`, `Control_InputScheduleFrame`, `Control_InputHold` and `Control_InputCurrentMask`.
[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the block after `MyGetInput` that ORs `Control_InputCurrentMask` into `Input`.
[^hold-test]: [tests/automation/test_input_hold_throttle.sh](../../../tests/automation/test_input_hold_throttle.sh).
[^pr]: Pull request 438 on origin.
