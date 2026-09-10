---
type: Subsystem
title: Movement
description: Three idioms move things, and which one a thing uses decides whether it is frame-rate independent; the hero and every walker use the one that is not, a keyframe step applied once per simulated frame with no carried remainder, so the port pins the simulation to the 60 fps reference the keyframes were authored for rather than correcting the arithmetic the ASM oracle asserts.
status: draft
subsystem: movement
as_of: b7612652
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T22:00:00Z }
relates_to:
  - /subsystems/timing.md
  - /subsystems/input.md
  - /subsystems/transitions.md
  - /porting/anim.md
  - /decisions/the-force-step-whitelist-stays.md
  - /decisions/presents-mint-the-pinned-step.md
sources:
  - id: movement-doc
    resource: ../../MOVEMENT_FRAMERATE.md
    title: Movement is frame-rate dependent (docs/MOVEMENT_FRAMERATE.md)
  - id: render-split
    resource: ../../plan/ENGINE_RENDER_SPLIT_RESEARCH.md
    title: Engine and render split research, the five couplings
  - id: interdep
    resource: ../../../LIB386/ANIM/INTERDEP.CPP
    title: INTERDEP.CPP, the keyframe step
  - id: move
    resource: ../../../LIB386/3D/MOVE.CPP
    title: MOVE.CPP, the accumulator with a carried remainder
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the manual move, the per-object animation call and InitAnim
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the sub-step loop in MainLoop
  - id: timer
    resource: ../../../LIB386/SYSTEM/TIMER.CPP
    title: TIMER.CPP, the step planner and the step count
  - id: global
    resource: ../../../SOURCES/GLOBAL.CPP
    title: GLOBAL.CPP, the timestep's default and its comment
---

Distilled from [docs/MOVEMENT_FRAMERATE.md](../../MOVEMENT_FRAMERATE.md), which stays the account of the defect, its evidence and the fix's phases, and from [docs/plan/ENGINE_RENDER_SPLIT_RESEARCH.md](../../plan/ENGINE_RENDER_SPLIT_RESEARCH.md), which owns the survey of what still ties the simulation to the render. The principles are structural. The loop's shape and the cap are read at `as_of`.

# Principles

- **Three idioms, and the one that walks is the one that is not frame-rate independent.** An accumulator with a carried remainder, `MOVE`, drives turning, falling, conveyors, projectiles and vehicles, and is independent of how finely time is sampled. A deadline on the game clock drives the inventory's rotation, the credits and the menu timeouts, and is independent too. A keyframe step applied once per simulated frame drives every walker, and is not: it is [a Quirk](/quirks/the-keyframe-step-carries-no-remainder.md), inherent to 1997 and invisible at the stable frame rate of the period.[^movement-doc]
- **Speed is a mode, not an axis.** A direction bit selects an animation, `GEN_ANIM_MARCHE` forward, `GEN_ANIM_RECULE` back, and the displacement is baked into that animation's keyframe translations. Which animation plays is chosen by the behaviour, so the hero's speed is a property of the mode and there is no scalar anywhere for a stick to fill. [Input](/subsystems/input.md) inherits the consequence.[^object]
- **The port pins the step rather than correcting the arithmetic.** Every user is put at the 60 fps reference the keyframes were authored for by simulating in fixed 16 ms steps, zero, one or several per rendered frame, instead of making the keyframe step carry a remainder, which would diverge from the ASM oracle and break the equivalence tests. That is [a decision](/decisions/a-fixed-simulation-timestep-not-a-corrected-interpolator.md), and it pins cadence only: it is not a collision fix.[^movement-doc]
- **A frame's input is one sample, however many steps it runs.** Input is polled once per rendered frame and held across the frame's sub-steps; the first step sees the edge and the rest see steady state, by [a decision](/decisions/sub-steps-hold-the-frames-input.md) that replaced a mask which read a held action as a release.[^perso]

# Contracts

| Contract | Statement |
|---|---|
| The keyframe step | `ObjectSetInterDep` is called once per object per simulated frame from `GereObjAnim`, with no catch-up loop. It interpolates the current keyframe's translation to the object's time, subtracts what it already applied in local space, rotates the difference to world space and adds the integer result to the position; when a keyframe completes it re-anchors the next window to the object's time and discards the overshoot. Called with a clock earlier than the object's own it shifts its window back and does nothing.[^interdep] |
| The accumulator | `ChangeSpeedMove` and `GetDeltaMove` bank speed times elapsed game time into `Acc` and hand out the whole part, keeping the fraction, so a turn, a fall or a projectile covers the same ground over a span of game time however the span is chunked. `StepFalling` and `StepShifting` are read from it once per simulated step for every object.[^move] |
| The reference rate | The keyframe step tracks game time only near 60 fps: above it the per-frame deltas round toward zero and a slow mover freezes; below it the discarded overshoot lets the animation fall behind. Measured over 3200 ms of game time on one save, the scripted mover covers 47% of its 60 fps distance at 1000 fps and 88% at 16, the slow mover 0% and 71%.[^movement-doc] |
| The throttle | `FixedTimestep`, 16 by default, persisted in lba2.cfg and settable live with the `fixedtimestep` console verb; zero is the historical per-frame path byte for byte. Each rendered frame runs `elapsed / FixedTimestep` simulation steps, where elapsed is the game time banked since the last step: zero above the rate, one at it, several below it, at most `FIXED_TIMESTEP_MAX_STEPS` with the backlog dropped. Each step sets `TimerRefHR` to its own value and the remainder carries; a rewind of the game clock re-anchors and runs no step that frame; a jump beyond the cap re-anchors and runs one. `Timer_PlanSimSteps` and `Timer_FixedStepCount` are the pure, host-tested core.[^perso][^timer] |
| Byte identity at the reference | Under `--fixed-dt 16` elapsed is exactly one step, so the throttle never skips and never sub-steps, and the on path is byte for byte the off path. That is what keeps the ASM-equivalence tests and the projection golden green, and it is why a headless fixture reproduces either loss only by choosing another `--fixed-dt`.[^perso] |
| Modals | The inventory's rotation and zoom, the credits' scroll and the menu timeouts are time-based already; the holomap's manual rotation was the one modal that advanced per frame, and it is scaled by the step now.[^movement-doc] |
| The one-frame signals | A frame the throttle would skip is promoted to a step when it carries an inventory use, a full-scene flip or a hero action edge; the list and why it stays a list are [the whitelist decision](/decisions/the-force-step-whitelist-stays.md). |

# Seams

- **`GereObjAnim`**, the one call of the keyframe step per object per step.[^object]
- **The step block in `MainLoop`**, from `Timer_PlanSimSteps` through the `sim_step` label to the restore of `TimerRefHR` and `LastInput` after the loop; everything inside runs per step, the render and the frame-level UI once.[^perso]
- **`Timer_PlanSimSteps`** and its rewind and large-jump guards, the only place the step grid is anchored.[^timer]
- **`fixedtimestep`** in the console, and the `FixedTimestep` key in lba2.cfg.[^global]
- **`--fixed-dt`**, the harness's other clock: it fixes what a frame advances, the throttle fixes what a step advances, and the two coincide at 16.
- **The ported routine.** `ObjectSetInterDep` is ASM with a C++ twin, row and test in [the ANIM porting status](/porting/anim.md); a change to its arithmetic is a change to the oracle.

# What it does not tell you

- **That distance is a velocity.** Nothing walks at a speed times a time; a walker covers what its keyframes say, once per step, and the same held direction covers different ground at a different step size.
- **That the render is smooth.** The throttle re-presents the settled pose on skipped frames, so above 60 fps motion updates sixty times a second whatever the display does; the interpolated render that would smooth it was designed, measured as imperceptible on every display to hand, and parked.[^render-split]
- **That a walk and a turn scale alike.** The turn is an accumulator and the walk is a keyframe, so a held direction and rotation carve an arc whose halves respond to the step differently; see [the arc Quirk](/quirks/the-turn-is-an-accumulator-and-the-walk-is-a-keyframe.md).
- **What the simulation step is, to the world.** The world still runs inside the render's iteration: modal loops own the clock, game code calls the renderer, the render writes world state, and a present is also the tick. The step is a loop around one block of that iteration, not a ticker of its own.[^render-split]

[^movement-doc]: [docs/MOVEMENT_FRAMERATE.md](../../MOVEMENT_FRAMERATE.md), "The short version", "Evidence", "Mechanism", "Fix" and "Implementation plan".
[^render-split]: [docs/plan/ENGINE_RENDER_SPLIT_RESEARCH.md](../../plan/ENGINE_RENDER_SPLIT_RESEARCH.md), "What still ties the two together"; [docs/plan/RENDER_INTERP_PLAN.md](../../plan/RENDER_INTERP_PLAN.md) for the parked interpolation.
[^interdep]: [LIB386/ANIM/INTERDEP.CPP](../../../LIB386/ANIM/INTERDEP.CPP), `ObjectSetInterDep`.
[^move]: [LIB386/3D/MOVE.CPP](../../../LIB386/3D/MOVE.CPP), `ChangeSpeedMove`, `GetDeltaMove` and `GetDeltaAccMove`.
[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `GereObjAnim`, the `MOVE_MANUAL` case and `InitAnim`.
[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the comment above `Timer_PlanSimSteps`, the `sim_step` label and the comment above the input hold.
[^timer]: [LIB386/SYSTEM/TIMER.CPP](../../../LIB386/SYSTEM/TIMER.CPP), `Timer_PlanSimSteps` and `Timer_FixedStepCount`.
[^global]: [SOURCES/GLOBAL.CPP](../../../SOURCES/GLOBAL.CPP), `FixedTimestep` and the comment above it.
