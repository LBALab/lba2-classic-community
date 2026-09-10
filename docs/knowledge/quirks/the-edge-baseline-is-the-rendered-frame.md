---
type: Quirk
title: The input edge baseline advances once per rendered frame
description: LastInput is assigned at the top of every MainLoop iteration and the hero's press and release edges are read against it in the object loop, which was the same frame in 1997 and is a simulated step under the port's fixed-timestep throttle, so an edge that lands on a skipped frame is never seen unless a hand-kept whitelist promotes that frame to a step.
status: draft
scope: "every build; the gap opens only when a frame is rendered without a simulated step, which the throttle does above about 60 fps"
equivalence: partial
asm_origin: "SOURCES/PERSO.CPP:MainLoop"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T19:00:00Z }
owner: /subsystems/input.md
verified_against:
  - /references/input-tests.md
relates_to:
  - /decisions/the-force-step-whitelist-stays.md
  - /decisions/the-harness-meters-input-in-sim-ticks.md
  - /subsystems/timing.md
sources:
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the LastInput assignment and the force-step call with its comment
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the edge reads in the hero's manual move
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, the same assignment in MainLoop and the same edge reads
  - id: sim-plan
    resource: ../../plan/INPUT_SIM_PLAN.md
    title: Input simulation and the throttle-drop class, "Why framestep still gobbles input"
  - id: melee-test
    resource: ../../../tests/automation/test_melee_throttle.sh
    title: The fixture that places a press on a skipped frame
---

# Scope

Every build, because the baseline and the readers are original. The gap between them opens only when the fixed-timestep throttle renders a frame without simulating one, which at a 16 ms step happens above about 60 fps, and which `--fixed-dt 16` never does.

# Evidence

`partial`. `test_melee_throttle.sh` and `test_blowgun_release_throttle.sh` place a press or a release on a frame the throttle skips and assert the hero acts; they pin the whitelist that closes the gap, and they drive this baseline to do it. They were validated against a neutered whitelist, where both fail. The baseline itself is in the initial import, the assignment in `MainLoop` and the edge reads in the hero's move code.[^melee-test][^original]

# Behaviour

`LastInput = Input` runs at the top of every iteration, before `MyGetInput` samples the new `Input`. Hero actions are edge-triggered against it in the object loop: weapon fire starts on the press of `I_THROW` and stops on its release, melee starts on the press of `I_ACTION_M` with an anti-repeat that tests `LastInput`. In 1997 the iteration, the frame and the simulation step were one thing. The port's throttle runs the object loop only on stepped frames while the assignment still runs every rendered frame, so a press and its release that straddle a skipped frame are never observed: the blowgun kept firing on its own and melee never started while moving. The cure is a gate before the simulation block: `Timer_ForceStepIfPending` promotes a would-skip frame to one step when it carries a named one-frame signal, a pending inventory use, a full-scene flip, or an `Input` edge in `I_ACTION_EDGE`. Movement is excluded on purpose, since it is held state the throttle is right to skip. When a long frame sub-steps, only the movement bits are kept after the first sub-step so a held action does not re-fire.[^perso][^object]

# Why it is load bearing

- The whitelist is complete for the consumers that exist and silent for the next one. Every `Input` reader outside `I_ACTION_EDGE`, weapon and behaviour selection, the holomap, pause and the menus, runs before the gate and so on every rendered frame; the only post-gate edge readers are the object loop's action edges, which the whitelist names. A new edge consumer added after the gate and outside the list drops on skipped frames with nothing said. That is why the list is the subject of [a decision](/decisions/the-force-step-whitelist-stays.md) and not just a fix.[^sim-plan]
- Moving the baseline into the simulation domain, an accumulator that carries edges to the next step, is byte-exact at one step per frame and would retire the list; it was designed, measured against, and not built.[^sim-plan]
- A harness that injects input has the same exposure as a player. Counting a hold in rendered frames let a hold expire before any step consumed it, which is why [the injector meters in simulated ticks](/decisions/the-harness-meters-input-in-sim-ticks.md) and must not force a step because it injected.[^sim-plan]

# What it does not tell you

- **That an edge was dropped.** Nothing reports it; the boundary counters in INPUT_FLOW.H are the only instrument and they count rather than fail.
- **Which frame was skipped.** Under `--fixed-dt 16` none is, so a headless fixture reproduces the gap only by widening it, a small `--fixed-dt` against a larger `--fixed-timestep`, and a frame-precise injector to land the press.[^melee-test]

[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the `LastInput` assignment before `MyGetInput` and the `Timer_ForceStepIfPending` call with the comment above it.
[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), the `LastInput` tests in the hero's manual move.
[^original]: Commit 333929ab, the initial import, SOURCES/PERSO.CPP and SOURCES/OBJECT.CPP, the same assignment and the same two edge reads.
[^sim-plan]: [docs/plan/INPUT_SIM_PLAN.md](../../plan/INPUT_SIM_PLAN.md), "Why framestep still gobbles input", "Layer A" and the Phase 2 status.
[^melee-test]: [tests/automation/test_melee_throttle.sh](../../../tests/automation/test_melee_throttle.sh), the header comment.
