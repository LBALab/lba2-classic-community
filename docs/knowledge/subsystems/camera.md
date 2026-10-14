---
type: Subsystem
title: Camera
description: The exterior camera has two owners, a world yaw everything renders from and an offset from the hero's facing the Auto camera aims by, and every camera bug the port has met was a writer of one that left the other behind; the Auto camera is one opt-in follow camera to which each later behaviour is added as a switch whose off value is the camera from before it, frame for frame.
status: draft
subsystem: camera
as_of: 372f920f
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
relates_to:
  - /subsystems/transitions.md
  - /subsystems/input.md
  - /subsystems/movement.md
  - /subsystems/recording.md
sources:
  - id: internals
    resource: ../../CAMERA_INTERNALS.md
    title: Camera internals (docs/CAMERA_INTERNALS.md)
  - id: player
    resource: ../../CAMERA.md
    title: The player's camera page (docs/CAMERA.md)
  - id: followcam
    resource: ../../../SOURCES/FOLLOWCAM.CPP
    title: FOLLOWCAM.CPP, the Auto camera's update, its settings table and its view
  - id: math
    resource: ../../../SOURCES/FOLLOWCAM_MATH.H
    title: FOLLOWCAM_MATH.H, the angle rule, the eases and the blend weights
  - id: extfunc
    resource: ../../../SOURCES/EXTFUNC.CPP
    title: EXTFUNC.CPP, the adopt call and the exterior render
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the camera zone dispatch, the shot and the hidden-hero recentre
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, where the Auto camera's update runs in the main loop
  - id: settings
    resource: ../../../SOURCES/SETTINGS.H
    title: SETTINGS.H, the setting row and its value from before
  - id: testing
    resource: ../../TESTING.md
    title: Testing (docs/TESTING.md), section 8, the camera fixtures
---

Distilled from [docs/CAMERA_INTERNALS.md](../../CAMERA_INTERNALS.md), which stays the account of each mechanism, and from the pull requests that built the Auto camera's later behaviours (#701 to #709). The principles are structural; the settings and their defaults are read at `as_of`. What a player sees is in [docs/CAMERA.md](../../CAMERA.md), which this concept does not restate.

# Principles

- **Two owners, one rule between them.** `BetaCam` is the world yaw the frame is drawn from; `AddBetaCam` is an offset from the hero's facing. The Auto camera re-derives its target from the offset every update, so a writer of `BetaCam` that leaves `AddBetaCam` alone has created a difference the camera reads as a turn owed and spends. The hero turning under a held angle (#450), an interrupted orbit and a camera zone's authored angle (#518) were all this. `FollowCamAdoptAngle` re-solves the offset for the angle on screen and is called after every such write.[^internals][^extfunc]
- **One Auto camera, every later behaviour a switch.** The 2022 Classic release's camera behaviours came in as `cam_*` settings on the one Auto camera, not as a second mode, by [a decision](/decisions/one-auto-camera-each-behaviour-a-switch.md). Each switch has an off value that is the camera from before it, and [every row says what that value is](/decisions/a-camera-setting-names-its-value-from-before.md).[^followcam]
- **What the frame shows is not always what the camera is.** The blend into and out of a shot and the obstruction tilt are render-time: `FollowCamSetView` and `FollowCamRenderExtraPitch` change the view drawn and leave the globals as they would be without them. Render-only is not state-free, though: the hidden-hero recentre reads what was drawn, so a render change can still move the camera, and is held off while a blend runs.[^internals][^followcam]
- **A scripted shot is a loan.** A camera zone takes the view and, with `cam_zone_return`, gives the yaw and pitch back when the hero leaves its box; what the player set by hand meanwhile is theirs. Anything that decides where the camera points on purpose ends the loan without a return.[^internals]
- **Drop what latches, leave what eases.** At a cut (a scene change, a zone, a recentre), state that converges by itself is carried, since a stale value costs a glide and a reset costs a jump; state that does not converge (the target yaw, the re-engage and manual-hold counters, an orbit's follow-through) is reset, or it discharges against a situation it was never measured in.[^internals]

# Contracts

| Contract | Statement |
|---|---|
| Opt-in | `FollowCamera` (cfg) / `cam_follow` (console) is 0 by default. With it off every exterior frame is the classic camera's; the later switches act only while it is on.[^player] |
| Off is before | With every switch whose row says `SETTING_BEFORE(v)` set to `v` (the console's `settings_before cam_`), the camera's state matches the build without those behaviours frame for frame. Each behaviour's pull request measured this against main over several scenarios, and the fixtures start from it.[^settings][^testing] |
| Where the update runs | `UpdateFollowCameraExt` runs inside the fixed simulation step and only outside a camera zone's frame, a cutscene and an interior, so a per-update share is frame-rate independent with the step on, and a frame without the update is a gap the clock-paced eases treat as no time at all.[^perso][^followcam] |
| Cuts | Center camera, every `CameraCenter(1)` and `(2)`, the quarter-turn key, a scene change, a building, a cutscene and a load are cuts: `FollowCamSyncTarget` restarts the eases and ends a blend, and a blend never runs across one.[^followcam] |
| What ends a loan | Fewer things than end a blend. `FollowCamForgetLoan` runs at `CameraCenter(1)` and `(2)` (Center camera, a script's camera order, the hidden-hero recentre), a cutscene or the Auto camera switched off, and a load. A building and a carried scene change do not end it: what was owed going in is owed coming out.[^followcam][^internals] |
| Pitch ownership | The pitch the Auto camera gives back is its own (`FollowCamOwnPitch()`), not `AlphaCam` as it stands, because a camera zone and `CameraCenter`'s obstruction search both leave pitches in `AlphaCam` that nothing takes away.[^internals] |
| Recorded | Every camera setting is carried in a recording's header and borrowed for the replay's length, so a recording replays on the settings it was made with.[^settings] |

# Seams

- **`SetZoneCamera` and `CheckZoneSce`**, where a shot is applied, a latching shot is passed over and the loan is opened.[^object]
- **`CameraCenter`**, the original recentre, and the Auto camera's apply through its flag 3; [the transitions Subsystem](/subsystems/transitions.md) owns which flag a scene change takes.
- **`AffGrilleExt`**, which sets the camera again from the globals before it draws, which is [a Quirk](/quirks/affgrilleext-sets-the-camera-from-the-globals.md) every render-time adjustment has to go through.[^extfunc]
- **`ApplyManualCameraNudge`**, the one routine every analog source feeds; [input](/subsystems/input.md) owns how the right stick and the mouse reach it.
- **The settings table** `FollowCamSettings`, read by the cfg reader, the cfg writer, the console and the recorder alike.[^followcam]
- **`camtrace`**, emitted from the main loop so it reports every camera, not only the Auto camera's update.

# What it does not tell you

- **That the classic camera is being kept as it was by accident.** It is the reference: every Auto camera behaviour is measured against it or against the Auto camera with the switch off, and a change that moves the classic camera is a regression.
- **That a green CI covers the camera.** CI runs the arithmetic in `FOLLOWCAM_MATH.H` and a source check on the setting rows, and nothing else; the behavioural fixtures need retail data and a display and run locally only. Reasons and habits are in [the camera tests Reference](/references/camera-tests.md).[^testing][^math]
- **That a measurement at one resolution holds at another.** The HD recompose is a no-op at 480 lines, so a change measured there has not met the correction, and one measured only at 1080 has not met the original framing.
- **Where a zone's index points.** The GOG and US disc data differ in the flags of three camera zones, two in cube 42; a zone named by cube and index has to be checked against the data being played.
- **What the 2022 Classic release does in a case nobody ported.** Its behaviours were read from the executable and emulated, not played side by side; what came in is what was ported, and "as retail does" in a comment names one behaviour, not the release.

[^internals]: [docs/CAMERA_INTERNALS.md](../../CAMERA_INTERNALS.md), "AddBetaCam, and why the camera has two owners", "Blending into and out of a shot", "A shot as a loan" and "Rules for changing the camera".
[^player]: [docs/CAMERA.md](../../CAMERA.md), "Switching it on" and "Settings".
[^followcam]: [SOURCES/FOLLOWCAM.CPP](../../../SOURCES/FOLLOWCAM.CPP), `UpdateFollowCameraExt`, `FollowCamSetView`, `FollowCamRenderExtraPitch`, `FollowCamSyncTarget`, `FollowCamForgetLoan` and the comment above `FollowCamSettings`.
[^math]: [SOURCES/FOLLOWCAM_MATH.H](../../../SOURCES/FOLLOWCAM_MATH.H), `FollowCamTargetBetaFor`, `FollowCamEaseStepMs` and `FollowCamBlendWeight`.
[^extfunc]: [SOURCES/EXTFUNC.CPP](../../../SOURCES/EXTFUNC.CPP), `FollowCamAdoptAngle`, `ApplyManualCameraNudge` and `AffGrilleExt`.
[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `SetZoneCamera`, `CheckZoneSce` and `FollowCamLendToShot`.
[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the call to `UpdateFollowCameraExt` and `FollowCamNoteFrame`.
[^settings]: [SOURCES/SETTINGS.H](../../../SOURCES/SETTINGS.H), `SETTING_BEFORE`, `SETTING_BEFORE_NONE` and `SETTING_RECORDED`.
[^testing]: [docs/TESTING.md](../../TESTING.md), "8. Camera fixtures".
