# Zones

A zone is an axis-aligned box in a scene that does something when an actor is inside
it: change cube, take the camera, hand over a bonus, drive a conveyor. Zones are the
scene's trigger layer, and almost all of a scene's interactive geometry is expressed
through them.

Their payload lives in eight opaque `Info` slots whose meaning depends entirely on
the zone's type. Nothing in the code names them, so this is the map.

## Where they come from

Zones are part of the scene file, read by `LoadScene` (`SOURCES/DISKFUNC.CPP`) into
`ListZone` / `NbZones`. They point straight into the loaded scene buffer rather than
being copied, so `Info` writes are runtime state that lasts until the cube is
reloaded, and several types use that deliberately (see `ZONE_ACTIVE` and the
already-taken and cooldown slots below).

`T_ZONE` (`SOURCES/COMMON.H`) is the box `(X0,Y0,Z0)-(X1,Y1,Z1)`, then `Info0`
through `Info7`, then `Type` and `Num`. `Info4` through `Info6` were added late in
the original development (a comment dates them to 1995-12-08), which is why the
older types stop at `Info3` and only the camera zone uses the full set.

## Info7 is the flag word

`Info7` is not a payload slot. It is the flag word that decides whether a zone does
anything at all, and for a camera zone how often.

| Flag | Value | Meaning |
| --- | --- | --- |
| `ZONE_INIT_ON` | 1 | Enabled at scene load |
| `ZONE_ON` | 2 | Currently enabled. A zone without this is inert |
| `ZONE_ACTIVE` | 4 | Has already fired. Set by the camera zone on entry, cleared on exit |
| `ZONE_OBLIGATOIRE` | 8 | Camera zone: re-aim every frame the hero is inside, instead of once |

Roughly half the camera zones in the shipped data are not enabled: of 440 across the
74 exterior scenes, 241 carry `ZONE_ON`.

## The types

Dispatch is a `switch (ptrz->Type)` in `GereZones` (`SOURCES/OBJECT.CPP`). Slots not
listed are unused by that type.

| Type | Name | Info slots |
| --- | --- | --- |
| 0 | cube | `Num` = target cube. `Info0/1/2` = arrival X/Y/Z (Y is relative to `Y0`). `Info5 & ZONE_TEST_BRICK` = require a door collision rather than firing on entry. `Info6 & ZONE_DONT_REAJUST_POS_TWINSEN` = skip the arrival reposition |
| 1 | camera | The whole authored shot. `Info0/1/2` = eye position in cube coordinates, `Info3` = `AlphaCam`, `Info4` = `BetaCam`, `Info5` = `GammaCam`, `Info6` = `VueDistance` |
| 2 | scenaric | `Num` is written to the hero's `ZoneSce`, where a Life script reads it via `LF_ZONE_OBJ`. Carries no other payload: the behaviour is entirely in the script |
| 3 | grid | No runtime handler found in the zone dispatch. Present in the type table and in editor paths only |
| 4 | giver | `Info0` = bonus kind (through `WhichBonus`), `Info1` = how many, `Info2` = already-taken latch. Fires on the action button while inside, and spawns the bonus at the box's centre and `Y1` (`ZoneGiveExtraBonus`, `SOURCES/EXTRA.CPP`) |
| 5 | message | `Info2` selects which edge of the box the hero must be facing for the message to fire (1 north, 2 south, and so on), tested against the angle to the box corners (`GereZoneMessage`) |
| 6 | ladder | `Info1` = enabled. `Y1` is the top the climb must not pass |
| 7 | escalator | `Info1` = enabled, `Info2` = direction (1 north, 2 south, 4 east, 8 west) |
| 8 | hit | `Info1` = damage, `Info2` = cooldown in fifths of a second, `Info3` = the running cooldown deadline (runtime state, 0 when ready) |
| 9 | rail | Latches the zone onto the actor as `PtrZoneRail`, and only for `MOVE_WAGON` |

## Camera zones in detail

`SetZoneCamera` (`SOURCES/OBJECT.CPP`) applies the shot. Outdoors it writes
`AlphaCam`, `BetaCam`, `GammaCam` and `VueDistance` from `Info3` through `Info6`,
then `SaveCamera()`, then `CameraCenter(0)`. The `SaveCamera` call is what stops the
zone re-applying every frame: the guard above it compares the live camera against
the saved one, so an unchanged camera means no work.

The authored values **latch**. No camera-zone exit path calls `RestoreCamera()`, so
once a zone has taken the view it keeps it until something else re-aims the camera.
That is what the classic camera shows, and it is deliberate.

`CameraZone` is not a latch, though. It is cleared every frame in the main loop
(`SOURCES/PERSO.CPP`) before zone detection runs, so it means "a camera zone claimed
the view this frame", not "the hero is under a shot".

The Auto camera (`FollowCamera`, a community addition) needs telling when a zone
takes over, because it derives its own angle from the hero's facing and would
otherwise read the authored angle as catch-up owed and lerp out of the shot.
`FollowCamAdoptAngle()` does that for the angle. There is **no equivalent for the
distance**: the Auto camera rewrites `VueDistance` from its own spring arm on every
update, so an authored boom survives exactly one frame before being replaced. The
authored pitch is kept, the authored angle is kept, the authored distance is not.

What the shipped data asks for, across the 241 enabled exterior camera zones:

| `Info6` (authored `VueDistance`) | |
| --- | --- |
| min | 2400 |
| median | 11100 |
| max | 46700 |
| within the Auto camera's zoom range (7000 to 20000) | 218 (90%) |
| below it | 2 |
| above it | 21 |

For comparison the Auto camera's own default boom is 13750, and the classic presets
(`DefVueDistance`) are 10500 and 17000. Authored pitches (`Info3`) span 5 to 4095,
well outside the Auto camera's own 150 to 600 band, and are applied unclamped.

## Inspecting zones

`zonelist [type]` (console) prints every zone in the loaded cube with its box, all
seven `Info` slots, the decoded flags, and a marker on whichever box the hero is
standing in. The optional argument filters to one type.

Because it prints boxes in world coordinates it turns "walk over there and stand in
the right spot" into a concrete `teleport <x> <y> <z>`, which is how a headless run
reaches a zone-gated interaction without walking. Note that `teleport` is often
refused on exterior islands, where the hero is snapped back to valid ground.

## Key files

- `SOURCES/COMMON.H` -- `T_ZONE`, the `ZONE_*` flags.
- `SOURCES/DISKFUNC.CPP` -- `LoadScene`, where zones are read.
- `SOURCES/OBJECT.CPP` -- the type dispatch, `SetZoneCamera`, `GereZoneChangeCube`,
  `GereZoneMessage`.
- `SOURCES/EXTRA.CPP` -- `ZoneGiveExtraBonus` (type 4).
- `SOURCES/CONSOLE/CONSOLE_CMD.CPP` -- `zonelist`.

## Related

- `docs/CAMERA.md` -- the camera system the type 1 zones drive.
- `docs/SCENES.md` -- the scenes zones belong to, and the exterior island grid.
- `docs/TRANSITIONS.md` -- what a type 0 zone kicks off.
- `docs/CONSOLE.md` -- the rest of the console commands.
