# Camera system

The camera behaves differently in interior (isometric) and exterior (perspective) scenes. This document covers both paths, the `CameraCenter` function that ties them together, and Auto camera — community third-person follow behavior in exterior scenes (config key `FollowCamera`, not part of the original game). The name matches common usage (e.g. Enhanced Edition comparisons); the code and cfg key stay `FollowCamera` for clarity in the source.

## Low-level camera ([LIB386/3D/CAMERA.CPP](../LIB386/3D/CAMERA.CPP))


| Function          | Role                                                                                  |
| ----------------- | ------------------------------------------------------------------------------------- |
| `SetPosCamera`    | Sets `CameraX`/`Y`/`Z` and applies rotation matrix via `SetCamera`                    |
| `SetAngleCamera`  | Sets `CameraAlpha`/`Beta`/`Gamma`, rebuilds `MatriceWorld`, recomputes light          |
| `SetFollowCamera` | Sets target, angle, and zoom; inverse-rotates to compute actual camera world position |
| `SetTargetCamera` | Computes angles to look at a target point from the current camera position            |


All functions live behind `extern "C"` and are shared by both interior and exterior code.

## Interior (isometric)

- Projection: `SetIsoProjection` / `LongProjectPoint` — no perspective divide, fixed camera orientation.
- `PtrProjectPoint` calls `LongProjectPoint` directly ([SOURCES/INTEXT.CPP](../SOURCES/INTEXT.CPP) line 313).
- Camera position is `SetPosCamera(StartXCube * SIZE_BRICK_XZ, ...)` — tile-grid aligned.
- **Smooth follow:** When the hero goes off-screen, the main loop interpolates `StartXCube` / `StartZCube` toward the hero's brick with `xm + (xm - StartXCube) / 2` (`MainLoop` in [SOURCES/PERSO.CPP](../SOURCES/PERSO.CPP)). This half-step convergence keeps scrolling gentle.
- Enter (`I_RETURN`) recenters the camera by snapping to the hero's tile position.

## Exterior (perspective)

- Projection: `SetProjection` / `LongWorldRotatePoint` + `LongProjectPoint` — full perspective with far clip, field of view (`ChampX`/`ChampZ`), and fog.
- `PtrProjectPoint` world-rotates then projects ([SOURCES/INTEXT.CPP](../SOURCES/INTEXT.CPP) lines 315–317).
- Camera set via `SetFollowCamera(VueOffsetX, VueOffsetY, VueOffsetZ, AlphaCam, BetaCam, GammaCam, VueDistance)`.
- Initialized by `Init3DExtView` ([SOURCES/EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP) line 89).

### View parameters


| Global           | Role                                          | Default                                      |
| ---------------- | --------------------------------------------- | -------------------------------------------- |
| `AlphaCam`       | Elevation angle                               | `DefAlphaCam[VueCamera]` (300 or 530)        |
| `BetaCam`        | Horizontal rotation                           | Computed from hero `Beta`                    |
| `GammaCam`       | Roll (usually 0)                              | `DEFAULT_GAMMA_CAM` (0)                      |
| `VueDistance`    | Zoom / distance                               | `DefVueDistance[VueCamera]` (10500 or 17000) |
| `VueOffsetX/Y/Z` | Camera target (world coords)                  | Derived from hero position + `Rotate`        |
| `AddBetaCam`     | Camera rotation as an offset from the hero's facing | 0; see below                           |


### AddBetaCam, and why the camera has two owners

`BetaCam` is a **world** angle: the view's yaw, which everything downstream renders from. `AddBetaCam` is not. It is an offset **from the hero's facing**, and the two are tied together by one rule:

```
BetaCam = (2048 - (AddBetaCam + heroBeta)) & 4095
```

The rule is what the camera is aimed *by*, not a property it always has. It holds the instant something aims the camera at the hero and for as long as the Auto camera is converged, and at no other time: the classic camera keeps whatever angle it was left with until a control re-aims it, and a loaded save restores an angle satisfying no such relationship. Read it as the thing the code maintains at particular moments rather than as an invariant to assert on a given frame.

In the original game `AddBetaCam` only ever holds quarter turns: `I_CAMERA` adds 1024, and the Life opcode `LM_CAMERA_CENTER` sets `num * 1024` for an authored orientation. The Auto camera reuses it for continuous player orbit, so it now holds any value.

That reuse is worth understanding before changing anything here, because it is the source of the camera's characteristic bug. The rule above is re-evaluated every frame the Auto camera runs, so **any code that writes `BetaCam` directly, without also updating `AddBetaCam`, has created a difference the Auto camera will read as catch-up owed and spend.** Three writers have done exactly that:

- the hero turning while the camera holds an angle, which made a stick touch snap the view three quarters of a turn (#450);
- an orbit interrupted by a camera zone, cutscene or the Auto camera being switched off, whose leftover state made the next touch skip its realign;
- a camera zone's authored angle, which the Auto camera unwound over the following second (#518).

The rule and its inverse live in [FOLLOWCAM_MATH.H](../SOURCES/FOLLOWCAM_MATH.H), along with the shortest-way-round difference and one frame of the rotation lerp, so the arithmetic has one home rather than a copy at each site that needs it. `FollowCamAdoptAngle()` ([EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP)) is the shared answer: it solves the rule above for the current `BetaCam`, so the target comes out where the camera already is and nothing is owed. Call it after writing `BetaCam` on a path the Auto camera might be running under. `FollowCamForgetManualGesture()` is its companion for the other direction: it drops an in-flight orbit when something else takes the camera.

### Off-screen recenter (original behavior)

When the hero projects outside the screen clip bounds (`MainLoop` in [PERSO.CPP](../SOURCES/PERSO.CPP)):

- **Exterior:** `StartXCube`/`StartZCube` snap to the hero's rotated position (no interpolation). Then `CameraCenter(0)` applies the new position without reorienting `BetaCam`.
- **Interior:** uses half-step interpolation (see above).

### Manual controls

- **Enter (`I_RETURN`):** calls `CameraCenter(1)` for a full reorient behind the hero (`MainLoop` in [PERSO.CPP](../SOURCES/PERSO.CPP), which then calls `FollowCamResetToView`).
- **Camera cycle (`I_CAMERA`):** rotates `AddBetaCam` by 90° (1024 units). **Classic:** `CameraCenter(2)` (preset `AlphaCam` / `VueDistance`). **Auto camera (`FollowCamera`, exterior):** recomputes `VueOffset*` / `BetaCam` from the hero and `AddBetaCam` only, then `CameraCenter(3)` — zoom and numpad elevation are not reset.
- **`GereExtKeys`:** keyboard-driven `AlphaCam` / `BetaCam` adjustment ([SOURCES/EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP) line 1910+).

## CameraCenter ([SOURCES/INTEXT.CPP](../SOURCES/INTEXT.CPP) line 331)

Central function for camera recentering. The `flagbeta` parameter controls angle behavior:


| flagbeta | Effect                                                                                                        |
| -------- | ------------------------------------------------------------------------------------------------------------- |
| 0        | Apply `Start*Cube` to `VueOffset`*; **no** angle change                                                       |
| 1        | Reorient behind hero (`BetaCam = 2048 - heroBeta`); reset `AddBetaCam`, `AlphaCam`, `VueDistance` to defaults |
| 2        | Like 1 but preserves `AddBetaCam` (oriented camera cycle)                                                     |
| 3        | No globals modified; just calls `SetFollowCamera` / `SetPosCamera`. Skips `SearchCameraPos`                   |


After the switch, `CameraCenter` calls:

- **Interior:** `SetPosCamera` on the tile-grid position.
- **Exterior:** `SetFollowCamera`, then (if `flagbeta != 3` and no `CameraZone`) `SearchCameraPos` for terrain/decor obstruction. On hit, overrides with `SetPosCamera` + `SetTargetCamera` and sets `FlagCameraForcee`.

### SearchCameraPos ([SOURCES/3DEXT/MAPTOOLS.CPP](../SOURCES/3DEXT/MAPTOOLS.CPP) line 257)

Probes terrain and decors along the camera-to-target line to find an unoccluded position. Iterates from near (1000) to `VueDistance` in 512-unit steps, checking `CalculAltitudeObjet` and `NbObjDecors` bounding boxes. Cost is proportional to `(VueDistance - 1000) / 512 * NbObjDecors`.

## Camera zones (authored shots)

A zone of type 1 re-aims the camera when the hero is inside its box. `SetZoneCamera` ([OBJECT.CPP](../SOURCES/OBJECT.CPP)) applies the zone's `Info0`–`Info2` as the render anchor (`Start*Cube`) and, in exteriors, its `Info3`–`Info6` as `AlphaCam` / `BetaCam` / `GammaCam` / `VueDistance`, then raises `CameraZone` and `FlagCameraForcee`.

### Two shapes, decided by Info7

`Info7` is the zone's flag word, and for a camera zone it decides not just whether the zone fires but how often. The dispatch fires when the hero is `NumObjFollow` and `ZONE_ON` is set, and then only if one of:

| condition | effect |
| --- | --- |
| `ZONE_OBLIGATOIRE` | fires **every frame** the hero is inside: the shot holds the view for as long as he stays |
| `CubeMode == CUBE_INTERIEUR` | same, every frame: this is how interior cameras work at all |
| `!ZONE_ACTIVE AND AllCameras` | fires **once**, then latches `ZONE_ACTIVE` and stops |

`ZONE_ON` comes from `ZONE_INIT_ON` at scene load ([DISKFUNC.CPP](../SOURCES/DISKFUNC.CPP)); a zone without it never fires, however the hero moves. `ZONE_ACTIVE` is cleared when the hero is outside the box, so leaving and re-entering re-arms the zone; a Life script can also clear it ([GERELIFE.CPP](../SOURCES/GERELIFE.CPP)).

Both shapes are common in the shipped data. Listing every cube's zones as its scene loads them (`cube N`, then `zonelist`, from the corpus save `Wannies fragment.LBA`) gives, on the GOG classic data, 242 enabled camera zones across the exterior cubes: 128 latching and 114 forced, with a forced zone in 36 cubes. The Activision US disc gives 240, 128 and 112, in 35 cubes. The two sets differ in the flags of three zones and nowhere else, and two of those are in cube 42, where zones 17 and 18 hold the view on the first and are switched off on the second. A zone named by cube and index therefore has to be checked against the data being played, which is how this was found. `CameraZone` is therefore true for one frame per entry on about half of them and for the whole visit on the rest. Both are pinned by `test_camzone_model.sh`.

`zonelist` prints `Info7` with its bits named, which is the quickest way to tell which shape a given zone is.

### What a camera zone also switches off

The angle is the visible part, but `CameraZone` gates three other behaviours, and anything that suppresses or bypasses zones inherits all four:

- **The hidden-hero recovery.** When the hero is completely masked by geometry, the engine recentres the camera, unless `CameraZone` is set ([OBJECT.CPP](../SOURCES/OBJECT.CPP)). An authored shot is assumed deliberate, including when it hides him.
- **The follow update, for that one frame.** `UpdateFollowCameraExt` is called from inside `if (!CameraZone)`
  ([PERSO.CPP](../SOURCES/PERSO.CPP)), so on the frame a zone claims the view the Auto camera does not run at
  all and the zone's own writes reach the screen untouched. Note what this is *not*: an authored shot is not
  exempt from the HD recompose. It escapes it for that single frame and is recomposed like anything else for
  every frame it holds afterwards. A boom written raw there is the one frame in the sequence rendered without
  the correction its neighbours have, which at 1080p is a third of the boom in and straight back out.
- **The recentre input.** `I_RETURN` is a no-op inside a zone.

`FlagCameraForcee` outlives the zone: the off-screen path clears it and restores `AlphaCam` / `GammaCam` / `VueDistance` to the `VueCamera` presets, which is how a forced shot's parameters are given back.

### Interaction with the Auto camera

As shipped, three settings decide what a scripted shot does to the Auto camera. A zone that fires once is passed over while the player steers (`cam_zone_skip`). A shot that does fire, which in play means one that holds the view, gives the yaw and the pitch back when the hero leaves its box (`cam_zone_return`). And a pan the player set does not survive a visit to a building (`cam_scene_carry`, under [Scene changes](#scene-changes)). Each is described below beside what it replaces, which is also what the camera does with that setting off.

A zone writes `BetaCam` directly, so it is one of the writers described above and needs `FollowCamAdoptAngle` to keep its angle. With that in place, how long the shot survives is `cam_hold_angle`'s decision, and both answers are intentional:

- **on** (default): the shot is held until the player orbits away or the scene changes, like any angle they set themselves.
- **off**: the pan drift returns the camera behind the hero as he walks on, handing the shot back gradually. This is only possible because the angle was adopted; a shot the Auto camera never took ownership of cannot be given back smoothly, only snatched back.

What is held is the offset, not the shot. The zone's yaw is a world angle, and adopting it turns it into a pan measured from the heading the hero had on the frame the shot fired. From then on the Auto camera carries that pan round with him, so the authored direction is lost on his first turn and the camera is left wherever the shot's angle happened to fall relative to him. Measured in cube 90 with nobody touching the camera:

- **A latching zone** shows its shot for one frame. The boom goes back to the player's own arm on the next, and the yaw follows the hero's turning from then on, still inside the box. With a shot that faced him (a pan of 163 degrees) he was fully hidden about two seconds after it fired and the hidden-hero recovery cut the camera behind him. The classic camera stayed on the shot throughout.
- **A forced zone** matches the classic camera for as long as he is inside, since the follow update does not run there. On the way out the boom snaps to the player's arm and the camera swings by however far he turned inside the box (104 degrees over about two seconds, with him standing still), then keeps the pan it lands on.

The pitch stays as authored in both, because the path that restores the presets (above) is skipped for the exterior Auto camera. None of this is given back by walking on, by leaving the box, by crossing into the next exterior cube, or by going through a door and coming back out: every zone-triggered scene change, doors included, takes `CameraCenter(0)` and leaves the pan and the pitch alone. An orbit, Center camera, a scene change ordered by a script, the console or a load (those take `CameraCenter(1)`), or the hidden-hero recovery clears it. `test_camzone_hold.sh` compares the two cameras while the hero stands still, which is the one case where they agree.

### A shot as a loan

Two settings treat an authored shot as something the Auto camera lends the view to and gets back, instead of something whose leftovers it keeps. `cam_zone_return` is on by default; `cam_zone_anchor` is a prototype and off. With both off the camera's state matches the build without them frame for frame.

- **`cam_zone_return`** (cfg `FollowCamZoneReturn`): when the hero leaves the box of the shot that fired, the yaw pan and the pitch ease back to what the Auto camera had before the shot, at the pan drift's pace (`cam_recenter_drift`) and whether or not he is walking. For a player who never touches the camera that is behind the hero; for one who had set an angle it is that angle. The return starts from the angle on screen, so leaving a forced zone is one motion and not a swing followed by a drift. An axis the player sets by hand while the shot has the camera is theirs and is not given back: an orbit or the quarter-turn key for the yaw, a tilt for the pitch. A second shot firing before the first has been returned changes what is on screen but not what is owed.
- **`cam_zone_anchor`** (cfg `FollowCamZoneAnchor`): inside the box of a zone that fires once, hold the shot's world yaw as the classic camera does, by re-solving the pan against the hero's facing every frame. Forced zones already behave this way. On its own it ends at the edge of the box and nothing is given back.

The loan ends without a return whenever something else decides where the camera points: Center camera and every other `CameraCenter(1)` or `(2)`, which includes the hidden-hero recovery and a script's camera order, then a cutscene, the Auto camera being switched off, and loading a save. The boom is not part of it and still goes back to the player's arm on the first frame the update runs.

Straight out of a cutscene the pan is whatever its last shot left, so a shot that fires as one ends, over the spot the script left the hero at, owes back "behind the hero" for the yaw (`FollowCamNoteCinema()`). The ferry's arrival on the Desert Island is one: the dock's shot fires on the frame control comes back.

A visit to an interior does not end it. A door changes scene through `CameraCenter(0)`, which leaves the pan and the pitch as they were, and nothing indoors uses either, so a shot's leftovers come back out of the building with the hero. What was owed before he went in is still owed when he comes out, whether he lands back in the shot's box or in the open. One thing about it changes on the way out: for the yaw, what is owed becomes "behind the hero", whatever pan it was lent at. The pan is an offset from his facing and he comes out facing the other way, so a pan that put the camera beside him going in would put it in front of him coming out. That includes a pan the player set while the shot over the door had the camera, which the zone lends afresh as he steps back outside. Played at the Tavern door, giving that pan back as it stood left the camera 159 degrees round for the next thirty seconds; an orbit followed by a walk out of the box with no building in between still keeps its angle. This matters most where it is commonest: of the 95 enabled doors that lead from an interior to an exterior, 57 have their arrival point inside an enabled camera zone (48 latching, 9 forced), going by each door zone's target point in the same listing as the zone counts above; on the US disc it is 58 of 94.

What is owed for the pitch is the Auto camera's own pitch, which is not always what `AlphaCam` holds when a shot fires. Two writers leave a pitch there that is neither the player's nor the camera's rest, and nothing takes either away again: a camera zone, and the obstruction search `CameraCenter` runs, whose camera position the next follow update discards while the pitch it wrote beside it stays. A door's recentre runs that search on the arrival frame, just ahead of the zone over the door, so a loan taken against `AlphaCam` as it stood handed the search's pitch back: 767 at the Tavern door, above anything the player can tilt to, for a hero who started indoors. The camera's own pitch is kept separately (`FollowCamOwnAlpha`) and is written in three places only: the player tilting, a recentre setting its preset before the search runs, and a load. The recentre the engine makes when it loses sight of the hero is not the player's, so it leaves that value as it found it. Easing the pitch on screen back to it outside a shot is not done here.

Knowing the hero is still inside a latching zone's box needs more than `CameraZone`, which is true only on the frame the shot is applied. `CheckZoneSce` sets `FollowCamInShotBox` on each frame the followed object stands in the box of a camera zone that has fired, and `camtrace` reports it as `inshot`, beside `loan` (1 yaw, 2 pitch, 4 on its way back).

### Passing a latching shot over

Everything above takes it as given that the shot fires. `cam_zone_skip` (cfg `FollowCamZoneSkip`, on by default) is the other answer, and the one that ships: a latching shot does not take the view from a player who is steering. On, the dispatch in `CheckZoneSce` passes a latching zone over while the Auto camera is on, the hero's `Move` is `MOVE_MANUAL` and no cutscene is running. Forced zones, interiors and the classic camera are as they were, and so is a hero a script is moving, who still gets the shot. A zone passed over is not latched, so it fires if a script takes the hero over while he is inside the box, and it still shadows the camera zones after it in the list, as it does when it fires.

What comes with it:

- **Everything `CameraZone` gates stays on.** A shot authored for a spot where the hero is hidden no longer covers that spot, so the hidden-hero recovery fires there instead and leaves the obstruction search's pitch (see [Scene changes](#scene-changes)).
- **A door that lands in a latching zone becomes an arrival with no shot**, which is most doors. `cam_scene_carry` is what makes that arrival a clean one.
- **Session recordings do not carry the setting.**

## Auto camera (`FollowCamera` — community addition, not in the original game)

Config key `FollowCamera` (0 = classic, 1 = auto; default 0). Also reads legacy key `AutoCameraCenter` for backward compatibility. Toggled in Options → Advanced options ("Auto camera" / "Classic camera" — localized). Off by default so the original camera behavior is preserved.

When enabled in exterior mode (and not in a camera zone or cinema), the implementation is a third-person follow with several coupled pieces (tuning in `FOLLOWCAM_CFG.H`, logic in [SOURCES/FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP) / `EXTFUNC.CPP`):

### Spring arm (zoom)

Zoom is not a single distance: the player sets a target arm length, and the camera smoothly converges toward it each frame — a standard spring-arm pattern:

- **`FollowCamBaseDist`** — target distance (numpad `/` closer, `*` farther; clamped `FOLLOW_CAM_DIST_MIN`–`FOLLOW_CAM_DIST_MAX`).
- **`FollowCamEffectiveDist`:** smoothed length (private state in `FOLLOWCAM.CPP`); moves toward `FollowCamBaseDist` by `FOLLOW_CAM_SPRING_RECOVER` per frame whether the arm is too short or too long.
- **`VueDistance`** is set from **`FollowCamEffectiveDist`** before `CameraCenter(3)`, so terrain render matches the eased distance.

Branch history tried heavier correction (terrain penetration along the boom, LOS samples); those were removed to keep behavior predictable and the PR focused — so there is no lens pull-through-terrain and no classic `SearchCameraPos` on this path (see below).

### Lazy orbit and pan

- **Rotation lag:** `BetaCam` eases toward “behind the hero” with distance-scaled inertia (`FollowCamEffectiveDist` / `FollowCamBaseDist` scales it) — closer zoom = snappier orbit, longer arm = lazier drift. The pace is `cam_ease_ms`, a time constant; see [Eases paced by the clock](#eases-paced-by-the-clock).
- **Pan drift:** `[` / `]` adjust `AddBetaCam`; drift back toward center only while the hero is walking; standing still preserves pan. Disabled by **hold-angle mode** (`cam_hold_angle`, default on): the manual rotation is then held indefinitely, like a free third-person camera, and only re-centers when the player hits Center camera (Enter / gamepad B) or a script or the console orders a scene change; both go through `CameraCenter(1)`, which zeroes `AddBetaCam`. Coming out of a building sends the pan home as well (`cam_scene_carry`, on by default); walking across the seam between two outdoor scenes does not (see [Scene changes](#scene-changes)). `cam_hold_angle 0` restores the classic lazy drift-back.

### World anchor (prototype)

`cam_world_anchor` (cfg `FollowCamWorldAnchor`, off by default) changes what the horizontal angle is held against. Off, the Auto camera aims behind the hero offset by `AddBetaCam`, so a turn of his turns the camera with him once any grace after a pan has run out, and hold-angle holds the offset, not a direction. On, each update takes the hero's turn since the previous update back out of `AddBetaCam`, which leaves the target where it was in the world. Whatever else wrote the offset in between (an orbit, `[` / `]`, the quarter-turn key, a script) is kept, so the player still turns the camera and only the hero no longer does. The pan drift is off with it, whatever `cam_hold_angle` says, because the drift is a way back behind the hero.

The turn is taken out only across consecutive updates. After a frame without one (a camera zone holding the view, a cutscene, a building) or a cut, the offset is taken as it stands, since whoever had the view set it for the facing the hero has then. `FollowCamAdoptAngle` re-solves the offset against the hero's facing as it is, so it tells the anchor that the turn so far is already in it.

Center camera, an ordered scene change and the hidden-hero recentre still put the camera behind the hero, and the anchor holds that direction from there. With the camera no longer swinging round behind him, a hero walking side-on past scenery is hidden from it more often than before, and the recentre fires where it otherwise would not.

With `cam_zone_return` on, what a shot owes back for the yaw is a world direction too: the pan it was lent is re-solved against the hero's facing every update, from the facing it was measured at, across the frames a holding shot keeps the update from running. Leaving the box brings the camera back to where it faced before the shot, however the hero turned inside. Inside the box of a shot that fires once, the anchor holds the shot's angle, as `cam_zone_anchor` does.

### Orbit gestures (mouse drag, right stick)

Every analog camera source goes through `ApplyManualCameraNudge` ([EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP)) so they clamp each axis identically. Horizontal orbit is the axis with state attached, and a gesture has three parts:

- **Start.** The first frame of a gesture calls `FollowCamAdoptAngle`, so the stick's delta applies from the angle on screen rather than from wherever the hero-relative target had drifted to. Only the first frame: doing it every frame would halve the stick's travel, since the lerp's steady-state lag is what the following frames are spending.
- **Drive.** Orbiting selects a tight lerp divisor (`cam_smooth`, default 2) instead of the auto-follow's distance-scaled one, and re-arms `FollowCamReengageDelay` each frame. In steady state the camera moves at the speed asked for, lagging the target by twice the per-frame step.
- **Release.** The lerp ramps *up* to the stick's speed over about four frames and has nothing to ramp it back down, so the orbit would otherwise stop dead from full speed. The last speed is decayed by `cam_glide` percent per frame and fed back through the same nudge, spending the tail over several frames. Because it goes through the nudge, `AddBetaCam` moves with it and the angle the camera settles on is the one the follow update recomputes.

`FollowCamForgetManualGesture()` ends a gesture outright, called from the per-frame check for camera zone / cutscene / interior / camera-off and from `CameraCenter`. Without it the tail resumes on a camera the player has since re-aimed, and the stale "gesture under way" flag makes the next nudge skip its realign.

The follow-through is tuned for the stick, which springs back to centre so "no input this frame" reliably means released. A mouse held still reports the same thing while still being driven, which is #514.

**Timing is per update, and an update is a simulation step.** The orbit's lerp divisor, the glide decay and the divisors of the other eases here are a share per call of `UpdateFollowCameraExt`. That call sits inside the fixed simulation step ([MOVEMENT_FRAMERATE.md](MOVEMENT_FRAMERATE.md); `FixedTimestep`, 16 ms by default), not once per rendered frame: a 33 ms frame runs it twice. So with the step on, which is the default, a per-update ease takes the same time at any frame rate. The per-update lerp brings a quarter turn behind the hero within 5% in 1664 ms at 4, 7, 16, 33 and 50 ms frames: above 60 fps most frames skip the simulation and the camera with it, and it moves on the next step. With `fixedtimestep off` an update is a rendered frame long and the same turn takes 3432 ms at 33 ms frames.

For a test that asserts a duration this has a trap in it. Under `--fixed-dt` above 16, one tick is one rendered frame and several `camtrace` rows, one per step, so the time a motion took is its rows times the step, not its rows times the frame.

### Eases paced by the clock

Two settings pace an ease by the game clock, in milliseconds, in place of a share per update. Both read `TimerRefHR`, which stands still in a menu and is what a session replay reproduces, and take the time since the previous update (`FollowCamEaseTick()`).

- **`cam_ease_ms`** (cfg `FollowCamEaseMs`, default 300): the time constant of the turn behind the hero. 300 is what the 2022 Classic release eases its auto camera with, and brings a quarter turn within 5% in about 0.9 s. 600 is the pace the per-update lerp has at the default step, about 1.7 s. 0 goes back to that lerp. Unlike the lerp, which stops inside its 10 unit snap threshold and leaves the camera up to a degree off until something else redraws the frame, the timed ease lands on its target. An orbit the player is driving keeps the per-update lerp (`cam_smooth`): that one smooths a device.
- **`cam_target_ms`** (cfg `FollowCamTargetMs`, default 300): the camera looks at a spot `DISTANCE_VISEE` ahead of the hero. What it looks at trails that spot and closes on it with this time constant (`FollowCamEaseLookAt()`), as the Classic release's does at 300: the hero leads the frame a little while he runs and the view settles after he stops. 0 pins it to the spot, so the view starts and stops on the frame he does.

The arithmetic is `FollowCamEaseStepMs()` in [FOLLOWCAM_MATH.H](../SOURCES/FOLLOWCAM_MATH.H). The share of what is left is `2dt / (2tau + dt)`, which follows `1 - exp(-dt / tau)` within one percent while an update is no longer than a third of the time constant and cannot pass the target until an update is twice the time constant long. The plain `dt / tau` runs a tenth fast at 20 updates a second and overshoots on any update longer than the time constant. The move is worked out in thousandths of a unit and what does not make a whole unit is carried to the next update; dropped, it made the same turn a tenth slower at 8 ms updates than at 16.

Three rules keep a clock-paced ease from turning time that was not a frame into a jump:

- **A step without the update restarts the clock.** `FollowCamNoteFrame()` counts simulation steps, and an update that does not follow one on the step before is paced by no time at all and moves nothing. A rendered frame that skips the simulation (above about 60 fps with the fixed step on) is not a step, so it is no gap. A camera zone, a cutscene or a building may have held the view for any length of time.
- **A cut restarts it too**: `FollowCamSyncTarget()` and `FollowCamForgetLoan()`, which between them cover Center camera, every `CameraCenter(1)` and `(2)`, the quarter-turn key, an arrival under `cam_scene_carry` and a loaded save, whose clock comes from the file. The look-at point goes back onto its spot at the same moments.
- **A long update is paced as one of 100 ms** (`FOLLOW_CAM_EASE_DT_MAX`), so a stall is not spent in one step.

What `cam_target_ms` keeps is how far the look-at point trails, not where it is. Walking across the seam between two outdoor scenes renumbers the world under the hero without moving him on screen, and a place remembered in the old scene's coordinates would be a scene's width away in the new one's. Each update the trail grows by as far as the spot moved and then closes. A spot that moved more than `FOLLOW_CAM_TARGET_JUMP` in one update was not walked there, and the trail is held as it was for that update.

### Apply path

- Uses `CameraCenter(3)` (apply current globals only; skips the classic `SearchCameraPos` snap). Terrain occlusion is instead handled by the eased ground-clearance below; decor/scenery is not yet cleared for. This keeps the orbit from being fought every frame and matches “cinematic follow” rather than a hard “collision camera.”

**Camera elevation:** the Camera-level inputs (`I_CAMERA_LEVEL_PLUS` / `I_CAMERA_LEVEL_MOINS`, bound by default to numpad `+` / `-` with Page Up / Page Down as the second binding) adjust `AlphaCam` freely (range 150–600) instead of switching between the two fixed `VueCamera` presets. Fires every frame while held (no debounce) for smooth real-time tilt. Being an input action rather than a raw key, it follows any rebinding done in Options → Keyboard.

**Zoom input:** numpad `/` and `*`, or the mouse wheel, update `FollowCamBaseDist` every frame while held; idle zoom/tilt still apply (dirty check includes base distance and `AlphaCam`). Unlike elevation, zoom and pan read raw scancodes through `CheckKey` in `GereExtKeys`, so they are not rebindable and the numpad is the only keyboard route to zoom.

**Zoom is per-session.** `FollowCamBaseDist` is neither persisted to `lba2.cfg` nor exposed as a console cvar. Only two things write the resting value: the first-frame init, and Center camera (Enter / gamepad B), which snaps it back to `FOLLOW_CAM_INITIAL_DIST`, the midpoint of the two `DefVueDistance` presets. Walking between scenes does not reset it. Changing the resting zoom therefore means editing `FOLLOW_CAM_INITIAL_DIST` in `FOLLOWCAM_CFG.H`; at tall render heights the HD recompose below also pulls the boom in, and that gain *is* live and persisted (`cam_hd_dist`).

### HD recompose (tall render heights)

The exterior projection has a fixed focal (`ChampX`/`ChampZ` = 600); a taller framebuffer therefore reveals more vertical field of view rather than zooming, so the frame fills with sky and the hero shrinks (see [WIDESCREEN.md](WIDESCREEN.md) "Vertical framing at HD"). At 1080p the vertical FOV roughly doubles, so the follow camera measurably mis-frames the subject: far away, with the upper third lost to sky.

The Auto camera answers this with a render-time recompose ("recompose, not crop"). With `k = ModeDesiredY / 480` (so every term is an exact no-op at the original 480 height), each apply pulls the boom in, steepens the pitch, and shortens the forward lean in proportion to `k - 1`:

- **Render-time only.** The player's logical `AlphaCam` and `FollowCamBaseDist` are read, adjusted for the `CameraCenter(3)` call, and restored, so manual tilt/zoom and the spring arm keep their logical values.
- **Auto path only, never in a camera zone.** Authored / scripted shots (`CameraZone`) and the classic camera are untouched, as are isometric interiors.
- **Tunable live**, then baked into `FOLLOWCAM_CFG.H`: `cam_hd` (master), `cam_hd_pitch`, `cam_hd_dist`, `cam_hd_lean` console cvars. `FollowCamHDExcess()` ([FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP)) returns the `k`-excess the apply path scales by.

Pitch is the binding lever and saturates at the `AlphaCam` clamp (600 ≈ 53°), so on an open vista the recompose brings the subject back to a good size but leaves a natural widescreen horizon band rather than a 4:3-tight sky.

**Status: tuned for ~720p; 1080p is experimental.** The strength scales linearly with render height, which over-reaches at 1080p: the subject can feel too far and terrain near-clipping is more visible (the height-scaled near clip reduces it but does not fully solve it; a per-cell near-plane clip of the terrain grid is the real fix). The `cam_hd_*` cvars (and `cam_hd_cap`, which caps the strength so tall resolutions converge) let you tune it, and `cam_hd 0` disables it. Refining the 1080p framing and the terrain near-clip is left to a focused follow-up.

**480 and 1080 are different regimes, so a camera change verified at one is not verified.** Every recompose
term is an exact no-op at 480, which means a fix measured only there has not met the correction at all, and
one measured only at 1080 has not met the original framing. A change to the boom that removed a 14% jump at
480 introduced a 49% one at 1080, because the frame it corrected is the one frame the recompose never
reaches. Capture both, and treat a result from one height as half an answer.

### Ground / occlusion clearance

A smooth port of the classic `SearchCameraPos` terrain awareness onto the follow path (`cam_ground`, default on; clearance `cam_ground_clear`). After `CameraCenter(3)` positions the eye, a few points are sampled along the eye-to-hero line (`FollowCamHDExcess`-style, in `FOLLOWCAM.CPP`). Where terrain there would rise above the line of sight (a hill between the camera and the hero, or the eye sinking into rising ground), the eye is raised just enough to clear the worst occluder and re-aimed at the hero via the same `SetPosCamera` / `SetTargetCamera` calls the classic recenter uses.

The difference from the classic path is that the lift **eases** toward its target every frame (`FollowCamEyeLift`, tuned by `FOLLOW_CAM_GROUND_*`) instead of snapping. An earlier always-on snap fought the orbit and was reverted; easing is what makes it safe to run every frame. A `FollowCamGroundSettling` flag keeps the dirty check live until the lift converges, so it finishes even while the hero stands still. Active at every resolution (world awareness, not HD-specific), skipped in camera zones and when the eye leaves the cube (where `CalculAltitudeObjet` is invalid). It clears terrain only; decor/scenery occlusion is not yet handled.

**Eased state is self-correcting; latched state is not.** The distinction decides what a discontinuity
(a scene change, a camera zone, a recentre) has to reset. `FollowCamEyeLift` and the spring arm converge on
whatever the new situation asks for a frame at a time, so carrying a stale value across costs a short glide
and zeroing it costs a jump of thousands of units. `FollowCamTargetBeta`, the re-engage and manual-hold
counters and the orbit follow-through do not converge on their own: nothing retires them, so they discharge
against a situation they were never measured in. Drop what latches, leave what eases.

### Obstruction clearance

The classic camera asks `SearchCameraPos` how steeply it has to look down for the boom to clear the terrain, the scenery boxes and the scripted objects between it and the hero, once per recentre, and the answer is written into `AlphaCam`. It gives that pitch back at its next off-screen recentre. The Auto camera has that recentre switched off, so the pitch stays as though the player had set it: out of the Tavern's door the camera looks down at 767, above the 600 the player's own tilt stops at, and goes on doing so wherever he walks and however the camera is turned.

`cam_obstruct` (cfg `FollowCamObstruct`, on by default) has the Auto camera ask the same search itself, on every update, and keep the answer out of `AlphaCam`. `FollowCamNeededPitch()` runs `SearchCameraPos` with the globals it writes saved and restored. What the place needs above the player's own pitch is carried as an extra, `FollowCamObstructExtra`, that eases toward it each update, up and down (`FOLLOW_CAM_OBSTRUCT_*`), so there is never a pitch to give back. With the setting off the camera's state matches a build without it frame for frame.

The extra reaches the frame through `FollowCamRenderExtraPitch()`, which `CameraCenter` and the scene render add to `AlphaCam` where they set the exterior camera. It cannot be applied the way the recompose applies its own pitch, by changing `AlphaCam` for the update's `CameraCenter(3)` and putting it back: `AffGrilleExt` ([EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP)) sets the camera again from the globals before it draws, so anything applied that way is gone by the time the frame is rendered. That is true of the recompose's pitch steepen today (at 1080p a frame rendered with `cam_hd_pitch` at 0, 180 or 400 is byte for byte the same, while `cam_hd_dist` and `cam_hd` change it), and by the same mechanism of the ground lift above, whose `SetPosCamera` is overwritten likewise. Both are findings about those two, not something this setting changes: the extra is measured from the player's own pitch, which is the pitch the frame is drawn with.

What stands down while the setting is on and the Auto camera has the view outdoors:

- **`CameraCenter`'s own search.** It no longer writes `AlphaCam` or raises `FlagCameraForcee`. A re-aim (flags 1 and 2) is a cut already, so the extra starts the next update settled at what the place needs; so does an arrival through `cam_scene_carry`. A plain re-apply (flag 0, as at the seam between two outdoor scenes) leaves it to ease.
- **The hidden-hero recentre.** A hero drawn fully hidden with the clearance still rising tells it to hurry (`FollowCamObstructHidden()`), and the recentre, which would zero the player's pan, is left out. How hard it hurries is `cam_obstruct_hurry` (cfg `FollowCamObstructHurryDivisor`): the ease divisor used on those frames, from 1, a cut, to 10, the ordinary ease and so no hurry at all, which is the default. Lower shows him sooner and tilts harder to do it. Arriving at the Tavern's door at 1080p with the extra starting from nothing, 3 leaves him hidden for 2 frames with a step of 140 units, 6 for 3 frames with a step of 70, and 10 for 5 frames with a step of 46. Played at that resolution, 3 read as a pop and 10 did not show. If the extra has given all the search asked for and he is hidden still, the search does not see what is in the way, and the render's word is taken over it: the extra is raised a step (`FOLLOW_CAM_OBSTRUCT_HIDDEN_STEP`, 20 units, taken whole and not eased) for each such frame and held as a floor until he has walked `FOLLOW_CAM_OBSTRUCT_FLOOR_REACH` away from where it was last raised. Walking west out of Near Tavern into Near the Harbor with the camera panned is such a place: a few frames before the seam the search asks for less tilt and the hero is hidden. Only when there is no pitch left to give does the classic recentre run.

In the box of a shot that has fired the search is not asked: the place is the author's to judge there, as it is for the classic camera, which never searches inside a zone. A hero drawn hidden there is still answered, through the floor, since with the recentre stood down nothing else would be. A shot starts as written: the extra and any floor are dropped as it is applied (`FollowCamObstructShotFired()`).

None of it applies in a cutscene. The script's own re-aims and scene changes get the search and the centring they were written against, as they do without the setting.

What it does not do:

- **Only the pitch is taken from the search.** The classic search also moves the target in toward the hero as it steepens; this does not, and the boom is not shortened either.
- **It sees what the search sees**: boxes, sampled every 512 units out along the boom. Scenery the boxes do not cover still hides the hero for the few frames it takes the floor to rise.
- **Session recordings do not carry the setting.**

### Manual-override fade

The HD recompose steepens pitch and the ground-clearance re-aims the eye, both on top of whatever the player set. Left alone, that fights a manual tilt: the recompose offsets it and the re-aim pins pitch to the geometry. Any manual camera nudge therefore arms `FollowCamManualHold` (in `ApplyManualCameraNudge`), which drives an `autoFactor` to 0 so the **pitch** assist (recompose pitch steepen + ground-lift re-aim) yields completely while the player drives; it eases back to full over `FollowCamManualHoldFrames` (cvar `cam_manual_hold`, ~0.75 s) once they let go. That key is bounded to 0..600 frames (ten seconds at 60fps), and a cfg holding more is read back at 600 and written back at 600: the fade multiplies the counter by 100, so an unbounded value overflows and lands the assist at full strength, which is the opposite of what a manual nudge asked for. This mirrors the existing azimuth re-center (`FollowCamReengageDelay`) on the pitch axis. The boom pull-in and lean are deliberately **not** faded, so grabbing the stick to orbit does not pop the camera out to full distance.

**Performance:** Two optimizations keep the per-frame cost manageable:

1. **Idle skip:** Dirty check on hero `X`, `Y`, `Z`, `Beta` skips all camera/terrain work when the hero hasn't moved. Standing still has zero extra cost.
2. **Full refresh on movement:** Sets `FirstTime = AFF_ALL_FLIP` for full render + flip in one frame. `AFF_ALL_FLIP` runs the `AffScene` preamble (`MemoClipWindow` / `UnsetClipWindow` / `ClearImpactRain`) preventing black edges. Vsync caps at ~60 Hz. Timer compensation (`SaveTimer`/`RestoreTimer` + `TimerSystemHR`) keeps game logic at correct speed.

See [CONFIG.md](CONFIG.md) for persistence and [MENU.md](MENU.md) for the menu entry.

### Scene changes

How the hero reaches an outdoor scene decides what happens to the Auto camera, because `ChangeCube` ([OBJECT.CPP](../SOURCES/OBJECT.CPP)) centres the camera differently for each way in and none of the three was written with this camera in mind. Measured at 640x480 with `--fixed-dt 16` and hold-angle on:

| Arrival | `ChangeCube` calls | What the Auto camera is left with | With `cam_scene_carry` |
| --- | --- | --- | --- |
| Walking across the seam between two outdoor scenes | `CameraCenter(0)` | Pan, pitch and yaw as they were, with no step. | The same; this case is not touched. |
| A change ordered by a script or the console (`FlagChgCube` 2 or 0) | `CameraCenter(1)` | The pan zeroed. The pitch back at the preset, or at the obstruction search's answer when the search finds something. | Between two outdoor scenes, pan and pitch kept and the camera behind the hero at that pan. From an interior, as for a door. |
| A door out of an interior | `CameraCenter(0)` | The pan kept, and the yaw where the last outdoor scene left it. Having walked in facing the door and out facing away from it, the hero arrives with the camera half a turn from its target, and it spends about 140 frames swinging round him. The pitch is rewritten when the search finds the building. | The pan sent home and the pitch left alone, the camera directly behind the hero from the first frame. |

A camera zone that fires where the hero lands decides the view either way, which hides the door's swing behind the shot. Of the 95 enabled doors that lead from an interior to an outdoor scene, about 60 land inside the box of an enabled camera zone (about 50 that latch, about 10 forced) and about 35 land in the open; with scenario cameras off the latching ones join those. The count is of the Classic re-release's scene data, takes the zones as each scene loads them and reads the landing spot off the door zone's target point. It moves by a few doors with how that point and the box edges are read (57 to 62 in a zone), so treat it as a size, not a census.

`cam_scene_carry` (cfg `FollowCamSceneCarry`) is on by default. On, `FollowCamCarryIntoScene()` ([FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP)) takes the second and third arrivals in place of `ChangeCube`'s own centring, except in a cutscene, where a scene change is the script's and is centred as it always was. The pitch is the player's and is left alone. The pan holds from one outdoor scene to the next and goes home when the scene left was an interior: it was set for a view the hero left at the door on his way in, and held through a building it turns the camera to his side or his face as he steps out. The camera is then aimed behind the hero at that pan with the lerp's target on it, and the re-engage countdown and any orbit in flight are dropped, since both describe the scene just left. With the setting off the camera's state matches a build without it frame for frame.

What it does not do:

- **The pitch can still be taken at a door.** The scene change no longer runs the classic obstruction search, but where the spot behind the hero is inside the building he has just left, he is drawn fully hidden on the arrival frame and the hidden-hero recentre (`AffOneObject`, `CameraCenter(1)`) fires as it would anywhere else. It zeroes the pan and leaves the search's pitch: 767 at the Tavern's door (cube 3 to cube 42) with scenario cameras off, where the player's own tilt stops at 600. Nothing gives that pitch back. The classic camera returns to its presets at the next off-screen recentre, and the Auto camera has that path switched off. This is the decor half of the clearance listed under Future work, and it happens wherever the hero is hidden, not only at a door. `cam_obstruct` is the answer to it (see [Obstruction clearance](#obstruction-clearance)).
- **An ordered change that arrives in an interior still recentres**, so a pan and a tilt do not survive a scripted trip indoors.
- **A pitch left by a camera zone is carried like any other.** Whether a shot's pitch is the player's to keep is a question about camera zones, not about scene changes.
- **Session recordings do not carry the setting**, so one recorded with it on diverges when replayed with it off.

## Observing and testing the camera

The camera resisted iteration for a long time because nothing about it was asserted: a change was judged by playing, and a regression arrived as a field report. A 75 degree snap from a 1 unit stick touch survived two releases that way.

**`camtrace <0|1>`** logs one line per frame: the angles, how far `BetaCam` moved and who moved it, the re-engage countdown, whether an orbit is driving, `zone` / `forced` / `cine` / `follow` / `ext` / `vue` / `alpha` / `dist` for which camera holds the view, `inshot` / `loan` for a scripted shot's hold on it, `obs` for the pitch `cam_obstruct` is adding for the frame, `hid` for what the last render made of the followed object (0 seen, 1 drawn fully hidden and left so, 2 hidden with the clearance still rising, 3 hidden and recentred), `dt` for the milliseconds the Auto camera's update was paced by (0 on a frame it did not run, and on the first one after such a frame), and `tlag` for how far the look-at point trails its spot under `cam_target_ms`. It is emitted from the main loop rather than from the Auto camera's update, so it reports camera zones, cutscenes, interiors and the classic camera, none of which run that update. `target` is maintained by the Auto camera alone and reads stale when `follow` is 0.

**`camnudge <dBeta> [dAlpha] [frames]`** feeds the same per-frame nudge the mouse drag and the right stick feed, at the same point in the frame, so the analog camera can be driven without a device.

**`--dump-state`** carries a `camera` block for end-state assertions, and **`zonelist`** prints each zone's box and `Info7`, which turns "get into a camera zone" into a concrete `cube` plus `teleport`.

The camera is guarded in two places, and it is worth knowing which is which before relying on either. The behavioural fixtures below drive the real engine, so they need retail data and a display and run in **no CI workflow**: every platform runs `ctest -L host_quick` and nothing else. What CI does see is `tests/camera/test_followcam_math.cpp`, which covers the angle arithmetic from `FOLLOWCAM_MATH.H` over its whole domain, links nothing, and needs no data. A change to the camera that CI passes has had its arithmetic checked and its behaviour not.

Fixtures live in `tests/automation/`, with `camlib.sh` turning a run into a per-frame table so each asserts on the shape of a motion rather than one end state:

| fixture | what it pins |
| --- | --- |
| `autocam_orbit_snap` | a 1-unit touch moves the camera 1 unit, with a turn's worth of rotation pending |
| `followcam_hold` | the angle survives the hero turning underneath it |
| `followcam_release` | letting go eases down instead of halting |
| `followcam_tracking` | the camera orbits at the speed asked for |
| `followcam_recenter` | the classic camera drifts back while walking, not while standing |
| `followcam_interrupt` | an interrupted gesture ends instead of resuming |
| `followcam_obstruct` | with `cam_obstruct`, the tilt over a building is an extra that comes and goes with the building, the player's pitch is never rewritten, and the hero is never hidden or recentred |
| `followcam_ease` | with `cam_ease_ms`, the turn behind the hero takes the time the setting says at either update length and lands on its target; with `cam_target_ms`, the look-at point trails the hero, closes after he stops, and keeps its trail across the seam between two scenes |
| `followcam_scene_carry` | with `cam_scene_carry`, the camera arrives behind the hero with the pitch left alone: the pan kept across an ordered change between two outdoor scenes, and sent home after a visit to a building |
| `followcam_world_anchor` | with `cam_world_anchor`, the target holds still while the hero turns on the spot and while walking, an orbit still moves it, the drift of `cam_hold_angle 0` stands down, and a holding shot gives back the world direction the camera had before it |
| `camzone_model` | the two shapes a camera zone comes in |
| `camzone_skip` | with `cam_zone_skip`, a latching zone leaves a steering player's view alone under the Auto camera; forced zones and the classic camera are unaffected |
| `camzone_hold` | both cameras leave an authored shot in the same place |
| `camzone_return` | with `cam_zone_return` or `cam_zone_anchor` on, a shot's yaw and pitch come back on leaving its box, the player's own stay, and a latching shot holds its world angle |

Two habits are worth keeping when adding to these. **Run a new fixture against a build without the thing it guards** and confirm it fails there; several of these passed at first because their setup never happened rather than because the engine was right. And **assert the cause when the symptom is unobservable**: the realign runs in the input pass, before the camera update logs anything, so on a working engine the divergence it corrects leaves no trace at all.

### Future work

- **Rendering architecture:** A faster terrain path (GPU or structural changes) would reduce the CPU cost of per-frame `RefreshGrille`.
- **Rebinding:** optional rebinding of zoom/tilt/pan (today numpad-heavy) for laptops and alternate layouts. The right stick already drives orbit and elevation (`cam_stick_*`).
- **Hero-relative hold:** an angle held as an offset from the hero's facing cannot express a held world heading, which is what makes turning while the stick is down still drag the camera. #351 proposes anchoring horizontal rotation to the overworld instead, retiring that whole class rather than correcting instances of it. `cam_world_anchor` is a prototype of it (see [World anchor](#world-anchor-prototype)).
- **Mouse follow-through:** the release tail suits a stick that springs back to centre, not a pointer held still (#514).
- **Decor occlusion and clipping:** #363.
- **Auto camera vs terrain / decor:** the eased ground/occlusion clearance above now ports the terrain half of `SearchCameraPos` (an earlier always-on snap was reverted for fighting the orbit; easing fixes that). Still open: decor/scenery occlusion (the classic path also tests `TestZVDecors`), and the eye leaving the cube on far authored cameras (the clearance is skipped there, matching the classic out-of-cube guard).
- **Decor occlusion:** the ground clearance clears terrain only; the classic path also tests scenery boxes (`TestZVDecors`). `cam_obstruct` asks the classic search itself, boxes and scripted objects included, and eases its answer (see [Obstruction clearance](#obstruction-clearance)).
- **Render-time camera adjustments that do not reach the frame:** the recompose's pitch steepen and the ground lift are applied to the camera by the follow update and overwritten when `AffGrilleExt` sets it again from the globals (measured for the pitch, by mechanism for the lift; see the same section). Making either live changes what every player at a tall resolution sees, so it is a decision of its own.
- **Manual camera at tall heights:** the recompose runs on every apply, including while mouse/stick-orbiting, so the manual cam inherits the HD framing fix. Widening the manual `AlphaCam`/`FollowCamBaseDist` clamps with `k` and normalizing raw mouse deltas by render height are open refinements for fine manual control at 1080p+.

## Code reference


| Concept                 | File                       | Symbol                                                                              |
| ----------------------- | -------------------------- | ----------------------------------------------------------------------------------- |
| Low-level camera        | LIB386/3D/CAMERA.CPP       | `SetCamera`, `SetFollowCamera`, `SetPosCamera`, `SetTargetCamera`, `SetAngleCamera` |
| Camera globals          | LIB386/H/3D/CAMERA.H       | `CameraX`/`Y`/`Z`, `CameraAlpha`/`Beta`/`Gamma`, `Xp`, `Yp`                         |
| View globals            | SOURCES/3DEXT/VAR_EXT.CPP  | `AlphaCam`, `BetaCam`, `GammaCam`, `VueDistance`, `VueOffsetX`/`Y`/`Z`              |
| CameraCenter            | SOURCES/INTEXT.CPP         | `CameraCenter(flagbeta)`                                                            |
| SearchCameraPos         | SOURCES/3DEXT/MAPTOOLS.CPP | `SearchCameraPos(x, y, z, objbeta, mode)`                                           |
| Exterior init           | SOURCES/EXTFUNC.CPP        | `Init3DExtView`, `Init3DExtGame`                                                    |
| Camera level keys       | SOURCES/EXTFUNC.CPP        | `GereExtKeys` — preset switch (classic) or free `AlphaCam` tilt (auto `FollowCamera`) |
| Main loop follow cam    | SOURCES/PERSO.CPP          | Off-screen check, Enter key recentre; calls into the Auto camera below               |
| Auto camera update      | SOURCES/FOLLOWCAM.CPP      | `UpdateFollowCameraExt`, `FollowCamResetToView`, `FollowCamSyncTarget`; interface in `FOLLOWCAM.H` |
| FollowCamera state      | SOURCES/FOLLOWCAM.CPP      | `FollowCamera`, `FollowCamBaseDist`; `FollowCamEffectiveDist` (spring arm, private to the file) |
| Follow cam tuning       | SOURCES/FOLLOWCAM_CFG.H    | All `FOLLOW_CAM_*` build-time constants                                             |
| Auto cam HD recompose   | SOURCES/FOLLOWCAM.CPP, FOLLOWCAM_CFG.H | `FollowCamHDExcess`, `FollowCamHD{Recompose,PitchGain,DistGain,LeanGain}`, `cam_hd*` cvars |
| Ground/occlusion clearance | SOURCES/FOLLOWCAM.CPP, FOLLOWCAM_CFG.H | `FollowCamEyeLift`, `FollowCamGroundSettling`, `FollowCamGround`, `FollowCamGroundClearance`, `cam_ground*` cvars |
| Orbit gesture state     | SOURCES/EXTFUNC.CPP        | `FollowCamAdoptAngle`, `FollowCamForgetManualGesture`, `ApplyManualCameraNudge`, `cam_glide` |
| Camera zone dispatch    | SOURCES/OBJECT.CPP         | `SetZoneCamera`, `ZONE_ON` / `ZONE_ACTIVE` / `ZONE_OBLIGATOIRE` (COMMON.H), `AllCameras` |
| Obstruction clearance | SOURCES/FOLLOWCAM.CPP, SOURCES/INTEXT.CPP, SOURCES/EXTFUNC.CPP, SOURCES/OBJECT.CPP | `FollowCamNeededPitch`, `FollowCamRenderExtraPitch`, `FollowCamObstructOwnsSearch`, `FollowCamObstructSettle`, `FollowCamObstructHidden`, `cam_obstruct` cvar |
| Camera trace            | SOURCES/FOLLOWCAM.CPP      | `FollowCamTrace`, `camtrace` / `camnudge` console commands |
| A shot's loan | SOURCES/FOLLOWCAM.CPP, SOURCES/OBJECT.CPP | `FollowCamLendToShot`, `FollowCamPlayerTook`, `FollowCamForgetLoan`, `FollowCamInShotBox`, `cam_zone_return` / `cam_zone_anchor` cvars |
| World anchor | SOURCES/FOLLOWCAM.CPP, SOURCES/EXTFUNC.CPP | `FollowCamWorldAnchorStep`, `FollowCamWorldAnchorRebase`, `cam_world_anchor` cvar |
| Angle arithmetic        | SOURCES/FOLLOWCAM_MATH.H   | `FollowCamAngleDiff`, `FollowCamTargetBetaFor`, `FollowCamPanForAngle`, `FollowCamRotStep`, `FollowCamEasePan`, `FollowCamEaseValue`, `FollowCamEaseStepMs`; host test in tests/camera |
| Eases paced by the clock | SOURCES/FOLLOWCAM.CPP, SOURCES/PERSO.CPP | `FollowCamEaseTick`, `FollowCamEaseRestart`, `FollowCamEaseLookAt`, `FollowCamNoteFrame`, `cam_ease_ms` / `cam_target_ms` cvars |
| Manual-override fade    | SOURCES/FOLLOWCAM.CPP, SOURCES/EXTFUNC.CPP | `FollowCamManualHold`, `FollowCamManualHoldFrames`, `autoFactor`, `cam_manual_hold` cvar |
| Config read/write       | SOURCES/PERSO.CPP          | `ReadConfigFile`, `WriteConfigFile`                                                 |
| Menu toggle             | SOURCES/GAMEMENU.CPP       | `GereAdvancedOptionsMenu`                                                           |


## Cross-references

- [CONFIG.md](CONFIG.md) — `FollowCamera` key (Auto camera)
- [MENU.md](MENU.md) — Advanced options menu entry
- [GLOSSARY.md](GLOSSARY.md) — Zone type 1 = camera zone; `AllCameras`
- [LIFECYCLES.md](LIFECYCLES.md) — Scene load phase 6: initialize camera; main loop step 7: `AffScene`
- [CONSOLE.md](CONSOLE.md): `camtrace`, `camnudge`, `zonelist`, and the `cam_*` cvars
- [CONTROL.md](CONTROL.md): driving the camera headlessly

