# Controller and camera

How the manual camera works and which input sources drive it.

## Manual camera (Auto camera mode)

In exterior scenes with Auto camera (`FollowCamera`) on, the player can orbit,
tilt, and zoom the follow camera. Every manual source feeds one shared routine,
`ApplyManualCameraNudge` in [SOURCES/EXTFUNC.CPP](../SOURCES/EXTFUNC.CPP), which
applies and clamps three axes and re-engages the auto-follow grace period:

| Axis | Variable | Bound |
|------|----------|-------|
| Horizontal orbit | `AddBetaCam` | wraps mod 4096 |
| Elevation | `AlphaCam` | `FOLLOW_CAM_ALPHA_MIN..MAX` |
| Zoom (dolly) | `FollowCamBaseDist` | `FOLLOW_CAM_DIST_MIN..MAX` |

Only horizontal orbit raises `ManualCameraOrbitActive`, which switches
[PERSO.CPP](../SOURCES/PERSO.CPP) to a tight rotation lerp for a responsive feel
while the player is actively turning. On release the camera drifts back behind
the hero after `FOLLOW_CAM_REENGAGE_FRAMES` (tunables in
[FOLLOWCAM_CFG.H](../SOURCES/FOLLOWCAM_CFG.H)).

Manual camera is exterior plus Auto camera only. Interior scenes and the classic
fixed cameras have no orbit to drive.

## Input sources

### Keyboard

`[` / `]` orbit, numpad `+` / `-` tilt elevation, numpad `*` / `/` zoom. Handled
directly in `GereExtKeys`.

### Mouse

Right-drag (or free move) orbits and tilts, the wheel zooms, and middle-click
recenters behind the hero. Tunables in `lba2.cfg`:

| Key | Default | Description |
|-----|---------|-------------|
| `MouseCamera` | 1 | enable (0 = off) |
| `MouseCameraDrag` | 1 | 1 = right-drag to orbit, 0 = free orbit on move |
| `MouseSensitivityX` | 4 | horizontal orbit speed (1-10) |
| `MouseSensitivityY` | 4 | elevation speed (1-10) |
| `MouseInvertY` | 0 | invert elevation axis |
| `MouseCameraDivisor` | 4 | pixel-to-angle scaling (lower = faster, 1-16) |
| `MouseCameraSmoothing` | 2 | orbit lerp (1 = snap, higher = laggier, 1-16) |

### Gamepad right stick

In exterior Auto camera scenes the right stick orbits and tilts the camera
through the same `ApplyManualCameraNudge` routine as the mouse. While it owns the
stick, `UpdateJoystick` ([SOURCES/JOYSTICK.CPP](../SOURCES/JOYSTICK.CPP)) stops
emitting the digital `RSTICK_*` scancodes so the analog path and the bindings
never double-drive. Outside Auto camera (the classic fixed cameras) the stick
falls back to the digital bindings: left/right cycle the camera (`I_CAMERA`),
up/down change the camera preset (`I_CAMERA_LEVEL`). Tunables in `lba2.cfg`:

| Key | Default | Description |
|-----|---------|-------------|
| `GamepadCameraAnalog` | 1 | analog right-stick camera in Auto cam (0 = digital bindings only) |
| `GamepadCameraSensX` | 5 | orbit sensitivity (1-10) |
| `GamepadCameraSensY` | 5 | elevation sensitivity (1-10) |
| `GamepadCameraInvertY` | 0 | invert elevation axis |

The deadzone is shared with the rest of the gamepad (`GamepadDeadzone`). There is
no stick zoom: the triggers are movement and the remaining buttons are bound, so
zoom stays on the keyboard and mouse.

## Live tuning (console cvars)

Open the console (default `F12`) to tune the camera live. `name` shows a value
(bools toggle), `name value` sets it. Values are **not** clamped on set, so you
can explore past the official ranges while tuning; they clamp on cfg load, and
changes persist to `lba2.cfg` on exit. `varlist` lists them; tab completes names.

| cvar | setting |
|------|---------|
| `cam_stick_x` / `cam_stick_y` | right-stick orbit / elevation sensitivity (1-10) |
| `cam_stick_invert` | invert right-stick elevation |
| `cam_stick_analog` | analog right-stick camera in Auto cam (off = digital bindings) |
| `cam_stick_max_x` / `cam_stick_max_y` | right-stick orbit / elevation speed ceiling (at sens 10) |
| `pad_deadzone` | gamepad stick deadzone (SDL units) |
| `cam_mouse` / `cam_mouse_drag` | mouse camera enable / right-drag vs free orbit |
| `cam_mouse_x` / `cam_mouse_y` | mouse orbit / elevation sensitivity (1-10) |
| `cam_mouse_invert` / `cam_mouse_div` | invert mouse elevation / pixel-to-angle divisor |
| `cam_smooth` | manual orbit lerp (1 = snap, higher = laggier) |
| `cam_recenter_delay` | frames before Auto cam re-centers after a manual pan |
| `cam_recenter_drift` | re-center drift divisor (higher = gentler) |

The `cam_stick_max_*` ceilings, `cam_recenter_delay`, and `cam_recenter_drift`
were the constants that previously needed a recompile to tune; they are now live.
