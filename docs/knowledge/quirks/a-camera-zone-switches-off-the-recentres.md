---
type: Quirk
title: A camera zone also switches off the recentres
description: On a frame a camera zone has claimed the view, the original engine leaves out the recentre it makes when the hero is drawn fully hidden, the off-screen recentre, CameraCenter's obstruction search, Enter's recentre, the camera keys and the object obstruction test, so an authored shot is taken as deliberate even where it hides the hero; anything that suppresses or bypasses a zone inherits all of these, not only the angle.
status: draft
scope: "exterior and interior scenes, on the frame a camera zone claims the view; for a forced zone that is every frame inside its box"
equivalence: untested
asm_origin: "SOURCES/OBJECT.CPP:AffOneObject"
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
owner: /subsystems/camera.md
relates_to:
  - /quirks/a-camera-zone-fires-once-or-every-frame.md
sources:
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the hidden-hero recentre in AffOneObject
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, Enter's recentre and the camera keys in MainLoop
  - id: extfunc
    resource: ../../../SOURCES/EXTFUNC.CPP
    title: EXTFUNC.CPP, the object obstruction test
  - id: intext
    resource: ../../../SOURCES/INTEXT.CPP
    title: INTEXT.CPP, CameraCenter's test before the search
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, the same four tests of CameraZone
  - id: internals
    resource: ../../CAMERA_INTERNALS.md
    title: Camera internals (docs/CAMERA_INTERNALS.md), "What a camera zone also switches off"
---

# Scope

The frames on which `CameraZone` is true: the frame a latching zone fires, and every frame the hero stands in a forced zone or an interior camera zone ([the two shapes](/quirks/a-camera-zone-fires-once-or-every-frame.md)).

# Evidence

`untested`, read against the initial import: each test below is in the 1997 source, where the off-screen recentre shares one test with the camera keys; at `as_of` it has a test of its own, so there are six.[^original] No fixture pins the gating itself; the Auto camera's fixtures meet it only through the behaviours that depend on it.

# Behaviour

`CameraZone` is tested in six places at `as_of`, and a frame a zone claims takes none of them:

| Where | What is left out |
|---|---|
| `AffOneObject`, when the followed object is drawn fully hidden | the recentre behind the hero, `CameraCenter(1)`[^object] |
| `MainLoop`, Enter (`I_RETURN`) | Center camera, also left out during a cutscene[^perso] |
| `MainLoop`, the camera keys | `GereExtKeys` (the tilt keys) and the quarter-turn key (`I_CAMERA`)[^perso] |
| `MainLoop`, the off-screen block | the recentre when the hero leaves the frame, with its restore of the preset pitch under `FlagCameraForcee`[^perso] |
| `CameraCenter` | the obstruction search, `SearchCameraPos`[^intext] |
| `TestCameraPosObjets` | the test of scripted objects between the camera and the hero[^extfunc] |

At `as_of` the hidden-hero recentre also waits for a blend into or out of a shot and for `cam_obstruct`'s reply, and the Auto camera's update runs only when `CameraZone` is clear.[^object][^internals]

# Why it is load bearing

- **An authored shot that hides the hero is deliberate.** Without the test, a shot framed past a wall would be recentred away on its first frame. A change that suppresses a zone, as `cam_zone_skip` does for a steering player, loses the exemption with the angle: the recentre fires where the shot used to cover the spot.
- **It is why the Auto camera does nothing on the frame a shot fires.** The update sits inside the same test, so the zone's writes reach the screen untouched on that frame, and a later behaviour that needs to know a shot fired has to be told (`FollowCamShotTookView`), since the update cannot see it.
- **Center camera does nothing under a shot.** A player pressing Enter in a forced zone gets no response; that is the original's, not a dropped key.

# What it does not tell you

- **That the hero is outside every shot when `CameraZone` is clear.** For a latching zone it is clear on every frame after the first, with the hero still inside the box.
- **That a zone keeps the view while it is set.** It says a zone claimed this frame; under the Auto camera a latching shot's leftovers are the camera's to handle, through `cam_zone_return`.

[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `AffOneObject`, the test after `DrawRecover` and the comment above it.
[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the `I_RETURN` test, the block that calls `GereExtKeys` and the off-screen block that tests `FlagCameraForcee`.
[^extfunc]: [SOURCES/EXTFUNC.CPP](../../../SOURCES/EXTFUNC.CPP), `TestCameraPosObjets`.
[^intext]: [SOURCES/INTEXT.CPP](../../../SOURCES/INTEXT.CPP), `CameraCenter`, the test before `SearchCameraPos`.
[^original]: Commit 333929ab, the initial import, [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP) `AffOneObject`, and the same tests in [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP) `MainLoop` and [SOURCES/EXTFUNC.CPP](../../../SOURCES/EXTFUNC.CPP) `TestCameraPosObjets`.
[^internals]: [docs/CAMERA_INTERNALS.md](../../CAMERA_INTERNALS.md), "What a camera zone also switches off" and "Blending into and out of a shot".
