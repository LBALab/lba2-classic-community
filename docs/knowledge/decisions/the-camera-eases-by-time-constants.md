---
type: Decision
title: The Auto camera's turn and look-at point ease by time constants on the game clock
description: The turn behind the hero and the look-at point's trail are eased by a time constant in milliseconds on the game clock, 300 by default as in the 2022 Classic release, with a step that cannot overshoot and that carries its remainder, instead of the release's integer ease, which stops short of its goal and overshoots on long frames.
status: draft
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
owner: /subsystems/camera.md
sources:
  - id: pr-ease
    resource: https://github.com/LBALab/lba2-classic-community/pull/702
    title: The turn and the look-at point paced by the clock
  - id: math
    resource: ../../../SOURCES/FOLLOWCAM_MATH.H
    title: FOLLOWCAM_MATH.H, the ease step
  - id: cfg
    resource: ../../../SOURCES/FOLLOWCAM_CFG.H
    title: FOLLOWCAM_CFG.H, the defaults and the longest frame an ease is paced by
  - id: followcam
    resource: ../../../SOURCES/FOLLOWCAM.CPP
    title: FOLLOWCAM.CPP, the clock the eases read and the rules that restart it
  - id: math-test
    resource: ../../../tests/camera/test_followcam_math.cpp
    title: The host test of the ease step
---

# Context

The Auto camera turned behind the hero by a share per update and stopped inside a 10 unit snap threshold, up to a degree short, until something else redrew the frame; a quarter turn came within 5% in 1664 ms. The look-at point was pinned to the hero. The 2022 Classic release eases both by the clock with a 300 ms time constant.[^pr-ease] Its arithmetic, read from its executable and emulated rather than taken from anything in the tree, is integer: it stops short of its goal by up to 300 / dt units, 18 at 16 ms frames, and overshoots on a frame longer than the constant.

# Decision

- **A time constant, read from the game clock.** `cam_ease_ms` and `cam_target_ms` are time constants in milliseconds, 300 by default; a quarter turn comes within 5% in 912 ms and lands on its target. The clock is `TimerRefHR`, which stands still in a menu and is what a replay reproduces.[^pr-ease][^cfg]
- **A step that cannot overshoot.** `FollowCamEaseStepMs` takes `2dt / (2tau + dt)` of what is left, which follows `1 - exp(-dt / tau)` within one percent while an update is under a third of the time constant, and cannot pass the target until an update is twice the constant. The plain `dt / tau` runs a tenth fast at 20 updates a second and overshoots on an update longer than the constant. The move is worked in thousandths of a unit and the remainder carried, since dropping it made the same turn a tenth slower at 8 ms updates than at 16.[^math][^math-test]
- **Time that was not a frame is not spent.** An update that does not follow a simulation step on the previous step is paced by no time; a cut restarts the clock; a long update is paced as 100 ms (`FOLLOW_CAM_EASE_DT_MAX`).[^followcam][^cfg]
- **0 is the old camera.** The per-update lerp stays behind `cam_ease_ms 0`, and a look-at point pinned to the hero behind `cam_target_ms 0`.

# Non-goals

- **The release's arithmetic.** Its time constant is kept, its integer step is not.
- **A player's orbit.** An orbit the player is driving keeps its own per-update lerp (`cam_smooth`), which smooths a device rather than following a target.
- **Fixing frame-rate dependence.** With the fixed simulation step on, the per-update lerp was already frame-rate independent; what the ease changes there is its pace and its landing, not its dependence on the frame rate. With the step off it changes both.

[^pr-ease]: Pull request 702 on origin, the before and after and the release's time constant.
[^math]: [SOURCES/FOLLOWCAM_MATH.H](../../../SOURCES/FOLLOWCAM_MATH.H), `FollowCamEaseStepMs` and the comment above it.
[^cfg]: [SOURCES/FOLLOWCAM_CFG.H](../../../SOURCES/FOLLOWCAM_CFG.H), `FOLLOW_CAM_EASE_MS_DEFAULT`, `FOLLOW_CAM_TARGET_MS_DEFAULT` and `FOLLOW_CAM_EASE_DT_MAX`.
[^followcam]: [SOURCES/FOLLOWCAM.CPP](../../../SOURCES/FOLLOWCAM.CPP), `FollowCamEaseTick` and `FollowCamNoteFrame`.
[^math-test]: [tests/camera/test_followcam_math.cpp](../../../tests/camera/test_followcam_math.cpp), the ease step's cases.
