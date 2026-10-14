---
type: Quirk
title: A camera zone fires once or every frame, by one flag
description: The camera zone dispatch applies a zone's shot on every frame the hero is inside when the zone carries ZONE_OBLIGATOIRE or the scene is an interior, and otherwise once per entry, latching ZONE_ACTIVE until he leaves the box, and only while the movie cameras option is on; the two shapes are about equally common outdoors, so "the hero is under a shot" and "a zone claimed the view this frame" are different questions.
status: draft
scope: "every scene; the once-per-entry shape needs AllCameras on, and interiors always take the every-frame shape"
equivalence: tested
asm_origin: "SOURCES/OBJECT.CPP:CheckZoneSce"
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
owner: /subsystems/camera.md
verified_against: /references/camera-tests.md
sources:
  - id: object
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the camera case of the zone dispatch and SetZoneCamera
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, where CameraZone is cleared each frame
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, SOURCES/OBJECT.CPP, the camera case of the zone dispatch
  - id: model
    resource: ../../../tests/automation/test_camzone_model.sh
    title: The fixture that pins both shapes
  - id: zones
    resource: ../../ZONES.md
    title: Zones (docs/ZONES.md), the Info7 flag word
  - id: census
    resource: https://github.com/LBALab/lba2-classic-community/blob/372f920f/docs/CAMERA.md
    title: docs/CAMERA.md before the split, "Two shapes, decided by Info7", the zone census on both data sets
---

# Scope

Every scene with camera zones. A zone fires only for the followed object (`NumObjFollow`) and only with `ZONE_ON`; the first enabled camera zone the hero stands in shadows the ones after it in the list, whether it fired or not.[^object]

# Evidence

`tested`: `test_camzone_model.sh` puts the hero in and out of a zone of each shape and asserts both behaviours.[^model] The dispatch was also read against the initial import, where the condition is the same three-way test.[^original]

# Behaviour

The camera case of the dispatch applies the shot (`SetZoneCamera`) and sets `ZONE_ACTIVE` when any of these holds: the zone carries `ZONE_OBLIGATOIRE`; the scene is an interior; or `ZONE_ACTIVE` is clear and `AllCameras` is on. `ZONE_ACTIVE` is cleared when the hero is outside the box, so walking back in re-arms the zone.[^object][^original] `CameraZone`, which `SetZoneCamera` raises, is cleared every frame in the main loop before zones are checked, so it means "a zone claimed the view this frame".[^perso]

Of 242 enabled exterior camera zones in the GOG data, 128 latch and 114 are forced, with a forced zone in 36 cubes; the US disc gives 240, 128 and 112, and the two sets differ in the flags of three zones, two of them in cube 42.[^census] The flag word is in [docs/ZONES.md](../../ZONES.md).[^zones]

# Why it is load bearing

- **A latching zone's shot lasts one frame on screen under the classic camera only because nothing else moves the camera.** The classic camera keeps the angle it was given, so one frame of `SetZoneCamera` reads as a held shot; any camera that re-aims every frame, as the Auto camera does, shows a latching shot for one frame and then its own view. That is why the Auto camera needed `cam_zone_skip` and `cam_zone_return` at all.
- **Interiors depend on the every-frame shape.** Interior cameras are camera zones that re-apply every frame; treating every zone as once per entry breaks every interior.
- **The movie cameras option only turns off the latching shape.** Forced zones and interiors fire with it off, which is what keeps scripted places framed for a player who switched the option off.

# What it does not tell you

- **Whether the hero is inside a shot's box.** `CameraZone` is true on the frame a shot is applied, which for a latching zone is one frame per entry. The port tracks the box separately (`FollowCamInShotBox`, `inshot` in `camtrace`).[^object]
- **That a zone named by cube and index is the same zone on another release.** The flags differ in three zones between the two data sets.
- **That "latching" means common.** The two shapes are close to even outdoors; a fixture comment that calls the latching one "by far the more common" overstates it.[^model]

[^object]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), `CheckZoneSce`, its camera case, and `SetZoneCamera`.
[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`, the assignment that clears `CameraZone`.
[^original]: Commit 333929ab, the initial import, [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), the camera case (`case 1`) of the zone dispatch.
[^model]: [tests/automation/test_camzone_model.sh](../../../tests/automation/test_camzone_model.sh), the header comment.
[^zones]: [docs/ZONES.md](../../ZONES.md), "Info7 is the flag word".
[^census]: docs/CAMERA.md at 372f920f, "Two shapes, decided by Info7": every exterior cube's zones listed with `zonelist` as each scene loads them, from the corpus save `Wannies fragment.LBA`, on the GOG and the US disc data.
