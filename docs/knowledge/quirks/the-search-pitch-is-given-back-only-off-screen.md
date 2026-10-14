---
type: Quirk
title: The obstruction search's pitch is given back only by the off-screen recentre
description: When CameraCenter's obstruction search finds something between the camera and the hero it writes a steeper pitch into AlphaCam and raises FlagCameraForcee, and the only restore nobody asks for is the recentre the original makes when the hero walks off screen; Center camera and the quarter-turn key reset the pitch to the preset but search again, so a camera that never takes the off-screen recentre, as the Auto camera does not, keeps the search's pitch until the player recentres, unless it handles it itself.
status: draft
scope: "exterior scenes; a CameraCenter with flag 0, 1 or 2 outside a camera zone"
equivalence: untested
asm_origin: "SOURCES/PERSO.CPP:MainLoop"
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
owner: /subsystems/camera.md
relates_to:
  - /quirks/a-camera-zone-switches-off-the-recentres.md
sources:
  - id: intext
    resource: ../../../SOURCES/INTEXT.CPP
    title: INTEXT.CPP, CameraCenter and its call to the search
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the off-screen recentre and its restore of the presets
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, the same restore under FlagCameraForcee
  - id: pr-feel
    resource: https://github.com/LBALab/lba2-classic-community/pull/701
    title: The Auto camera around buildings, scripted shots and scenery, cam_obstruct
  - id: internals
    resource: ../../CAMERA_INTERNALS.md
    title: Camera internals (docs/CAMERA_INTERNALS.md), "Obstruction clearance"
---

# Scope

An exterior `CameraCenter` with a flag other than 3, outside a camera zone, that finds terrain, a scenery box or a scripted object in the way. A camera zone raises the same flag for its own authored pitch.[^intext]

# Evidence

`untested`, read against the initial import: the search, the flag it raises and the restore in the off-screen block are in the 1997 source in the same form.[^original] Under the Auto camera the consequence was measured when `cam_obstruct` was built: out of the Tavern's door (cube 3 to cube 42) the pitch stayed at 767, above the 600 the player's own tilt stops at, wherever he walked.[^pr-feel]

# Behaviour

`CameraCenter` calls `SearchCameraPos` when its flag is not 3 and no zone holds the view. On a hit it places the camera with `SetPosCamera` and `SetTargetCamera`, leaving the steeper pitch it needed in `AlphaCam`, and raises `FlagCameraForcee`.[^intext] Center camera and the classic quarter-turn key (`CameraCenter(1)` and `(2)`) put `AlphaCam` back to the preset, but run the search again and leave `FlagCameraForcee` as it was. The only restore nobody asks for is the off-screen block of `MainLoop`: when the hero projects outside the screen margins it moves the render anchor to him and, if `FlagCameraForcee` is set, clears it and puts `AlphaCam`, `GammaCam` and `VueDistance` back to the `VueCamera` presets before `CameraCenter(0)`.[^perso]

# Why it is load bearing

- **The classic camera relies on walking off screen to look normal again.** A door's arrival, a scripted recentre and the hidden-hero recentre all run the search, and each steep pitch they leave is undone only when the hero next leaves the frame.
- **The Auto camera never takes that recentre**, since it keeps the hero framed, so without its own answer it kept the search's pitch, as though the player had set it, until the player pressed Center camera. `cam_obstruct` runs the search itself every update, keeps the answer out of `AlphaCam` as an eased extra, and has `CameraCenter`'s own search stand down while it is on.[^internals]
- **The pitch the Auto camera gives back after a shot is its own**, not `AlphaCam` as it stands, for the same reason: a door's recentre runs the search on the arrival frame just before the zone over the door, so a loan taken against `AlphaCam` handed the search's pitch back.[^internals]

# What it does not tell you

- **Whose pitch `AlphaCam` holds.** It can be the player's tilt, a preset, a zone's authored pitch or the search's answer, and nothing in the variable says which; `FlagCameraForcee` says only that something other than the player set it.
- **That the search sees everything.** It samples boxes every 512 units along the boom, so scenery the boxes miss can still hide the hero.

[^intext]: [SOURCES/INTEXT.CPP](../../../SOURCES/INTEXT.CPP), `CameraCenter`, the call to `SearchCameraPos` and the assignment to `FlagCameraForcee`.
[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the off-screen block and its `FlagCameraForcee` test.
[^original]: Commit 333929ab, the initial import, [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), the off-screen block, and [SOURCES/INTEXT.CPP](../../../SOURCES/INTEXT.CPP) `CameraCenter`.
[^pr-feel]: Pull request 701 on origin, the `cam_obstruct` row.
[^internals]: [docs/CAMERA_INTERNALS.md](../../CAMERA_INTERNALS.md), "Obstruction clearance" and "A shot as a loan".
