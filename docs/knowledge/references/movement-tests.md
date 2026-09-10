---
type: Reference
title: Movement tests, host and automation
description: The host tests behind the movement Subsystem, a characterisation that asserts the frame-rate coupling is present, the step planner's invariance, and the ASM-against-C++ interpolator test that asserts the overshoot discard as correct, and the automation fixtures that drive a save through the throttle at several step sizes.
status: draft
resource: ../../../tests/movement/test_move_framerate.cpp
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T22:00:00Z }
sources:
  - id: framerate
    resource: ../../../tests/movement/test_move_framerate.cpp
    title: The coupling, characterised on a synthetic walk
  - id: step
    resource: ../../../tests/timer/test_fixed_step.cpp
    title: The step count over any chunking of a span
  - id: interdep
    resource: ../../../tests/ANIM/test_interdep.cpp
    title: The interpolator against its ASM
  - id: move-framerate
    resource: ../../../tests/automation/test_move_framerate.sh
    title: High-frame-rate invariance through the engine
  - id: move-substep
    resource: ../../../tests/automation/test_move_substep.sh
    title: Low-frame-rate invariance through the engine
  - id: action-substep
    resource: ../../../tests/automation/test_action_substep.sh
    title: A held action across sub-steps
---

# What they pin

Host tests, no retail data, in CI:

- `tests/movement` drives the real keyframe step on a synthetic four-keyframe walk with no game data and asserts that the coupling is present: full distance at high sampling, a plateau below about 60 fps. It is a characterisation and a root-cause proof, deliberately not an invariance test, since the fix pins the step rather than the arithmetic.[^framerate]
- `tests/timer/test_fixed_step.cpp` asserts the property that makes the fix work: over any span of wall time the total number of steps is the span divided by the step, whether the time arrives steady, fine, coarse or jittery, plus the catch-up clamp and the remainder carry.[^step]
- `tests/ANIM/test_interdep.cpp` runs the interpolator in its ASM and C++ forms on the same inputs and asserts the frame advance, including the re-anchor that discards overshoot, because the oracle does the same.[^interdep]

Automation fixtures, a real engine headless, retail data required:

- The two invariance fixtures drive the same save over the same game time at step sizes that emulate a high and a low frame rate and assert the displacement agrees with the reference within a tolerance; before the fix the high-rate run lost distance and the low-rate run lost more.[^move-framerate][^move-substep]
- The action fixture holds a throw across sub-steps at a low emulated frame rate and asserts the throttled path reaches the action frame, byte-identical to the unthrottled path over the same input.[^action-substep]

What none of them pins: the feel above 60 fps, which is presentation and not simulation, and the shape of a walking turn at step sizes other than the reference.

[^framerate]: [tests/movement/test_move_framerate.cpp](../../../tests/movement/test_move_framerate.cpp), the file comment.
[^step]: [tests/timer/test_fixed_step.cpp](../../../tests/timer/test_fixed_step.cpp), the file comment.
[^interdep]: [tests/ANIM/test_interdep.cpp](../../../tests/ANIM/test_interdep.cpp), `test_cpp_frame_advance`.
[^move-framerate]: [tests/automation/test_move_framerate.sh](../../../tests/automation/test_move_framerate.sh), the header comment.
[^move-substep]: [tests/automation/test_move_substep.sh](../../../tests/automation/test_move_substep.sh), the header comment.
[^action-substep]: [tests/automation/test_action_substep.sh](../../../tests/automation/test_action_substep.sh), the header comment.
