---
type: Quirk
title: AffGrilleExt sets the camera again from the globals before it draws
description: The exterior render begins by setting the camera from VueOffset, AlphaCam, BetaCam, GammaCam and VueDistance, so a change made to the camera itself after the follow update, and not to those globals, is gone by the time the frame is drawn; the port's render-time adjustments therefore go through the view the render sets, and two that did not never reached the screen.
status: draft
scope: "exterior scenes, every frame that draws the terrain"
equivalence: untested
asm_origin: "SOURCES/EXTFUNC.CPP:AffGrilleExt"
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
owner: /subsystems/camera.md
sources:
  - id: extfunc
    resource: ../../../SOURCES/EXTFUNC.CPP
    title: EXTFUNC.CPP, AffGrilleExt
  - id: followcam
    resource: ../../../SOURCES/FOLLOWCAM.CPP
    title: FOLLOWCAM.CPP, the view the render sets and the render-time pitch
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, SOURCES/EXTFUNC.CPP, AffGrilleExt opening with SetFollowCamera
  - id: internals
    resource: ../../CAMERA_INTERNALS.md
    title: Camera internals (docs/CAMERA_INTERNALS.md), "Obstruction clearance" and "Open work"
  - id: measured
    resource: https://github.com/LBALab/lba2-classic-community/blob/372f920f/docs/CAMERA.md
    title: docs/CAMERA.md before the split, "Obstruction clearance", the frames compared at three pitch gains
---

# Scope

Every exterior frame that draws the terrain. Interiors set their camera from the tile grid and are not concerned.

# Evidence

`untested`, read against the initial import, where `AffGrilleExt` opens with `SetFollowCamera` from the globals.[^original] The consequence was measured: at 1080 lines a frame rendered with `cam_hd_pitch` at 0, 180 or 400 is byte for byte the same, while `cam_hd_dist` and `cam_hd` change it.[^measured]

# Behaviour

`AffGrilleExt` sets the camera before anything is drawn, from `VueOffsetX`/`Y`/`Z`, `AlphaCam`, `BetaCam`, `GammaCam` and `VueDistance`. In the original that is `SetFollowCamera`; at `as_of` it is `FollowCamSetView`, which sets the same view from the same globals, or from the blend while one runs, and adds the obstruction extra to the pitch (`FollowCamRenderExtraPitch`).[^extfunc][^followcam]

# Why it is load bearing

- **A camera change has to be in the globals, or in the view the render sets.** The Auto camera's update ends with `CameraCenter(3)`, which positions the camera; anything applied to the camera around that call and put back afterwards is overwritten here. The HD recompose's pitch steepen is applied that way and never reaches the frame; the ground clearance's lift, a `SetPosCamera` around the same call, is overwritten by the same mechanism (read, not measured).[^internals]
- **That is why the blend and the obstruction tilt are written where they are.** Both are drawn, not simulated, and both enter through the view the render sets rather than through `AlphaCam` or the camera position.[^followcam]
- **Making the two dead adjustments live changes what every player sees at a tall resolution**, so it is a decision of its own, not a fix to slip into another change.

# What it does not tell you

- **That `CameraCenter(3)` has no effect.** It still sets the camera for what runs between it and the render, the clearance's sampling among them; the render discards it only for drawing.
- **That every frame passes here.** A frame drawn in the objects-only flip mode, over the background kept from the last full redraw, does not draw the terrain (read, not measured).

[^extfunc]: [SOURCES/EXTFUNC.CPP](../../../SOURCES/EXTFUNC.CPP), `AffGrilleExt` and its call to `FollowCamSetView`.
[^followcam]: [SOURCES/FOLLOWCAM.CPP](../../../SOURCES/FOLLOWCAM.CPP), `FollowCamSetView` and `FollowCamRenderExtraPitch`.
[^original]: Commit 333929ab, the initial import, [SOURCES/EXTFUNC.CPP](../../../SOURCES/EXTFUNC.CPP), the first statement of `AffGrilleExt`.
[^internals]: [docs/CAMERA_INTERNALS.md](../../CAMERA_INTERNALS.md), "HD recompose", "Obstruction clearance" and "Open work".
[^measured]: docs/CAMERA.md at 372f920f, "Obstruction clearance", the paragraph on `FollowCamRenderExtraPitch`.
