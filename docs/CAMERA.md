# Camera System

The camera behaves differently in interior (isometric) and exterior (perspective) scenes. This document covers both paths, the `CameraCenter` function that ties them together, and the `AutoCameraCenter` option for exterior follow-camera behavior.

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
- **Camera cycle (`I_CAMERA`):** rotates `AddBetaCam` by 90° (1024 units), calls `CameraCenter(2)` ([PERSO.CPP](../SOURCES/PERSO.CPP) lines 1599–1606).
- `**GereExtKeys`:** keyboard/mouse-driven `AlphaCam` / `BetaCam` adjustment ([SOURCES/EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP) line 1910+).

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

## AutoCameraCenter (community addition)

Config key `AutoCameraCenter` (0 = manual, 1 = auto; **default 0**). Toggled in Options → Advanced options ("Auto camera centering" / "Manual camera centering"). Off by default: the feature still has visible trade-offs (terrain throttle vs object projection) and cost on CPU software rendering; turn it on when acceptable or after future architectural improvements (e.g. unified per-frame terrain/object camera path or accelerated terrain).

When enabled in exterior mode (and not in a camera zone or cinema), the main loop reorients the camera behind the hero when the hero's position or facing changes. Uses `CameraCenter(3)` (camera-apply-only, no `SearchCameraPos`). The player's `AddBetaCam` offset (camera cycle) is preserved; `AlphaCam` and `VueDistance` are not reset, so elevation and zoom remain as the player set them.

**Performance:** Terrain (`RefreshGrille` → `AffGrilleExt`) is the dominant per-frame cost in software rendering: 65x65 vertex projection, 64x64 textured Z-buffered polygon rasterization, horizon, sky/sea, and decor objects — all on the CPU. Two optimizations keep this manageable:

1. **Idle skip:** A dirty check on the hero's `X`, `Y`, `Z`, and `Beta` avoids all camera and terrain work when the hero hasn't moved. Standing still has zero extra cost compared to manual mode.
2. **Terrain throttle:** When moving, terrain refreshes are throttled with `AUTO_CAM_TERRAIN_REFRESH_INTERVAL` (default **2** in `PERSO.CPP`): only every Nth movement update sets `FirstTime = AFF_ALL_NO_FLIP` to trigger `RefreshGrille`. On intermediate frames, `AffScene` runs the cheaper `AFF_OBJETS_FLIP` path (objects use the live camera; terrain pixels lag). Higher N saves CPU but increases object-vs-ground desync. **N=2** is the usual compromise.

See [CONFIG.md](CONFIG.md) for persistence and [MENU.md](MENU.md) for the menu entry.

### Future work

- **Rendering architecture:** Throttling terrain without redrawing it every frame causes a mismatch between the ground bitmap (last full `RefreshGrille`) and objects (current camera). A proper fix is to refresh terrain every frame when the camera moves at acceptable cost (e.g. faster terrain path, GPU, or structural changes so ground and actors share one consistent camera state per frame).
- **Smooth swivel:** Interpolate `BetaCam` toward the target angle over several frames rather than snapping. Would give a more polished feel similar to the enhanced edition's rotation.
- **Dead zone:** Only reorient when the hero's facing changes by more than a threshold, to avoid jitter during small movements.

## Code reference


| Concept                 | File                       | Symbol                                                                              |
| ----------------------- | -------------------------- | ----------------------------------------------------------------------------------- |
| Low-level camera        | LIB386/3D/CAMERA.CPP       | `SetCamera`, `SetFollowCamera`, `SetPosCamera`, `SetTargetCamera`, `SetAngleCamera` |
| Camera globals          | LIB386/H/3D/CAMERA.H       | `CameraX`/`Y`/`Z`, `CameraAlpha`/`Beta`/`Gamma`, `Xp`, `Yp`                         |
| View globals            | SOURCES/3DEXT/VAR_EXT.CPP  | `AlphaCam`, `BetaCam`, `GammaCam`, `VueDistance`, `VueOffsetX`/`Y`/`Z`              |
| CameraCenter            | SOURCES/INTEXT.CPP         | `CameraCenter(flagbeta)`                                                            |
| SearchCameraPos         | SOURCES/3DEXT/MAPTOOLS.CPP | `SearchCameraPos(x, y, z, objbeta, mode)`                                           |
| Exterior init           | SOURCES/EXTFUNC.CPP        | `Init3DExtView`, `Init3DExtGame`                                                    |
| Main loop recenter      | SOURCES/PERSO.CPP          | Off-screen check, Enter key recentre, `AutoCameraCenter` block                      |
| AutoCameraCenter global | SOURCES/GLOBAL.CPP         | `AutoCameraCenter`                                                                  |
| Config read/write       | SOURCES/PERSO.CPP          | `ReadConfigFile`, `WriteConfigFile`                                                 |
| Menu toggle             | SOURCES/GAMEMENU.CPP       | `GereAdvancedOptionsMenu`                                                           |


## Cross-references

- [CONFIG.md](CONFIG.md) — `AutoCameraCenter` key
- [MENU.md](MENU.md) — Advanced options menu entry
- [GLOSSARY.md](GLOSSARY.md) — Zone type 1 = camera zone; `AllCameras`
- [LIFECYCLES.md](LIFECYCLES.md) — Scene load phase 6: initialize camera; main loop step 7: `AffScene`

