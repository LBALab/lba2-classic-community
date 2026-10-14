---
type: Decision
title: A blend into or out of a shot starts at a named moment, and is drawn, not simulated
description: The Auto camera blends into and out of a scripted camera shot from two named moments, the zone about to write its shot and the first Auto camera update after one, instead of blending whatever large change it sees at the point the camera is committed, and the blend changes only the view drawn, so the camera's state is what it would be with the setting at 0.
status: draft
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
owner: /subsystems/camera.md
sources:
  - id: pr-blend
    resource: https://github.com/LBALab/lba2-classic-community/pull/708
    title: The blend into and out of a scripted shot
  - id: pr-sign
    resource: https://github.com/LBALab/lba2-classic-community/pull/709
    title: A sign's shot shown at once
  - id: pr-prototype
    resource: https://github.com/LBALab/lba2-classic-community/pull/350
    title: The camera-handoff blend prototype, closed
  - id: followcam
    resource: ../../../SOURCES/FOLLOWCAM.CPP
    title: FOLLOWCAM.CPP, the blend's start, its cuts and the view it draws
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, where a shot is applied and where a sign applies one
  - id: blend-test
    resource: ../../../tests/automation/test_camzone_blend.sh
    title: The blend fixture
---

# Context

A scripted shot writes the whole camera in one frame, and the Auto camera takes it back the same way. On the covered bridge in cube 97 that is 517 units of yaw, 89 of pitch, 250 of distance and 1,700 of look-at point in one frame on the way in, and 2,103 of look-at point on the way out.[^pr-blend] An earlier prototype blended any change above a threshold at the point the camera is committed before the render, with scene changes and cutscenes excepted by name and an eight-frame settle after a scene change.[^pr-prototype] It was closed unmerged as replaced by the camera work merged since, which left two cuts: entering a shot that holds the view, and the look-at point and distance on leaving one.

# Decision

- **Named moments.** A blend starts where a shot is about to take the view (`FollowCamShotTookView`, called from `SetZoneCamera`) and on the first Auto camera update after a shot. Nothing is inferred from the size of a change.[^followcam][^object]
- **Drawn, not simulated.** `FollowCamSetView` sets the camera for the render from the blend while one runs; the globals stay as they would be at 0. With `cam_shot_blend_ms 0`, eleven screenshots were byte-identical to main and the camera trace equal over eight scenarios.[^pr-blend]
- **Never across a cut.** A blend starts only from a view the Auto camera showed of the same scene with nothing else having had the view since, and every cut ends it at once.[^followcam]
- **What reads the frame waits.** The hidden-hero recentre reads what was drawn, so a view on its way between two others could hide the hero for a reason nobody chose. It and `cam_obstruct`'s reply are held off until the blend ends, and the fixture compares the camera's own trace fields against a run at 0.[^blend-test]

A blend toward a view that cannot keep moving is wrong, and the first exception arrived after merge: a sign that names a shot draws one frame and holds it under the message, so a blend in showed its first instant, the old view, for the whole message. That shot is applied whole (`FollowCamShotAtOnce`, after `SetZoneCamera` in `GereZoneMessage`), and closing the message still blends back.[^pr-sign][^object]

# Non-goals

- **Smoothing Center camera, a cutscene or a scene change.** They are cuts on purpose.
- **Gliding out into a sign's shot.** It would delay every sign's text by the blend; left for the maintainer to decide.
- **Changing what a shot does to the camera's state.** That is `cam_zone_return`'s, and the blend is measured to leave it alone.

[^pr-blend]: Pull request 708 on origin, the before and after on the covered bridge and the byte-identical check at 0.
[^pr-sign]: Pull request 709 on origin.
[^pr-prototype]: Pull request 350 on origin, closed unmerged.
[^followcam]: [SOURCES/FOLLOWCAM.CPP](../../../SOURCES/FOLLOWCAM.CPP), `FollowCamShotTookView`, `FollowCamSetView`, `FollowCamShotAtOnce`, `FollowCamSyncTarget` and `FollowCamForgetLoan`.
[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `SetZoneCamera` and `GereZoneMessage`.
[^blend-test]: [tests/automation/test_camzone_blend.sh](../../../tests/automation/test_camzone_blend.sh), the comparison against the run at 0.
