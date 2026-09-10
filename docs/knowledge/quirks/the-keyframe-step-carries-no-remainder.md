---
type: Quirk
title: The keyframe step carries no remainder
description: ObjectSetInterDep adds a rotated, integer-rounded delta to the position once per simulated frame and, when a keyframe completes, re-anchors the next window to the object's time and discards the overshoot; both losses are in the 1997 ASM and the equivalence test asserts the discard as correct, so a walker tracks game time only near 60 fps and the port pins the step rather than the arithmetic.
status: draft
scope: "every animation-driven walker, hero and NPC, at any step size other than 16 ms; the throttle holds production at 16"
equivalence: tested
asm_origin: "LIB386/ANIM/INTERDEP.ASM:ObjectSetInterDep"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T22:00:00Z }
owner: /subsystems/movement.md
ports: /porting/anim.md#objectsetinterdep
verified_against:
  - /references/asm-equivalence-suite.md
  - /references/movement-tests.md
relates_to:
  - /decisions/a-fixed-simulation-timestep-not-a-corrected-interpolator.md
sources:
  - id: interdep
    resource: ../../../LIB386/ANIM/INTERDEP.CPP
    title: INTERDEP.CPP, ObjectSetInterDep
  - id: interdep-asm
    resource: ../../../LIB386/ANIM/INTERDEP.ASM
    title: INTERDEP.ASM, the original
  - id: interdep-test
    resource: ../../../tests/ANIM/test_interdep.cpp
    title: The equivalence test, including the frame advance
  - id: framerate-test
    resource: ../../../tests/movement/test_move_framerate.cpp
    title: The characterisation on a synthetic walk
  - id: movement-doc
    resource: ../../MOVEMENT_FRAMERATE.md
    title: Movement is frame-rate dependent, "Mechanism"
---

# Scope

Every object whose motion is an animation, which is the hero and every walking NPC, at any simulation step other than the 16 ms the keyframes were authored against. The throttle holds production at 16, so the loss is reachable there only with the throttle off; under the harness it is reachable with any other `--fixed-dt`.

# Evidence

`tested`, twice over. The ASM and C++ forms run on the same inputs in `tests/ANIM/test_interdep.cpp`, and its frame-advance case asserts the re-anchor that discards overshoot, because the oracle does the same. `tests/movement/test_move_framerate.cpp` drives the C++ step on a synthetic four-keyframe walk and asserts the coupling is present, full distance at fine sampling and a plateau below about 60 fps. The porting status row is `tested`.[^interdep-test][^framerate-test]

# Behaviour

Once per object per simulated frame, `GereObjAnim` calls `ObjectSetInterDep`. The routine interpolates the current keyframe's translation to the object's time, subtracts what it applied last time in local space, rotates the difference into world space, and adds the integer result to the position. Two things are lost. The rotation's output is integer and nothing carries its fraction, so at a fine step each delta is sub-unit and rounds toward zero; a slow mover freezes at a thousand frames a second. And when the object's time passes the keyframe's end, the next window is anchored at the object's time rather than at the keyframe's intended end, so the overshoot is discarded; at a coarse step more is discarded per boundary and the animation, with the walk baked into it, falls behind game time. Called with a clock earlier than its own, the routine shifts its window back by the difference and applies nothing.[^interdep][^movement-doc]

# Why it is load bearing

- It is the whole of #358, and it is not a port bug. Movement code had no behavioural change in the port; what changed was exposure, vsync as the only cap, an honest game clock, a costlier default render. Measured on one save over 3200 ms of game time, a scripted mover covers 47% of its 60 fps distance at 1000 fps and 88% at 16, a slow one 0% and 71%.[^movement-doc]
- The surgical fix is the wrong one here. Carrying the remainder makes the routine disagree with its ASM, and the equivalence test and the projection golden exist to report exactly that disagreement. The port pins the step at 16 ms instead, which is [a decision](/decisions/a-fixed-simulation-timestep-not-a-corrected-interpolator.md), and the routine runs in production at the step it always ran at under the harness.[^movement-doc]
- The step size is a hidden parameter of every fixture. A displacement measured at one `--fixed-dt` is not comparable to one at another, and a fixture that pins a distance at a step other than 16 pins the loss as design.

# What it does not tell you

- **Its own rate.** Nothing in the routine says 60 fps; the reference is the rate the keyframes were authored at and is recovered only by measurement.
- **Whether a rewind was a rewind.** Called with an earlier clock it applies nothing and shifts its window, which is what a modal's timer restore or a cube change produces; the object cannot tell that from a frame that did not run.

[^interdep]: [LIB386/ANIM/INTERDEP.CPP](../../../LIB386/ANIM/INTERDEP.CPP), `ObjectSetInterDep`: the rewind branch, the interpolator, the `LastAnimStep` subtraction, the `RotatePoint` call and the re-anchor of `LastTimer` and `NextTimer`.
[^interdep-asm]: [LIB386/ANIM/INTERDEP.ASM](../../../LIB386/ANIM/INTERDEP.ASM).
[^interdep-test]: [tests/ANIM/test_interdep.cpp](../../../tests/ANIM/test_interdep.cpp), `test_cpp_frame_advance`.
[^framerate-test]: [tests/movement/test_move_framerate.cpp](../../../tests/movement/test_move_framerate.cpp), the file comment.
[^movement-doc]: [docs/MOVEMENT_FRAMERATE.md](../../MOVEMENT_FRAMERATE.md), "Evidence", "Mechanism" and "Why it looks like a recent regression".
