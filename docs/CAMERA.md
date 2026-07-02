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
- **Smooth follow:** When the hero goes off-screen, the main loop interpolates `StartXCube` / `StartZCube` toward the hero's brick with `xm + (xm - StartXCube) / 2` ([SOURCES/PERSO.CPP](../SOURCES/PERSO.CPP) lines 1621–1626). This half-step convergence keeps scrolling gentle.
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
| `AddBetaCam`     | Player camera-cycle offset (0/1024/2048/3024) | 0; cycled by `I_CAMERA` in steps of 1024     |


### Off-screen recenter (original behavior)

When the hero projects outside the screen clip bounds ([PERSO.CPP](../SOURCES/PERSO.CPP) lines 1609–1656):

- **Exterior:** `StartXCube`/`StartZCube` snap to the hero's rotated position (no interpolation). Then `CameraCenter(0)` applies the new position without reorienting `BetaCam`.
- **Interior:** uses half-step interpolation (see above).

### Manual controls

- **Enter (`I_RETURN`):** calls `CameraCenter(1)` — full reorient behind the hero ([PERSO.CPP](../SOURCES/PERSO.CPP) lines 1386–1408).
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

## Auto camera (`FollowCamera` — community addition, not in the original game)

Config key `FollowCamera` (0 = classic, 1 = auto; default 0). Also reads legacy key `AutoCameraCenter` for backward compatibility. Toggled in Options → Advanced options ("Auto camera" / "Classic camera" — localized). Off by default so the original camera behavior is preserved.

When enabled in exterior mode (and not in a camera zone or cinema), the implementation is a third-person follow with several coupled pieces (tuning in `FOLLOWCAM_CFG.H`, logic in `PERSO.CPP` / `EXTFUNC.CPP`):

### Spring arm (zoom)

Zoom is not a single distance: the player sets a target arm length, and the camera smoothly converges toward it each frame — a standard spring-arm pattern:

- **`FollowCamBaseDist`** — target distance (numpad `/` closer, `*` farther; clamped `FOLLOW_CAM_DIST_MIN`–`FOLLOW_CAM_DIST_MAX`).
- **`FollowCamEffectiveDist`** — smoothed length (static state in `PERSO.CPP`); moves toward `FollowCamBaseDist` by `FOLLOW_CAM_SPRING_RECOVER` per frame whether the arm is too short or too long.
- **`VueDistance`** is set from **`FollowCamEffectiveDist`** before `CameraCenter(3)`, so terrain render matches the eased distance.

Branch history tried heavier correction (terrain penetration along the boom, LOS samples); those were removed to keep behavior predictable and the PR focused — so there is no lens pull-through-terrain and no classic `SearchCameraPos` on this path (see below).

### Lazy orbit and pan

- **Rotation lag:** `BetaCam` lerps toward “behind the hero” with distance-scaled inertia (`FollowCamEffectiveDist` / `FollowCamBaseDist` feeds the divisor) — closer zoom = snappier orbit, longer arm = lazier drift.
- **Pan drift:** `[` / `]` adjust `AddBetaCam`; drift back toward center only while the hero is walking; standing still preserves pan. Disabled by **hold-angle mode** (`cam_hold_angle`, default on): the manual rotation is then held indefinitely, like a free third-person camera, and only re-centers when the scene changes or the player hits Center camera (Enter / gamepad B); both go through `CameraCenter(1)`, which zeroes `AddBetaCam`. `cam_hold_angle 0` restores the classic lazy drift-back.

### Apply path

- Uses `CameraCenter(3)` (apply current globals only; skips the classic `SearchCameraPos` snap). Terrain occlusion is instead handled by the eased ground-clearance below; decor/scenery is not yet cleared for. This keeps the orbit from being fought every frame and matches “cinematic follow” rather than a hard “collision camera.”

**Camera elevation:** Numpad `+` / `-` adjust `AlphaCam` freely (range 150–600) instead of switching between the two fixed `VueCamera` presets. Fires every frame while held (no debounce) for smooth real-time tilt.

**Zoom input:** Numpad `/` and `*` update `FollowCamBaseDist` every frame while held; idle zoom/tilt still apply (dirty check includes base distance and `AlphaCam`).

### HD recompose (tall render heights)

The exterior projection has a fixed focal (`ChampX`/`ChampZ` = 600); a taller framebuffer therefore reveals more vertical field of view rather than zooming, so the frame fills with sky and the hero shrinks (see [WIDESCREEN.md](WIDESCREEN.md) "Vertical framing at HD"). At 1080p the vertical FOV roughly doubles, so the follow camera measurably mis-frames the subject: far away, with the upper third lost to sky.

The Auto camera answers this with a render-time recompose ("recompose, not crop"). With `k = ModeDesiredY / 480` (so every term is an exact no-op at the original 480 height), each apply pulls the boom in, steepens the pitch, and shortens the forward lean in proportion to `k - 1`:

- **Render-time only.** The player's logical `AlphaCam` and `FollowCamBaseDist` are read, adjusted for the `CameraCenter(3)` call, and restored, so manual tilt/zoom and the spring arm keep their logical values.
- **Auto path only, never in a camera zone.** Authored / scripted shots (`CameraZone`) and the classic camera are untouched, as are isometric interiors.
- **Tunable live**, then baked into `FOLLOWCAM_CFG.H`: `cam_hd` (master), `cam_hd_pitch`, `cam_hd_dist`, `cam_hd_lean` console cvars. `FollowCamHDExcess()` ([PERSO.CPP](../SOURCES/PERSO.CPP)) returns the `k`-excess the apply path scales by.

Pitch is the binding lever and saturates at the `AlphaCam` clamp (600 ≈ 53°), so on an open vista the recompose brings the subject back to a good size but leaves a natural widescreen horizon band rather than a 4:3-tight sky.

**Status: tuned for ~720p; 1080p is experimental.** The strength scales linearly with render height, which over-reaches at 1080p: the subject can feel too far and terrain near-clipping is more visible (the height-scaled near clip reduces it but does not fully solve it; a per-cell near-plane clip of the terrain grid is the real fix). The `cam_hd_*` cvars (and `cam_hd_cap`, which caps the strength so tall resolutions converge) let you tune it, and `cam_hd 0` disables it. Refining the 1080p framing and the terrain near-clip is left to a focused follow-up.

### Ground / occlusion clearance

A smooth port of the classic `SearchCameraPos` terrain awareness onto the follow path (`cam_ground`, default on; clearance `cam_ground_clear`). After `CameraCenter(3)` positions the eye, a few points are sampled along the eye-to-hero line (`FollowCamHDExcess`-style, in `PERSO.CPP`). Where terrain there would rise above the line of sight (a hill between the camera and the hero, or the eye sinking into rising ground), the eye is raised just enough to clear the worst occluder and re-aimed at the hero via the same `SetPosCamera` / `SetTargetCamera` calls the classic recenter uses.

The difference from the classic path is that the lift **eases** toward its target every frame (`FollowCamEyeLift`, tuned by `FOLLOW_CAM_GROUND_*`) instead of snapping. An earlier always-on snap fought the orbit and was reverted; easing is what makes it safe to run every frame. A `FollowCamGroundSettling` flag keeps the dirty check live until the lift converges, so it finishes even while the hero stands still. Active at every resolution (world awareness, not HD-specific), skipped in camera zones and when the eye leaves the cube (where `CalculAltitudeObjet` is invalid). It clears terrain only; decor/scenery occlusion is not yet handled.

### Manual-override fade

The HD recompose steepens pitch and the ground-clearance re-aims the eye, both on top of whatever the player set. Left alone, that fights a manual tilt: the recompose offsets it and the re-aim pins pitch to the geometry. Any manual camera nudge therefore arms `FollowCamManualHold` (in `ApplyManualCameraNudge`), which drives an `autoFactor` to 0 so the **pitch** assist (recompose pitch steepen + ground-lift re-aim) yields completely while the player drives; it eases back to full over `FollowCamManualHoldFrames` (cvar `cam_manual_hold`, ~0.75 s) once they let go. This mirrors the existing azimuth re-center (`FollowCamReengageDelay`) on the pitch axis. The boom pull-in and lean are deliberately **not** faded, so grabbing the stick to orbit does not pop the camera out to full distance.

**Performance:** Two optimizations keep the per-frame cost manageable:

1. **Idle skip:** Dirty check on hero `X`, `Y`, `Z`, `Beta` skips all camera/terrain work when the hero hasn't moved. Standing still has zero extra cost.
2. **Full refresh on movement:** Sets `FirstTime = AFF_ALL_FLIP` for full render + flip in one frame. `AFF_ALL_FLIP` runs the `AffScene` preamble (`MemoClipWindow` / `UnsetClipWindow` / `ClearImpactRain`) preventing black edges. Vsync caps at ~60 Hz. Timer compensation (`SaveTimer`/`RestoreTimer` + `TimerSystemHR`) keeps game logic at correct speed.

See [CONFIG.md](CONFIG.md) for persistence and [MENU.md](MENU.md) for the menu entry.

### Future work

- **Rendering architecture:** A faster terrain path (GPU or structural changes) would reduce the CPU cost of per-frame `RefreshGrille`.
- **Gamepad / rebinding:** Dual-stick camera; optional rebinding of zoom/tilt/pan (today numpad-heavy) for laptops and alternate layouts.
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
| Main loop follow cam    | SOURCES/PERSO.CPP          | Off-screen check, Enter key recentre, follow camera block                            |
| FollowCamera state      | SOURCES/GLOBAL.CPP, PERSO  | `FollowCamera`, `FollowCamBaseDist`; `FollowCamEffectiveDist` (spring arm, static in `PERSO.CPP`) |
| Follow cam tuning       | SOURCES/FOLLOWCAM_CFG.H    | All `FOLLOW_CAM_*` build-time constants                                             |
| Auto cam HD recompose   | SOURCES/PERSO.CPP, FOLLOWCAM_CFG.H | `FollowCamHDExcess`, `FollowCamHD{Recompose,PitchGain,DistGain,LeanGain}`, `cam_hd*` cvars |
| Ground/occlusion clearance | SOURCES/PERSO.CPP, FOLLOWCAM_CFG.H | `FollowCamEyeLift`, `FollowCamGroundSettling`, `FollowCamGround`, `FollowCamGroundClearance`, `cam_ground*` cvars |
| Manual-override fade    | SOURCES/PERSO.CPP, SOURCES/EXTFUNC.CPP | `FollowCamManualHold`, `FollowCamManualHoldFrames`, `autoFactor`, `cam_manual_hold` cvar |
| Config read/write       | SOURCES/PERSO.CPP          | `ReadConfigFile`, `WriteConfigFile`                                                 |
| Menu toggle             | SOURCES/GAMEMENU.CPP       | `GereAdvancedOptionsMenu`                                                           |


## Cross-references

- [CONFIG.md](CONFIG.md) — `FollowCamera` key (Auto camera)
- [MENU.md](MENU.md) — Advanced options menu entry
- [GLOSSARY.md](GLOSSARY.md) — Zone type 1 = camera zone; `AllCameras`
- [LIFECYCLES.md](LIFECYCLES.md) — Scene load phase 6: initialize camera; main loop step 7: `AffScene`

