# Camera internals

How the camera works in the engine: the interior and exterior paths, `CameraCenter`, camera zones, and the Auto camera (`FollowCamera`, a community addition). What a player sees and the settings they can change are in [CAMERA.md](CAMERA.md). How the camera is tested is in [TESTING.md](TESTING.md#8-camera-fixtures-testsautomation-local).

## Low-level camera ([LIB386/3D/CAMERA.CPP](../LIB386/3D/CAMERA.CPP))

| Function          | Role                                                                                  |
| ----------------- | ------------------------------------------------------------------------------------- |
| `SetPosCamera`    | Sets `CameraX`/`Y`/`Z` and applies rotation matrix via `SetCamera`                    |
| `SetAngleCamera`  | Sets `CameraAlpha`/`Beta`/`Gamma`, rebuilds `MatriceWorld`, recomputes light          |
| `SetFollowCamera` | Sets target, angle, and zoom; inverse-rotates to compute actual camera world position |
| `SetTargetCamera` | Computes angles to look at a target point from the current camera position            |

All functions live behind `extern "C"` and are shared by both interior and exterior code.

## Interior (isometric)

- Projection: `SetIsoProjection` / `LongProjectPoint`: no perspective divide, fixed camera orientation.
- `PtrProjectPoint` calls `LongProjectPoint` directly ([SOURCES/INTEXT.CPP](../SOURCES/INTEXT.CPP)).
- Camera position is `SetPosCamera(StartXCube * SIZE_BRICK_XZ, ...)`, aligned to the tile grid.
- **Smooth follow:** when the hero goes off-screen, the main loop moves `StartXCube` / `StartZCube` half way toward the hero's brick each frame (`MainLoop` in [SOURCES/PERSO.CPP](../SOURCES/PERSO.CPP)).
- Enter (`I_RETURN`) recentres by snapping to the hero's tile.

## Exterior (perspective)

- Projection: `SetProjection` / `LongWorldRotatePoint` + `LongProjectPoint`: full perspective with far clip, field of view (`ChampX`/`ChampZ`) and fog.
- `PtrProjectPoint` world-rotates, then projects ([SOURCES/INTEXT.CPP](../SOURCES/INTEXT.CPP)).
- The camera is set with `SetFollowCamera(VueOffsetX, VueOffsetY, VueOffsetZ, AlphaCam, BetaCam, GammaCam, VueDistance)`, initialised by `Init3DExtView` ([SOURCES/EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP)).

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

`BetaCam` is a **world** angle: the view's yaw, which everything downstream renders from. `AddBetaCam` is an offset **from the hero's facing**. One rule ties them:

```
BetaCam = (2048 - (AddBetaCam + heroBeta)) & 4095
```

The rule is what the camera is aimed *by*, not an invariant. It holds the instant something aims the camera at the hero and while the Auto camera is converged. The classic camera keeps whatever angle it was left with, and a loaded save restores an angle that need not satisfy it.

In the original game `AddBetaCam` only holds quarter turns: `I_CAMERA` adds 1024, and the Life opcode `LM_CAMERA_CENTER` sets `num * 1024`. The Auto camera reuses it for continuous orbit, so it now holds any value.

The Auto camera re-evaluates the rule every frame it runs, so **code that writes `BetaCam` without also updating `AddBetaCam` creates a difference the Auto camera reads as catch-up owed, and spends.** The hero turning under a held angle (#450), an interrupted orbit and a camera zone's authored angle (#518) have all done this. The rule and its inverse live in [FOLLOWCAM_MATH.H](../SOURCES/FOLLOWCAM_MATH.H). `FollowCamAdoptAngle()` ([EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP)) solves the rule for the current `BetaCam`, so nothing is owed: call it after writing `BetaCam` on any path the Auto camera might run under. `FollowCamForgetManualGesture()` is its companion: it drops an orbit in flight when something else takes the camera.

### Off-screen recenter (original behavior)

When the hero projects outside the screen clip bounds (`MainLoop` in [PERSO.CPP](../SOURCES/PERSO.CPP)):

- **Exterior:** `StartXCube`/`StartZCube` snap to the hero's rotated position, then `CameraCenter(0)` applies it without reorienting `BetaCam`.
- **Interior:** the half-step follow above.

### Manual controls

- **Enter (`I_RETURN`):** `CameraCenter(1)`, a full reorient behind the hero (`MainLoop` in [PERSO.CPP](../SOURCES/PERSO.CPP), which then calls `FollowCamResetToView`).
- **Camera cycle (`I_CAMERA`):** rotates `AddBetaCam` by 1024. Classic: `CameraCenter(2)` (preset `AlphaCam` / `VueDistance`). Auto camera: recomputes `VueOffset*` / `BetaCam` from the hero and `AddBetaCam`, then `CameraCenter(3)`, so zoom and tilt are kept.
- **`GereExtKeys`:** keyboard tilt, zoom and pan ([SOURCES/EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP)).

## CameraCenter ([SOURCES/INTEXT.CPP](../SOURCES/INTEXT.CPP))

The central recentring function. `flagbeta` controls the angle:

| flagbeta | Effect                                                                                                        |
| -------- | ------------------------------------------------------------------------------------------------------------- |
| 0        | Apply `Start*Cube` to `VueOffset*`; **no** angle change                                                       |
| 1        | Reorient behind hero (`BetaCam = 2048 - heroBeta`); reset `AddBetaCam`, `AlphaCam`, `VueDistance` to defaults |
| 2        | Like 1 but preserves `AddBetaCam` (oriented camera cycle)                                                     |
| 3        | No globals modified; just calls `SetFollowCamera` / `SetPosCamera`. Skips `SearchCameraPos`                   |

Then, in an interior, `SetPosCamera` on the tile-grid position. In an exterior, `SetFollowCamera`, then (if `flagbeta != 3` and no `CameraZone`) `SearchCameraPos`; on a hit it overrides with `SetPosCamera` + `SetTargetCamera` and sets `FlagCameraForcee`.

### SearchCameraPos ([SOURCES/3DEXT/MAPTOOLS.CPP](../SOURCES/3DEXT/MAPTOOLS.CPP))

Probes terrain and decors along the camera-to-target line for an unoccluded position, from 1000 out to `VueDistance` in 512-unit steps, checking `CalculAltitudeObjet` and the `NbObjDecors` bounding boxes. Cost is proportional to `(VueDistance - 1000) / 512 * NbObjDecors`.

## Camera zones (authored shots)

A zone of type 1 re-aims the camera when the hero is inside its box. `SetZoneCamera` ([OBJECT.CPP](../SOURCES/OBJECT.CPP)) applies `Info0`–`Info2` as the render anchor (`Start*Cube`) and, outdoors, `Info3`–`Info6` as `AlphaCam` / `BetaCam` / `GammaCam` / `VueDistance`, then raises `CameraZone` and `FlagCameraForcee`. The zone fields and the `Info7` flag word are in [ZONES.md](ZONES.md).

### Two shapes, decided by Info7

The dispatch fires when the hero is `NumObjFollow` and `ZONE_ON` is set, and then only if one of:

| condition | effect |
| --- | --- |
| `ZONE_OBLIGATOIRE` | fires **every frame** the hero is inside: the shot holds the view while he stays (a *forced* or holding zone) |
| `CubeMode == CUBE_INTERIEUR` | same, every frame: this is how interior cameras work at all |
| `!ZONE_ACTIVE AND AllCameras` | fires **once**, then latches `ZONE_ACTIVE` (a *latching* zone) |

`ZONE_ACTIVE` is cleared when the hero leaves the box, so re-entering re-arms the zone; a Life script can also clear it ([GERELIFE.CPP](../SOURCES/GERELIFE.CPP)). Of about 240 enabled exterior camera zones, a little over half latch and the rest are forced. The GOG and US disc data differ in the flags of three zones, two of them in cube 42, so check a zone named by cube and index against the data being played. `zonelist` prints `Info7` with its bits named.

### What a camera zone also switches off

`CameraZone` gates three behaviours besides the angle, and anything that suppresses or bypasses zones inherits all of them:

- **The hidden-hero recovery.** When the hero is fully masked, the engine recentres, unless `CameraZone` is set ([OBJECT.CPP](../SOURCES/OBJECT.CPP)): an authored shot is assumed deliberate.
- **The follow update, for that frame.** `UpdateFollowCameraExt` runs inside `if (!CameraZone)` ([PERSO.CPP](../SOURCES/PERSO.CPP)), so on the frame a zone claims the view its writes reach the screen untouched. The HD recompose still applies on every later frame the shot holds.
- **The recentre input.** `I_RETURN` does nothing inside a zone.

`FlagCameraForcee` outlives the zone: the off-screen path clears it and restores `AlphaCam` / `GammaCam` / `VueDistance` to the `VueCamera` presets, which is how the classic camera gives a forced shot's parameters back.

### Interaction with the Auto camera

A zone writes `BetaCam` directly, so it calls `FollowCamAdoptAngle` to keep its angle. Four settings decide what a shot does to the Auto camera, each described below: `cam_zone_skip` (a latching shot is passed over while the player steers), `cam_zone_return` (the angle and pitch are given back on leaving the box), `cam_shot_blend_ms` (taking the view and giving it back are blends) and `cam_scene_carry` (under [Scene changes](#scene-changes)).

With those off, what survives a shot is `cam_hold_angle`'s decision: on, the shot's angle is held until the player orbits or a recentre; off, the pan drift hands it back as the hero walks. What is held is the offset, not the shot: the zone's world yaw becomes a pan from the hero's facing on the frame it fired, so his first turn loses the authored direction. The pitch stays as authored, because the exterior Auto camera skips the path that restores the presets.

### A shot as a loan

`cam_zone_return` (cfg `FollowCamZoneReturn`, on by default) treats a shot as something the Auto camera lends the view to and gets back:

- When the hero leaves the box of the shot that fired, the yaw pan and the pitch ease back to what the Auto camera had before the shot, at the pan drift's pace (`cam_recenter_drift`), whether or not he walks. The return starts from the angle on screen, so leaving a forced zone is one motion.
- An axis the player sets by hand while the shot has the camera is theirs and is not given back: an orbit or the quarter-turn key for the yaw, a tilt for the pitch.
- A second shot firing before the first is returned changes what is on screen, not what is owed.
- The loan ends without a return when something else decides where the camera points: Center camera and every `CameraCenter(1)` or `(2)` (the hidden-hero recovery and a script's camera order included), a cutscene, the Auto camera switched off, a load.
- Straight out of a cutscene, a shot that fires as control returns owes back "behind the hero" for the yaw (`FollowCamNoteCinema()`); the ferry's arrival on the Desert Island is one.
- A visit to an interior does not end it: a door's `CameraCenter(0)` leaves pan and pitch alone. What was owed going in is owed coming out, except that the yaw becomes "behind the hero", since the hero comes out facing the other way and the old pan would put the camera beside or in front of him. Most interior-to-exterior doors land inside a camera zone, so this is the common case.
- What is owed for the pitch is the Auto camera's own pitch, `FollowCamOwnAlpha`, not `AlphaCam` as it stands: a zone and `CameraCenter`'s obstruction search both leave a pitch in `AlphaCam` that is neither the player's nor the camera's rest. `FollowCamOwnAlpha` is written in three places only: the player tilting, a recentre setting its preset before the search runs, and a load.

`cam_zone_anchor` (cfg `FollowCamZoneAnchor`, prototype, off) holds a latching zone's world yaw while the hero is inside its box, by re-solving the pan against his facing every frame, as forced zones already behave. It ends at the box edge and gives nothing back.

Knowing the hero is inside a latching zone's box needs more than `CameraZone`, which is true only on the frame the shot is applied. `CheckZoneSce` sets `FollowCamInShotBox` on each frame the followed object stands in the box of a camera zone that has fired; `camtrace` reports it as `inshot`, beside `loan` (1 yaw, 2 pitch, 4 on its way back).

### Passing a latching shot over

`cam_zone_skip` (cfg `FollowCamZoneSkip`, on by default): the dispatch in `CheckZoneSce` passes a latching zone over while the Auto camera is on, the hero's `Move` is `MOVE_MANUAL` and no cutscene runs. Forced zones, interiors, the classic camera and a hero a script is moving are as before. A zone passed over is not latched, so it fires if a script takes the hero over inside the box, and it still shadows the camera zones after it in the list.

Everything `CameraZone` gates stays on, so a shot authored for a spot where the hero is hidden no longer covers it and the hidden-hero recovery fires there instead. A door that lands in a latching zone becomes an arrival with no shot; `cam_scene_carry` makes that arrival clean.

### Blending into and out of a shot

`cam_shot_blend_ms` (cfg `FollowCamShotBlendMs`, default 600, 0 for the cut) blends the frame from the view on screen to the new one over that many milliseconds, eased in and out. Both ends are named moments, not guessed from the size of a jump: the zone about to write its shot (`FollowCamShotTookView()`, from `SetZoneCamera`), and the first Auto camera update after one. Each parameter (look-at point, distance, yaw, pitch, roll) blends on its own, the angles the short way round, toward the view as it is on each frame, since the Auto camera's own view keeps moving.

The blend is in the frame only. `FollowCamSetView()` sets the exterior camera for `CameraCenter` and the scene render from the globals, or from the blend while one runs; the globals are what they would be without it. The scene is drawn whole on each frame of a blend.

Code that borrows the 3D camera mid-frame (a 3D icon such as the holomap globe, the inventory, the behaviour menu) puts it back with `Init3DExtView`, which applies the view the last scene render showed (`FollowCamApplyShownView()`), not the globals. Above 60 fps a frame with no simulation step does not redraw the terrain, so the actors drawn on it must use the camera the terrain was drawn with; the globals hold the shot's end view during a blend and leave out `cam_obstruct`'s extra pitch.

- **A hero hidden mid-blend is not answered.** The hidden-hero recovery and `cam_obstruct`'s reply wait for the blend to end.
- **It never runs across a cut.** A blend starts only from a view the Auto camera showed of the same scene with nothing else having had the view since, and ends at once at Center camera, every `CameraCenter(1)` or `(2)`, the quarter-turn key, a scene change, a building, a cutscene, a load or the Auto camera switched off. A shot over a door that fires on arrival is therefore a cut; walking out of it blends.
- **A sign's shot is shown at once.** A sign that names a camera zone (`GereZoneMessage`) applies the shot, draws one frame and holds it still under the message. A blend in would freeze its first instant, so that shot is applied whole (`FollowCamShotAtOnce()`); closing the message blends back out.
- **From one shot straight into another**, the blend runs between the two shots' views. Between zones 31 and 32 in cube 90 (US disc) that is a 175-degree swing in 0.6 s with the hero hidden for 0.3 s in the middle, which the recentre leaves alone until the blend ends; played and kept.
- **A shot that fires once** (under `cam_zone_skip 0`, or for a scripted hero) has one frame, so the blend runs from the view before it toward the Auto camera's, which has adopted the shot's yaw.
- A console `teleport` within a scene is not a cut.

## Auto camera (`FollowCamera`)

Config key `FollowCamera` (0 = classic, 1 = auto; default 0, legacy key `AutoCameraCenter` also read), toggled in Options → Advanced options. When on, outdoors and outside camera zones and cutscenes, it is a third-person follow. Tuning constants are in `FOLLOWCAM_CFG.H`, logic in [SOURCES/FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP) and `EXTFUNC.CPP`.

### Spring arm (zoom)

- **`FollowCamBaseDist`:** the target distance (numpad `/` closer, `*` farther, mouse wheel; clamped `FOLLOW_CAM_DIST_MIN`–`FOLLOW_CAM_DIST_MAX`).
- **`FollowCamEffectiveDist`:** the smoothed length, private to `FOLLOWCAM.CPP`, moving toward `FollowCamBaseDist` by `FOLLOW_CAM_SPRING_RECOVER` per frame either way.
- **`VueDistance`** is set from `FollowCamEffectiveDist` before `CameraCenter(3)`, so the terrain render matches the eased distance.

The zoom is per session: `FollowCamBaseDist` is neither persisted nor a cvar. Only the first-frame init and Center camera write the resting value (`FOLLOW_CAM_INITIAL_DIST`, the midpoint of the two `DefVueDistance` presets).

### Lazy orbit and pan

- **Rotation lag:** `BetaCam` eases toward behind the hero, paced by `cam_ease_ms` (see [Eases paced by the clock](#eases-paced-by-the-clock)).
- **Pan:** `[` / `]` adjust `AddBetaCam`. With `cam_hold_angle` (default on) the pan is held until Center camera or an ordered scene change, both through `CameraCenter(1)`. With `cam_hold_angle 0` it drifts back toward centre while the hero walks, and is kept while he stands.

### World anchor (prototype)

`cam_world_anchor` (cfg `FollowCamWorldAnchor`, off) changes what the horizontal angle is held against. Off, the camera aims behind the hero offset by `AddBetaCam`, so his turns turn the camera. On, each update takes the hero's turn since the previous update back out of `AddBetaCam`, leaving the target fixed in the world. Whatever else wrote the offset (an orbit, `[` / `]`, the quarter-turn key, a script) is kept.

| | `cam_hold_angle 0` | `cam_hold_angle 1` |
|---|---|---|
| anchor off | back behind him on the clock, following every turn | the offset kept, turning with him |
| anchor on | back behind him by the ground he walks | a free camera that only the player turns |

With the anchor on and `cam_hold_angle 0`, the pan drift is replaced by a pull paced by distance: each update the pan closes on behind him by the ground covered since the last one. `cam_world_return` (cfg `FollowCamWorldReturn`, 700, 0 for none) is that distance in world units, about 95% of the way round after three of them. Turning on the spot, standing and walking backwards do not pull; sliding along a wall does. The pull waits out the grace after the player moved the camera, and stands down while a shot's yaw is owed and across a teleport or a scene seam.

The turn is taken out only across consecutive updates: after a frame without one (a holding shot, a cutscene, a building) or a cut, the offset is taken as it stands. `FollowCamAdoptAngle` tells the anchor the turn so far is already in the offset. With `cam_zone_return` on, what a shot owes for the yaw is a world direction too, so leaving the box restores where the camera faced before the shot. Inside a latching shot's box the anchor holds the shot's angle, as `cam_zone_anchor` does.

### Orbit gestures (mouse drag, right stick)

Every analog source goes through `ApplyManualCameraNudge` ([EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP)), which clamps each axis identically. A horizontal orbit has three parts:

- **Start.** The first frame of a gesture calls `FollowCamAdoptAngle`, so the delta applies from the angle on screen. Only the first frame: every frame would halve the stick's travel.
- **Drive.** Orbiting uses a tight lerp divisor (`cam_smooth`, default 2) and re-arms `FollowCamReengageDelay` each frame.
- **Release.** The last speed decays by `cam_glide` percent per frame and is fed back through the same nudge, so the orbit does not stop dead and `AddBetaCam` moves with it.

`FollowCamForgetManualGesture()` ends a gesture outright, from the per-frame check for camera zone, cutscene, interior or camera-off, and from `CameraCenter`. The follow-through suits a stick, which springs back to centre; a mouse held still reports the same "no input" while still driven (#514).

The orbit lerp, the glide decay and the pan drift are a share per call of `UpdateFollowCameraExt`, which runs inside the fixed simulation step ([MOVEMENT_FRAMERATE.md](MOVEMENT_FRAMERATE.md); `FixedTimestep`, 16 ms by default), not once per rendered frame. With the step on they take the same time at any frame rate; with `fixedtimestep off` they run once per frame.

### Eases paced by the clock

Two settings pace an ease by the game clock (`TimerRefHR`, which stands still in a menu and is what a replay reproduces), using the time since the previous update (`FollowCamEaseTick()`):

- **`cam_ease_ms`** (cfg `FollowCamEaseMs`, default 300): the time constant of the turn behind the hero. 300 is the 2022 Classic release's pace (a quarter turn within 5% in about 0.9 s); 600 matches the per-update lerp at the default step; 0 goes back to that lerp. Unlike the lerp, which stops inside a 10 unit snap threshold, the timed ease lands on its target. A player-driven orbit keeps `cam_smooth`.
- **`cam_target_ms`** (cfg `FollowCamTargetMs`, default 300): the camera looks at a spot `DISTANCE_VISEE` ahead of the hero; the look-at point trails that spot and closes on it with this time constant (`FollowCamEaseLookAt()`). 0 pins it.

The step is `FollowCamEaseStepMs()` in [FOLLOWCAM_MATH.H](../SOURCES/FOLLOWCAM_MATH.H): a share of `2dt / (2tau + dt)` of what is left, which follows `1 - exp(-dt / tau)` within one percent while an update is under a third of the time constant and cannot overshoot until an update is twice it. The move is worked in thousandths of a unit, with the remainder carried to the next update.

Three rules keep time that was not a frame from turning into a jump:

- **A step without the update restarts the clock.** `FollowCamNoteFrame()` counts simulation steps; an update that does not follow one on the previous step moves nothing. A camera zone, a cutscene or a building may have held the view for any length of time.
- **A cut restarts it too:** `FollowCamSyncTarget()` and `FollowCamForgetLoan()`, which cover Center camera, every `CameraCenter(1)` and `(2)`, the quarter-turn key, an arrival under `cam_scene_carry` and a load. The look-at point goes back onto its spot at the same moments.
- **A long update is paced as 100 ms** (`FOLLOW_CAM_EASE_DT_MAX`).

What `cam_target_ms` keeps is how far the look-at point trails, not where it is: walking across a scene seam renumbers the world without moving the hero on screen. A spot that moved more than `FOLLOW_CAM_TARGET_JUMP` in one update was not walked there, and the trail is held for that update.

### Apply path

The Auto camera applies its view with `CameraCenter(3)`: current globals only, without the classic `SearchCameraPos` snap. Terrain is cleared by the eased ground clearance and scenery by `cam_obstruct`, both below.

- **Tilt:** the Camera-level inputs (`I_CAMERA_LEVEL_PLUS` / `I_CAMERA_LEVEL_MOINS`, numpad `+` / `-` and Page Up / Page Down by default) move `AlphaCam` freely in 150–600 instead of switching between the two `VueCamera` presets, every frame while held. Being an input action, it follows rebinding.
- **Zoom and pan** read raw scancodes through `CheckKey` in `GereExtKeys`, so they cannot be rebound.

### HD recompose (tall render heights)

The exterior projection has a fixed focal (`ChampX`/`ChampZ` = 600), so a taller framebuffer shows more vertical field of view instead of zooming: the frame fills with sky and the hero shrinks ([WIDESCREEN.md](WIDESCREEN.md), "Vertical framing at HD"). With `k = ModeDesiredY / 480`, each apply pulls the boom in, steepens the pitch and shortens the forward lean in proportion to `k - 1`, so every term is a no-op at 480.

- **Render-time only.** The player's `AlphaCam` and `FollowCamBaseDist` are adjusted for the `CameraCenter(3)` call and restored.
- **Auto path only**, never in a camera zone, the classic camera or an interior.
- **Tunable live:** `cam_hd` (master), `cam_hd_pitch`, `cam_hd_dist`, `cam_hd_lean`, and `cam_hd_cap`, which caps the strength so tall resolutions converge. `FollowCamHDExcess()` ([FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP)) returns the excess the apply path scales by.

Pitch saturates at the `AlphaCam` clamp (600), leaving a widescreen horizon band on open vistas. Tuned for about 720p; at 1080p the strength over-reaches and terrain near-clipping shows (a per-cell near-plane clip of the terrain grid is the real fix). The pitch steepen does not reach the frame today; see [Open work](#open-work).

### Ground clearance

A smooth port of `SearchCameraPos`'s terrain check (`cam_ground`, default on; clearance `cam_ground_clear`). After `CameraCenter(3)` places the eye, points along the eye-to-hero line are sampled; where terrain rises above the line of sight, the eye is raised just enough and re-aimed at the hero with `SetPosCamera` / `SetTargetCamera`. The lift eases (`FollowCamEyeLift`, `FOLLOW_CAM_GROUND_*`) instead of snapping, and `FollowCamGroundSettling` keeps the dirty check live until it converges. It runs at every resolution, and is skipped in camera zones and when the eye leaves the cube, where `CalculAltitudeObjet` is invalid. It clears terrain only; scenery is `cam_obstruct`'s.

### Obstruction clearance

The classic camera asks `SearchCameraPos` once per recentre how steeply to look down to clear terrain, scenery boxes and scripted objects, writes the answer into `AlphaCam`, and gives it back at its next off-screen recentre. The Auto camera has that recentre switched off, so without help the pitch would stay as though the player had set it.

`cam_obstruct` (cfg `FollowCamObstruct`, on by default) runs the same search on every update and keeps the answer out of `AlphaCam`. `FollowCamNeededPitch()` runs `SearchCameraPos` with the globals it writes saved and restored. What the place needs above the player's pitch is an extra, `FollowCamObstructExtra`, that eases toward it up and down (`FOLLOW_CAM_OBSTRUCT_*`), so there is never a pitch to give back. It reaches the frame through `FollowCamRenderExtraPitch()`, which `CameraCenter` and the scene render add to `AlphaCam` where they set the exterior camera.

What stands down while it is on and the Auto camera has the view outdoors:

- **`CameraCenter`'s own search** no longer writes `AlphaCam` or raises `FlagCameraForcee`. After a re-aim (flags 1 and 2) or a `cam_scene_carry` arrival the extra starts settled; after a plain re-apply (flag 0, a scene seam) it eases.
- **The hidden-hero recentre**, which would zero the player's pan. A hero drawn hidden while the extra is still rising makes it hurry (`FollowCamObstructHidden()`), by `cam_obstruct_hurry` (cfg `FollowCamObstructHurryDivisor`: the ease divisor on those frames, 1 a cut, 10 the ordinary ease and the default). If the extra has given all the search asked for and he is still hidden, the search cannot see what is in the way: the extra rises by `FOLLOW_CAM_OBSTRUCT_HIDDEN_STEP` per such frame and is held as a floor until he has walked `FOLLOW_CAM_OBSTRUCT_FLOOR_REACH` away. Only when no pitch is left does the classic recentre run.

Inside the box of a shot that has fired the search is not asked, as the classic camera never searches inside a zone, but a hidden hero is still answered through the floor. A shot starts as written: the extra and floor are dropped when it is applied (`FollowCamObstructShotFired()`). None of this applies in a cutscene.

Only the pitch is taken from the search: the classic search also moves the target in toward the hero, and this does not shorten the boom. It sees what the search sees, boxes sampled every 512 units, so scenery the boxes miss still hides the hero until the floor rises.

### Manual-override fade

The HD recompose's pitch and the ground clearance's re-aim would fight a manual tilt. Any manual nudge arms `FollowCamManualHold` (in `ApplyManualCameraNudge`), which drives an `autoFactor` to 0 so the pitch assist yields while the player drives, then eases back over `FollowCamManualHoldFrames` (`cam_manual_hold`, default 45 frames, bounded to 600 because the fade multiplies the counter by 100). The boom pull-in and lean are not faded, so grabbing the stick does not pop the camera out.

### Performance

- **Idle skip:** a dirty check on hero `X`, `Y`, `Z`, `Beta`, the base distance and `AlphaCam` skips the camera and terrain work while nothing moves.
- **Full refresh on movement:** `FirstTime = AFF_ALL_FLIP` renders and flips the whole frame, through the `AffScene` preamble (`MemoClipWindow` / `UnsetClipWindow` / `ClearImpactRain`) that prevents black edges. A blend forces it on every frame.

### Scene changes

How the hero reaches an outdoor scene decides what `ChangeCube` ([OBJECT.CPP](../SOURCES/OBJECT.CPP)) does to the camera:

| Arrival | `ChangeCube` calls | What the Auto camera is left with | With `cam_scene_carry` |
| --- | --- | --- | --- |
| Walking across the seam between two outdoor scenes | `CameraCenter(0)` | Pan, pitch and yaw as they were | The same |
| A change ordered by a script or the console (`FlagChgCube` 2 or 0) | `CameraCenter(1)` | The pan zeroed; the pitch at the preset, or at the obstruction search's answer | Between two outdoor scenes, pan and pitch kept and the camera behind the hero at that pan. From an interior, as for a door |
| A door out of an interior | `CameraCenter(0)` | The pan kept and the yaw where the last outdoor scene left it, so the camera starts half a turn from its target and swings round; the pitch rewritten when the search finds the building | The pan sent home and the pitch left alone, the camera behind the hero from the first frame |

`cam_scene_carry` (cfg `FollowCamSceneCarry`, on by default): `FollowCamCarryIntoScene()` ([FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP)) takes the second and third arrivals in place of `ChangeCube`'s centring, except in a cutscene. The pan holds between outdoor scenes and goes home after an interior, where it was set for a view the hero left at the door. The camera is aimed behind the hero at that pan with the lerp's target on it, and the re-engage countdown and any orbit in flight are dropped. A camera zone where the hero lands decides the view either way.

It does not stop an ordered change into an interior from recentring, and it carries a pitch a camera zone left like any other. Where the spot behind the hero is inside the building he just left, he is drawn hidden on the arrival frame and the hidden-hero recentre fires; `cam_obstruct` is the answer to that.

## Tuning settings

The behaviour switches are listed in [CAMERA.md](CAMERA.md#settings). These tune them; all are console cvars with an `lba2.cfg` key, defined in the settings table in [FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP) (`FollowCamSettings`).

| Console | cfg key | Default | Tunes |
| --- | --- | --- | --- |
| `cam_recenter_delay` | `FollowCamReengageFrames` | 60 | frames before the camera recentres after a manual pan |
| `cam_recenter_drift` | `FollowCamDriftDivisor` | 24 | pan drift divisor (higher is gentler); also paces `cam_zone_return` |
| `cam_world_return` | `FollowCamWorldReturn` | 700 | `cam_world_anchor` with `cam_hold_angle 0`: units walked that pace the return |
| `cam_obstruct_hurry` | `FollowCamObstructHurryDivisor` | 10 | `cam_obstruct`: ease divisor while the hero is hidden (1 = cut) |
| `cam_hd_cap` | `FollowCamHDExcessCap` | 0 | cap on the recompose's strength (0 = none) |
| `cam_hd_pitch` | `FollowCamHDPitchGain` | 180 | recompose pitch, `AlphaCam` units per 1.0 of `k - 1` |
| `cam_hd_dist` | `FollowCamHDDistGain` | 27 | recompose boom pull, % per 1.0 of `k - 1` |
| `cam_hd_lean` | `FollowCamHDLeanGain` | 45 | recompose forward-lean cut, % per 1.0 of `k - 1` |
| `cam_ground_clear` | `FollowCamGroundClearance` | 600 | keep the sight line this far above terrain |
| `cam_manual_hold` | `FollowCamManualHoldFrames` | 45 | frames the pitch assist yields after a manual nudge |

The mouse and stick settings (`cam_mouse*`, `cam_stick*`, `cam_smooth`, `cam_glide`) are in [CONTROLLER.md](CONTROLLER.md).

## Rules for changing the camera

- **Adopt after writing `BetaCam`.** Any path the Auto camera may run under calls `FollowCamAdoptAngle()` after writing `BetaCam` (see [AddBetaCam](#addbetacam-and-why-the-camera-has-two-owners)).
- **Drop what latches, leave what eases.** At a discontinuity (a scene change, a camera zone, a recentre), state that converges by itself (`FollowCamEyeLift`, the spring arm) is left alone: carrying a stale value costs a short glide, zeroing it costs a jump. State that does not converge (`FollowCamTargetBeta`, the re-engage and manual-hold counters, the orbit follow-through) must be reset, or it discharges against a situation it was never measured in.
- **Render-time pitch goes through `FollowCamRenderExtraPitch()`.** `AffGrilleExt` ([EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP)) sets the camera again from the globals before it draws, so a change applied around the update's `CameraCenter(3)` and put back never reaches the frame.
- **A new `cam_` setting says what it was before it.** Its row ends `SETTING_BEFORE(v)` when it switches a behaviour on (`v` switches it off), or `SETTING_BEFORE_NONE` when it only tunes another; `make settings-before-check` refuses a row that says neither. With every new behaviour off, the camera's state matches the build without it frame for frame, and the fixtures check that.
- **Check at 480 and at a tall height.** Every recompose term is a no-op at 480, so a change measured at only one height is half measured.

## Open work

- **Render-time adjustments that do not reach the frame:** the recompose's pitch steepen and the ground lift are applied around the update's `CameraCenter(3)` and overwritten when `AffGrilleExt` sets the camera from the globals. Making them live changes what every player at a tall resolution sees, so it is a decision of its own.
- **1080p framing and terrain near-clipping**, and widening the manual `AlphaCam` / `FollowCamBaseDist` clamps with `k` for fine control at tall heights.
- **Scenery occlusion and clipping:** #363.
- **Mouse follow-through:** #514.
- **A world-anchored camera by default:** #351; `cam_world_anchor` is its prototype.
- **Rebinding zoom and pan**, which read raw scancodes.
- **A faster terrain path**, to cut the CPU cost of the per-frame `RefreshGrille`.

## Code reference

| Concept                 | File                       | Symbol                                                                              |
| ----------------------- | -------------------------- | ----------------------------------------------------------------------------------- |
| Low-level camera        | LIB386/3D/CAMERA.CPP       | `SetCamera`, `SetFollowCamera`, `SetPosCamera`, `SetTargetCamera`, `SetAngleCamera` |
| Camera globals          | LIB386/H/3D/CAMERA.H       | `CameraX`/`Y`/`Z`, `CameraAlpha`/`Beta`/`Gamma`, `Xp`, `Yp`                         |
| View globals            | SOURCES/3DEXT/VAR_EXT.CPP  | `AlphaCam`, `BetaCam`, `GammaCam`, `VueDistance`, `VueOffsetX`/`Y`/`Z`              |
| CameraCenter            | SOURCES/INTEXT.CPP         | `CameraCenter(flagbeta)`                                                            |
| SearchCameraPos         | SOURCES/3DEXT/MAPTOOLS.CPP | `SearchCameraPos(x, y, z, objbeta, mode)`                                           |
| Exterior init           | SOURCES/EXTFUNC.CPP        | `Init3DExtView`, `Init3DExtGame`                                                    |
| Camera level keys       | SOURCES/EXTFUNC.CPP        | `GereExtKeys`: preset switch (classic) or free `AlphaCam` tilt (Auto camera) |
| Main loop follow cam    | SOURCES/PERSO.CPP          | Off-screen check, Enter key recentre; calls into the Auto camera below               |
| Auto camera update      | SOURCES/FOLLOWCAM.CPP      | `UpdateFollowCameraExt`, `FollowCamResetToView`, `FollowCamSyncTarget`; interface in `FOLLOWCAM.H` |
| FollowCamera state      | SOURCES/FOLLOWCAM.CPP      | `FollowCamera`, `FollowCamBaseDist`; `FollowCamEffectiveDist` (spring arm, private to the file) |
| Follow cam tuning       | SOURCES/FOLLOWCAM_CFG.H    | All `FOLLOW_CAM_*` build-time constants                                             |
| Auto cam HD recompose   | SOURCES/FOLLOWCAM.CPP, FOLLOWCAM_CFG.H | `FollowCamHDExcess`, `FollowCamHD{Recompose,PitchGain,DistGain,LeanGain}`, `cam_hd*` cvars |
| Ground clearance        | SOURCES/FOLLOWCAM.CPP, FOLLOWCAM_CFG.H | `FollowCamEyeLift`, `FollowCamGroundSettling`, `FollowCamGround`, `FollowCamGroundClearance`, `cam_ground*` cvars |
| Orbit gesture state     | SOURCES/EXTFUNC.CPP        | `FollowCamAdoptAngle`, `FollowCamForgetManualGesture`, `ApplyManualCameraNudge`, `cam_glide` |
| Camera zone dispatch    | SOURCES/OBJECT.CPP         | `SetZoneCamera`, `ZONE_ON` / `ZONE_ACTIVE` / `ZONE_OBLIGATOIRE` (COMMON.H), `AllCameras` |
| Obstruction clearance | SOURCES/FOLLOWCAM.CPP, SOURCES/INTEXT.CPP, SOURCES/EXTFUNC.CPP, SOURCES/OBJECT.CPP | `FollowCamNeededPitch`, `FollowCamRenderExtraPitch`, `FollowCamObstructOwnsSearch`, `FollowCamObstructSettle`, `FollowCamObstructHidden`, `cam_obstruct` cvar |
| Camera trace            | SOURCES/FOLLOWCAM.CPP      | `FollowCamTrace`, `camtrace` / `camnudge` console commands |
| A shot's loan | SOURCES/FOLLOWCAM.CPP, SOURCES/OBJECT.CPP | `FollowCamLendToShot`, `FollowCamPlayerTook`, `FollowCamForgetLoan`, `FollowCamInShotBox`, `cam_zone_return` / `cam_zone_anchor` cvars |
| Blending into and out of a shot | SOURCES/FOLLOWCAM.CPP, SOURCES/OBJECT.CPP, SOURCES/INTEXT.CPP, SOURCES/EXTFUNC.CPP, SOURCES/PERSO.CPP | `FollowCamSetView`, `FollowCamShotTookView`, `FollowCamShotAtOnce`, `FollowCamBlending`, `FollowCamBlendFromShown`, `FollowCamBlendCut`, `cam_shot_blend_ms` cvar |
| World anchor | SOURCES/FOLLOWCAM.CPP, SOURCES/EXTFUNC.CPP | `FollowCamWorldAnchorStep`, `FollowCamWorldReturnStep`, `FollowCamWorldAnchorRebase`, `cam_world_anchor` / `cam_world_return` cvars |
| Angle arithmetic        | SOURCES/FOLLOWCAM_MATH.H   | `FollowCamAngleDiff`, `FollowCamTargetBetaFor`, `FollowCamPanForAngle`, `FollowCamRotStep`, `FollowCamEasePan`, `FollowCamEaseValue`, `FollowCamEaseStepMs`, `FollowCamBlendWeight`, `FollowCamBlendValue`, `FollowCamBlendAngle`; host test in tests/camera |
| Eases paced by the clock | SOURCES/FOLLOWCAM.CPP, SOURCES/PERSO.CPP | `FollowCamEaseTick`, `FollowCamEaseRestart`, `FollowCamEaseLookAt`, `FollowCamNoteFrame`, `cam_ease_ms` / `cam_target_ms` cvars |
| Manual-override fade    | SOURCES/FOLLOWCAM.CPP, SOURCES/EXTFUNC.CPP | `FollowCamManualHold`, `FollowCamManualHoldFrames`, `autoFactor`, `cam_manual_hold` cvar |
| Settings table          | SOURCES/FOLLOWCAM.CPP, SOURCES/SETTINGS.H | `FollowCamSettings`, `SETTING_BEFORE`, `SETTING_BEFORE_NONE` |
| Config read/write       | SOURCES/PERSO.CPP          | `ReadConfigFile`, `WriteConfigFile`                                                 |
| Menu toggle             | SOURCES/GAMEMENU.CPP       | `GereAdvancedOptionsMenu`                                                           |

## Cross-references

- [CAMERA.md](CAMERA.md): the player's guide, controls and the behaviour settings.
- [ZONES.md](ZONES.md): camera zone fields and the `Info7` flag word.
- [CONTROLLER.md](CONTROLLER.md): the manual camera's input sources.
- [CONSOLE.md](CONSOLE.md): `camtrace` and its fields, `camnudge`, `zonelist`, `settings_before`.
- [TESTING.md](TESTING.md): the camera fixtures.
- [CONTROL.md](CONTROL.md): driving the camera headlessly.
- [CONFIG.md](CONFIG.md): the `FollowCamera` key.
- [LIFECYCLES.md](LIFECYCLES.md): scene load phase 6 initialises the camera; main loop step 7 is `AffScene`.
