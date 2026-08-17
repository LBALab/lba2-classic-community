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

The latching shape is by far the more common in the shipped data. `CameraZone` is therefore true for one frame per entry on most zones, and for the whole visit on forced ones. Both are pinned by `test_camzone_model.sh`.

`zonelist` prints `Info7` with its bits named, which is the quickest way to tell which shape a given zone is.

### What a camera zone also switches off

The angle is the visible part, but `CameraZone` gates three other behaviours, and anything that suppresses or bypasses zones inherits all four:

- **The hidden-hero recovery.** When the hero is completely masked by geometry, the engine recentres the camera, unless `CameraZone` is set ([OBJECT.CPP](../SOURCES/OBJECT.CPP)). An authored shot is assumed deliberate, including when it hides him.
- **HD recompose.** Skipped in zones ([FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP)), so hand-composed framing is not adjusted at tall render heights.
- **The recentre input.** `I_RETURN` is a no-op inside a zone.

`FlagCameraForcee` outlives the zone: the off-screen path clears it and restores `AlphaCam` / `GammaCam` / `VueDistance` to the `VueCamera` presets, which is how a forced shot's parameters are given back.

### Interaction with the Auto camera

A zone writes `BetaCam` directly, so it is one of the writers described above and needs `FollowCamAdoptAngle` to keep its angle. With that in place, how long the shot survives is `cam_hold_angle`'s decision, and both answers are intentional:

- **on** (default): the shot is held until the player orbits away or the scene changes, like any angle they set themselves.
- **off**: the pan drift returns the camera behind the hero as he walks on, handing the shot back gradually. This is only possible because the angle was adopted; a shot the Auto camera never took ownership of cannot be given back smoothly, only snatched back.

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

- **Rotation lag:** `BetaCam` lerps toward “behind the hero” with distance-scaled inertia (`FollowCamEffectiveDist` / `FollowCamBaseDist` feeds the divisor) — closer zoom = snappier orbit, longer arm = lazier drift.
- **Pan drift:** `[` / `]` adjust `AddBetaCam`; drift back toward center only while the hero is walking; standing still preserves pan. Disabled by **hold-angle mode** (`cam_hold_angle`, default on): the manual rotation is then held indefinitely, like a free third-person camera, and only re-centers when the scene changes or the player hits Center camera (Enter / gamepad B); both go through `CameraCenter(1)`, which zeroes `AddBetaCam`. `cam_hold_angle 0` restores the classic lazy drift-back.

### Orbit gestures (mouse drag, right stick)

Every analog camera source goes through `ApplyManualCameraNudge` ([EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP)) so they clamp each axis identically. Horizontal orbit is the axis with state attached, and a gesture has three parts:

- **Start.** The first frame of a gesture calls `FollowCamAdoptAngle`, so the stick's delta applies from the angle on screen rather than from wherever the hero-relative target had drifted to. Only the first frame: doing it every frame would halve the stick's travel, since the lerp's steady-state lag is what the following frames are spending.
- **Drive.** Orbiting selects a tight lerp divisor (`cam_smooth`, default 2) instead of the auto-follow's distance-scaled one, and re-arms `FollowCamReengageDelay` each frame. In steady state the camera moves at the speed asked for, lagging the target by twice the per-frame step.
- **Release.** The lerp ramps *up* to the stick's speed over about four frames and has nothing to ramp it back down, so the orbit would otherwise stop dead from full speed. The last speed is decayed by `cam_glide` percent per frame and fed back through the same nudge, spending the tail over several frames. Because it goes through the nudge, `AddBetaCam` moves with it and the angle the camera settles on is the one the follow update recomputes.

`FollowCamForgetManualGesture()` ends a gesture outright, called from the per-frame check for camera zone / cutscene / interior / camera-off and from `CameraCenter`. Without it the tail resumes on a camera the player has since re-aimed, and the stale "gesture under way" flag makes the next nudge skip its realign.

The follow-through is tuned for the stick, which springs back to centre so "no input this frame" reliably means released. A mouse held still reports the same thing while still being driven, which is #514.

**Timing is in frames, not milliseconds.** The lerp divisors and the glide decay are per-frame, so a gesture's ease-in and tail last the same number of frames at any rate and therefore a different wall-clock time: the release tail measures about 112 ms at 60 fps and 198 ms at 30. This is the existing convention rather than something the follow-through introduced, but it means any test asserting a duration has to pin `--fixed-dt`.

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

### Ground / occlusion clearance

A smooth port of the classic `SearchCameraPos` terrain awareness onto the follow path (`cam_ground`, default on; clearance `cam_ground_clear`). After `CameraCenter(3)` positions the eye, a few points are sampled along the eye-to-hero line (`FollowCamHDExcess`-style, in `FOLLOWCAM.CPP`). Where terrain there would rise above the line of sight (a hill between the camera and the hero, or the eye sinking into rising ground), the eye is raised just enough to clear the worst occluder and re-aimed at the hero via the same `SetPosCamera` / `SetTargetCamera` calls the classic recenter uses.

The difference from the classic path is that the lift **eases** toward its target every frame (`FollowCamEyeLift`, tuned by `FOLLOW_CAM_GROUND_*`) instead of snapping. An earlier always-on snap fought the orbit and was reverted; easing is what makes it safe to run every frame. A `FollowCamGroundSettling` flag keeps the dirty check live until the lift converges, so it finishes even while the hero stands still. Active at every resolution (world awareness, not HD-specific), skipped in camera zones and when the eye leaves the cube (where `CalculAltitudeObjet` is invalid). It clears terrain only; decor/scenery occlusion is not yet handled.

### Manual-override fade

The HD recompose steepens pitch and the ground-clearance re-aims the eye, both on top of whatever the player set. Left alone, that fights a manual tilt: the recompose offsets it and the re-aim pins pitch to the geometry. Any manual camera nudge therefore arms `FollowCamManualHold` (in `ApplyManualCameraNudge`), which drives an `autoFactor` to 0 so the **pitch** assist (recompose pitch steepen + ground-lift re-aim) yields completely while the player drives; it eases back to full over `FollowCamManualHoldFrames` (cvar `cam_manual_hold`, ~0.75 s) once they let go. This mirrors the existing azimuth re-center (`FollowCamReengageDelay`) on the pitch axis. The boom pull-in and lean are deliberately **not** faded, so grabbing the stick to orbit does not pop the camera out to full distance.

**Performance:** Two optimizations keep the per-frame cost manageable:

1. **Idle skip:** Dirty check on hero `X`, `Y`, `Z`, `Beta` skips all camera/terrain work when the hero hasn't moved. Standing still has zero extra cost.
2. **Full refresh on movement:** Sets `FirstTime = AFF_ALL_FLIP` for full render + flip in one frame. `AFF_ALL_FLIP` runs the `AffScene` preamble (`MemoClipWindow` / `UnsetClipWindow` / `ClearImpactRain`) preventing black edges. Vsync caps at ~60 Hz. Timer compensation (`SaveTimer`/`RestoreTimer` + `TimerSystemHR`) keeps game logic at correct speed.

See [CONFIG.md](CONFIG.md) for persistence and [MENU.md](MENU.md) for the menu entry.

## Observing and testing the camera

The camera resisted iteration for a long time because nothing about it was asserted: a change was judged by playing, and a regression arrived as a field report. A 75 degree snap from a 1 unit stick touch survived two releases that way.

**`camtrace <0|1>`** logs one line per frame: the angles, how far `BetaCam` moved and who moved it, the re-engage countdown, whether an orbit is driving, and `zone` / `forced` / `cine` / `follow` / `ext` / `vue` / `alpha` / `dist` for which camera holds the view. It is emitted from the main loop rather than from the Auto camera's update, so it reports camera zones, cutscenes, interiors and the classic camera, none of which run that update. `target` is maintained by the Auto camera alone and reads stale when `follow` is 0.

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
| `camzone_model` | the two shapes a camera zone comes in |
| `camzone_hold` | both cameras leave an authored shot in the same place |

Two habits are worth keeping when adding to these. **Run a new fixture against a build without the thing it guards** and confirm it fails there; several of these passed at first because their setup never happened rather than because the engine was right. And **assert the cause when the symptom is unobservable**: the realign runs in the input pass, before the camera update logs anything, so on a working engine the divergence it corrects leaves no trace at all.

### Future work

- **Rendering architecture:** A faster terrain path (GPU or structural changes) would reduce the CPU cost of per-frame `RefreshGrille`.
- **Rebinding:** optional rebinding of zoom/tilt/pan (today numpad-heavy) for laptops and alternate layouts. The right stick already drives orbit and elevation (`cam_stick_*`).
- **Hero-relative hold:** an angle held as an offset from the hero's facing cannot express a held world heading, which is what makes turning while the stick is down still drag the camera. #351 proposes anchoring horizontal rotation to the overworld instead, retiring that whole class rather than correcting instances of it.
- **Mouse follow-through:** the release tail suits a stick that springs back to centre, not a pointer held still (#514).
- **Decor occlusion and clipping:** #363.
- **Auto camera vs terrain / decor:** the eased ground/occlusion clearance above now ports the terrain half of `SearchCameraPos` (an earlier always-on snap was reverted for fighting the orbit; easing fixes that). Still open: decor/scenery occlusion (the classic path also tests `TestZVDecors`), and the eye leaving the cube on far authored cameras (the clearance is skipped there, matching the classic out-of-cube guard).
- **Decor occlusion:** the ground clearance clears terrain only; the classic path also tests scenery boxes (`TestZVDecors`). Porting that would let the camera clear buildings/props too, not just landscape.
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
| Camera trace            | SOURCES/FOLLOWCAM.CPP      | `FollowCamTrace`, `camtrace` / `camnudge` console commands |
| Angle arithmetic        | SOURCES/FOLLOWCAM_MATH.H   | `FollowCamAngleDiff`, `FollowCamTargetBetaFor`, `FollowCamPanForAngle`, `FollowCamRotStep`; host test in tests/camera |
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

