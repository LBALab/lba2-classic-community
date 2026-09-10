---
type: Decision
title: The force-step whitelist stays, and the input latch was not built
description: A frame the throttle would skip is promoted to a step when it carries one of three named one-frame signals, and the general cure, an accumulator that carries any input edge to the next step, was designed, measured against, and declined, because the whitelist was shown complete for the consumers that exist and the latch would have been a refactor with byte-exact risk and no live defect behind it.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T19:00:00Z }
owner: /subsystems/input.md
relates_to:
  - /quirks/the-edge-baseline-is-the-rendered-frame.md
  - /decisions/the-harness-meters-input-in-sim-ticks.md
  - /subsystems/timing.md
sources:
  - id: sim-plan
    resource: ../../plan/INPUT_SIM_PLAN.md
    title: Input simulation and the throttle-drop class, "Layer A" and the Phase 2 status
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the force-step call and the comment that lists the three signals
  - id: timer
    resource: ../../../LIB386/SYSTEM/TIMER.CPP
    title: TIMER.CPP, Timer_ForceStepIfPending
  - id: melee-test
    resource: ../../../tests/automation/test_melee_throttle.sh
    title: The guard that replaced the latch
---

# Context

The fixed-timestep throttle fixed frame-rate-dependent movement by simulating at most once per 16 ms of game time and rendering the frames in between. Three one-frame signals then went missing on skipped frames, one bug at a time: an inventory use, a full-scene flip, and the hero's action edges. Each got a term in `Timer_ForceStepIfPending`, which promotes a would-skip frame to a single step when a term is pending. The list is hand-kept, so the next one-frame signal read after the gate drops silently until someone adds it. The plan's answer was a latch: accumulate `Input` across rendered frames, advance the edge baseline only on steps, let the object loop read edges against the accumulator, and retire the input term. Byte-exact at one step per frame, where the accumulator equals the snapshot.[^sim-plan]

# Decision

Measure first, then decide. A red test placed a melee press on a skipped frame with the frame-precise injector, at an 8 ms step against a 24 ms timestep so every third frame steps. The current engine fired the attack on the step frame and on the skip frame alike, and dropped it only when the whitelist term was neutered. The other `Input` readers outside the whitelist, weapon and behaviour selection, the holomap, pause, the menus, all run before the gate and so on every rendered frame. So there was no live edge drop on main, and the latch would have been a pure refactor, with the byte-exactness proof concentrated in reconciling level reads with edge reads under a union accumulator, for no defect. The maintainer's call: skip the latch, keep the whitelist, and lock in the test. `test_melee_throttle.sh` asserts the attack fires on both a step and a skip frame, fails against the neutered engine, and is the guard the latch would have been.[^sim-plan][^melee-test]

# Non-goals

- **The latch, now.** It stays a documented option, to be built when a post-gate edge consumer lands outside the whitelist. The plan carries its design.[^sim-plan]
- **Treating the list as a cure.** It is complete for the readers that exist and says nothing about the next one; a new edge consumer after the gate is a change that has to look at the list.
- **Movement.** Held state the throttle is right to skip; forcing a step on every walk change would undo the frame-rate fix.
- **The stutter.** The felt roughness at high refresh is the render-and-simulate interpolation half of #412, not an edge drop, and is outside this.

[^sim-plan]: [docs/plan/INPUT_SIM_PLAN.md](../../plan/INPUT_SIM_PLAN.md), "Layer A: engine input latch" and the status lines for Phase 2.
[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the `Timer_ForceStepIfPending` call and the comment above it naming the three signals.
[^timer]: [LIB386/SYSTEM/TIMER.CPP](../../../LIB386/SYSTEM/TIMER.CPP), `Timer_ForceStepIfPending`.
[^melee-test]: [tests/automation/test_melee_throttle.sh](../../../tests/automation/test_melee_throttle.sh), the header comment.
