---
type: Decision
title: The 2022 Classic release's camera behaviours join the one Auto camera, each as a switch
description: The behaviours read out of the 2022 Classic release's auto camera came into the existing Auto camera as cam_* settings, on by default while it is on, instead of a second camera mode beside it, because the release's goal is the Auto camera's goal with the player's offsets at zero and a second mode would double the hand-over paths the camera's bugs have come from.
status: draft
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
owner: /subsystems/camera.md
relates_to:
  - /decisions/a-camera-setting-names-its-value-from-before.md
sources:
  - id: pr-feel
    resource: https://github.com/LBALab/lba2-classic-community/pull/701
    title: The Auto camera around buildings, scripted shots and scenery
  - id: pr-ease
    resource: https://github.com/LBALab/lba2-classic-community/pull/702
    title: The Auto camera's turn and look-at point paced by the clock
  - id: cfg
    resource: ../../../SOURCES/FOLLOWCAM_CFG.H
    title: FOLLOWCAM_CFG.H, the defaults and the comments naming the release's values
  - id: internals
    resource: ../../CAMERA_INTERNALS.md
    title: Camera internals (docs/CAMERA_INTERNALS.md)
---

# Context

The maintainer judged the 2022 Classic release's auto camera more polished than the Auto camera here and wanted much of it brought in. Four behaviours were read out of its executable and emulated: carrying the camera across a change between outdoor scenes, passing over a shot that fires once while the player steers, clearing scenery as well as terrain, and easing by milliseconds. The Auto camera already had what the release does not: a free orbit, zoom, a player tilt, hold-angle and the HD recompose. The question was whether the release's camera comes in beside it as a second mode or into it.

# Decision

Into it. Each behaviour is a `cam_*` setting on the one Auto camera (`cam_scene_carry`, `cam_zone_skip`, `cam_obstruct`, `cam_ease_ms`, `cam_target_ms`), switched and tuned live from the console and kept in `lba2.cfg`, and nothing the Auto camera had is replaced.[^pr-feel][^pr-ease] Three reasons:

- The release aims behind the hero plus `AddBetaCam`, which is the Auto camera's own goal with the player's offsets at zero, so its behaviours are about reaching and keeping that goal and compose with an orbit.
- `FollowCamera` already reads `AutoCameraCenter`, which the reading of the release's executable found to be its own key, so a player coming from it lands on this camera.
- A second mode doubles the hand-over paths, a camera zone, a cutscene, an interior and a scene change, and those paths are where the camera's bugs have come from.

Agreed with it: the millisecond ease is ported without the release's integer arithmetic, which stops short of its goal and overshoots on long frames ([the ease Decision](/decisions/the-camera-eases-by-time-constants.md)); exact retail feel, if anyone wants it, is a preset of setting values, not a code path; and the Auto camera itself stays off by default. The shipped defaults were set on by the maintainer after playing the build at 736x480 and 1920x1080 before it merged.[^pr-feel] The defaults and the release's values are in the configuration header.[^cfg]

Where the release's answer and an existing prototype disagreed, both became settings and play decided: for a shot that fires once, the release passes it over and `cam_zone_return` fires it and gives the view back; `cam_zone_skip` ships on, and the two are complementary for shots that hold the view.[^internals]

# Non-goals

- **A retail camera mode.** There is no switch that turns the Auto camera into the release's camera; there are switches for what was ported.
- **Matching the release in what was not ported.** Its frame time and feel were not measured by playing it, and a comment saying "as the release does" names one behaviour.
- **Changing the classic camera.** Every switch acts only while the Auto camera is on.

What could reopen it: the full obstruction search fighting a held angle every frame, as an earlier always-on snap did. `cam_obstruct` eases its answer as an extra on top of the player's pitch for that reason.

[^pr-feel]: Pull request 701 on origin, the four settings and their before and after.
[^pr-ease]: Pull request 702 on origin, `cam_ease_ms` and `cam_target_ms`.
[^cfg]: [SOURCES/FOLLOWCAM_CFG.H](../../../SOURCES/FOLLOWCAM_CFG.H), the comments above `FOLLOW_CAM_EASE_MS_DEFAULT` and `FOLLOW_CAM_TARGET_MS_DEFAULT`.
[^internals]: [docs/CAMERA_INTERNALS.md](../../CAMERA_INTERNALS.md), "Interaction with the Auto camera" and "Passing a latching shot over".
