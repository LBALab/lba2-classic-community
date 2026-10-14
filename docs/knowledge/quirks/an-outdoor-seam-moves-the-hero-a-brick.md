---
type: Quirk
title: Walking across an outdoor scene seam moves the hero a brick in one frame
description: Outdoors the hero is held half a brick inside every scene edge and flagged as out, and the change-of-scene zone at the edge places him half a brick inside the next scene, so a walked crossing covers 512 world units, one brick, in one frame, and the camera's look-at point jumps with him; no position exists for the brick between the two margins, so a seamless crossing has to be designed, not restored.
status: draft
scope: "a walked crossing between two exterior scenes through an edge cube zone; a hero being knocked back is held further in and not flagged, and the buggy is not clamped here"
equivalence: untested
asm_origin: "SOURCES/EXTFUNC.CPP:DoAnimExt"
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
owner: /subsystems/transitions.md
relates_to:
  - /subsystems/camera.md
sources:
  - id: extfunc
    resource: ../../../SOURCES/EXTFUNC.CPP
    title: EXTFUNC.CPP, the edge clamp in DoAnimExt
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, GereZoneChangeCube's placement at an edge zone
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, the same clamp and placement
  - id: issue
    resource: https://github.com/LBALab/lba2-classic-community/issues/707
    title: Walking across an outdoor scene boundary moves Twinsen one brick forward in one frame
  - id: cfg
    resource: ../../../SOURCES/FOLLOWCAM_CFG.H
    title: FOLLOWCAM_CFG.H, the look-at point's jump threshold
---

# Scope

A walked crossing between two exterior scenes, through a change-of-scene zone on the edge of the scene (`info0` or `info2` at 512 or `32768 - 1024`). A scripted or console scene change places the hero where it says and is not this. The phantom-scene path, taken where there is no zone to change scene, uses the same clamp.[^extfunc][^object]

# Evidence

`untested`, read against the initial import: the clamp and the placement are in the 1997 source in the same form.[^original] Seen in play on island 6, walking east from cube 97 into cube 99; the camera trace shows the look-at point moving 516 units at both crossings, against 25 to 30 a step walking. Not re-measured on a build without the Auto camera.[^issue]

# Behaviour

`DoAnimExt` holds the hero `DEMI_BRICK_XZ` (256) inside every edge of an exterior scene and sets `FlagHeroOutX` or `FlagHeroOutZ` when he pushes past it. A hero in a knock-back animation is held at 256 + 10 and not flagged, any other object is held at 256 and not flagged, and the buggy, while present, is not clamped by this block at all. `GereZoneChangeCube` waits for that flag on an edge zone and places him `DEMI_BRICK_XZ` inside the next scene. In continuous world coordinates that is from 32,512 to 32,768 + 256: 512 units, in one step.[^extfunc][^object]

# Why it is load bearing

- **There is no position for the brick between the two margins.** The hero cannot stand in the last half brick of one scene or the first half brick of the next, so closing the gap means placing him somewhere the original never put him, with collision, zones and scripts that were never run there. A seamless crossing is therefore a design to make, not a fix to restore; the constraints it has to respect are listed in the issue.[^issue]
- **Anything that keeps a world position across updates sees a jump.** The Auto camera's look-at point trail treats a move larger than `FOLLOW_CAM_TARGET_JUMP` in one update as not walked and holds its trail across it, which is correct for the renumbering of the world at a seam but means the camera jumps a brick with the hero.[^cfg]
- **A recording diverges after a crossing changed by a fix**, since the hero's position on the next frame differs; any change here moves every recorded session that crosses a seam.

# What it does not tell you

- **That the camera causes it.** The jump is the hero's, and the classic camera shows it too; the Auto camera only makes it easier to see.
- **How the 2022 Classic release crosses.** Not checked; that is the first step the issue names.
- **That NPCs respawning at the same crossing is the same defect.** It happens at the same moment and is a separate issue (#538).

[^extfunc]: [SOURCES/EXTFUNC.CPP](../../../SOURCES/EXTFUNC.CPP), `DoAnimExt`, the `marge` clamp and the assignments to `FlagHeroOutX` and `FlagHeroOutZ`.
[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `GereZoneChangeCube`, the edge-zone cases that test `FlagHeroOutX` and `FlagHeroOutZ`.
[^original]: Commit 333929ab, the initial import, [SOURCES/EXTFUNC.CPP](../../../SOURCES/EXTFUNC.CPP) `DoAnimExt`, and [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP) `GereZoneChangeCube`.
[^issue]: Issue 707 on origin.
[^cfg]: [SOURCES/FOLLOWCAM_CFG.H](../../../SOURCES/FOLLOWCAM_CFG.H), `FOLLOW_CAM_TARGET_JUMP` and the comment above it.
