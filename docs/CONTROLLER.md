# Controller & Mouse Camera

Working design document for mouse camera, gamepad, and modern input features.

## Roadmap

| Phase | Branch | Description | Depends on |
|-------|--------|-------------|------------|
| **1** | `feature/mouse-camera` | Mouse orbit, zoom, elevation in exterior FollowCamera | — |
| **2** | `feature/gamepad-input` | SDL3 gamepad: left stick → movement, right stick → camera, triggers → zoom | Phase 1 |
| **3** | `feature/controls-menu` | Restructure Options: Controls → Keyboard / Mouse / Controller sub-menus | Phase 1+2 |
| **4** | `feature/camera-relative-movement` | Optional camera-relative stick / WASD movement (non-tank) | Phase 2+3 |

Each phase is an independent PR behind its own config flag.

---

## Phase 1 — Mouse Camera

Mouse orbits the camera, adjusts elevation, scroll wheel zooms.
Only active when `FollowCamera=1` AND exterior scene.
Reuses the same variables as the keyboard FollowCamera controls.

### Input mapping

| Mouse input | Variable | Keyboard equivalent |
|-------------|----------|-------------------|
| Move / right-drag horizontal | `AddBetaCam` | `[` / `]` |
| Move / right-drag vertical | `AlphaCam` | Numpad `+` / `-` |
| Scroll wheel | `FollowCamBaseDist` | Numpad `*` / `/` |
| Middle-click | `CameraCenter(1)` | Enter |

### Two orbit modes

| Mode | `MouseCameraDrag` | Behaviour |
|------|-------------------|-----------|
| **Free** | `0` (default) | Any mouse motion orbits; dead-zone threshold filters jitter |
| **Hold** | `1` | Right-button hold required to orbit |

### Config keys (`lba2.cfg`)

```
MouseCamera: 1          ; 0=off, 1=on
MouseCameraDrag: 0      ; 0=free orbit on move, 1=require right-drag
MouseSensitivityX: 4    ; orbit speed (1-10)
MouseSensitivityY: 4    ; elevation speed (1-10)
MouseInvertY: 0         ; 0=normal, 1=inverted
```

### Implementation

- Hook: `GereExtKeys()` in `SOURCES/EXTFUNC.CPP`, inside the `if (FollowCamera)` block
- Consumes `MouseXDep`/`MouseYDep` via `ManageMouse()`
- Sets `FollowCamReengageDelay` on orbit (same grace-period as `[`/`]` keys)
- PERSO.CPP dirty check already reacts to `AddBetaCam`/`AlphaCam` changes
- Tuning constants in `SOURCES/FOLLOWCAM_CFG.H`

---

## Phase 2 — SDL3 Gamepad (future)

Rewrite `SOURCES/JOYSTICK.CPP` (currently all commented out) using SDL3 gamepad API.
- Left stick → I_UP/DOWN/LEFT/RIGHT (tank controls, with dead zone)
- Right stick → camera orbit/elevation (same variables as mouse)
- L2/R2 triggers → zoom
- Face buttons → Action, Inventory, Behavior, Menu
- Config: `GamepadEnabled`, dead zone, invert, button mapping

## Phase 3 — Controls Menu (future)

Restructure Options menu:
```
Options → Controls → Keyboard (existing, moved)
                   → Mouse (sensitivity, invert, enable)
                   → Controller (dead zone, invert, button map)
```

## Phase 4 — Camera-Relative Movement (future)

Optional mode where stick / WASD moves character relative to camera facing instead
of tank controls.  Character auto-rotates toward movement direction.  Behind config
flag `ModernMovement=0`.  Needs extensive playtesting — changes combat and platforming feel.
