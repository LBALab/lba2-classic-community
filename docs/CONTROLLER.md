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

In headset sessions with the Auto camera, the right stick controls zoom
(X: right = closer, left = wider) and camera tilt (Y: up = toward the
horizon, down = top-down) through `ApplyManualCameraNudge`; the head-look
override pans the view, so the rig takes no motion from the head.

In exterior Auto camera scenes (flat presentation) the right stick orbits and tilts the camera
through the same `ApplyManualCameraNudge` routine as the mouse. While it owns the
stick, `UpdateJoystick` ([SOURCES/JOYSTICK.CPP](../SOURCES/JOYSTICK.CPP)) stops
emitting the digital `RSTICK_*` scancodes so the analog path and the bindings
never double-drive. Outside Auto camera (the classic fixed cameras) the stick
falls back to the digital bindings: left/right cycle the camera (`I_CAMERA`),
up/down change the camera preset (`I_CAMERA_LEVEL`). Pressing the right stick
(`K_GAMEPAD_RSTICK_BTN`, input slot 36) cycles the camera setting — Classic
camera / Auto camera / First person — the same three-way cycle as the
Advanced-options row; configs saved before this binding existed migrate R3 off
the dodge secondary automatically. The press does nothing in interiors, where
the isometric camera has no first-person mode; a press held across the exit is
consumed so it cannot fire on the threshold. Entering an interior while the
first-person camera is active returns to the classic camera, recentred on the
hero; entering the classic camera (from first person) also recenters on the
hero instead of keeping the previous eye framing. Tunables in `lba2.cfg`:

| Key | Default | Description |
|-----|---------|-------------|
| `GamepadCameraAnalog` | 1 | analog right-stick camera in Auto cam (0 = digital bindings only) |
| `GamepadCameraSensX` | 5 | orbit sensitivity (1-10) |
| `GamepadCameraSensY` | 5 | elevation sensitivity (1-10) |
| `GamepadCameraInvertY` | 0 | invert elevation axis |

In first-person stereo the left stick moves camera-relative: forward/back and
strafe, with diagonals taken from the stick angle. The movement pass points
`Obj.Beta` along the stick vector and holds `I_UP` for that frame, so the
normal walk animation and collision path carry the displacement; the body sits
behind the camera, so the one-frame facing change is invisible. The right stick
turns the view at 180°/s (after the shared deadzone). `UpdateJoystick`
suppresses both sticks' digital scancodes in this mode; the raw right axis is
integrated as a yaw offset in `UpdateFirstPersonCamera` and applied to both the
base camera and the per-eye stereo override. The offset resets when the camera
setting changes.

The deadzone is shared with the rest of the gamepad (`GamepadDeadzone`). There is
no stick zoom: the triggers are movement and the remaining buttons are bound, so
zoom stays on the keyboard and mouse.

### Quest interior camera

In a stereo Quest session, interior scenes keep the left stick's ordinary
locomotion bindings. The right stick moves the isometric view: horizontal
pans it across, vertical pans it up and down, both clamped to a quarter frame
so the room cannot be scrolled completely off screen. Enter recenters the pan
and returns the zoom to 100%; a cube change only recenters the pan.

Resting the left thumb on its capacitive thumb rest, the area by the buttons,
turns the right stick's vertical axis into the `VrFovScale` zoom (50–120)
instead of a vertical pan. The rest is a Touch-profile input; the left
thumbstick's own touch sensor is not the modifier, so a thumb left on the
stick still pans normally. The interior present scales the resolved stereo
image about each eye's centre: below 100 the centre is cropped and magnified,
above 100 the eye shrinks into a centred black frame.

The right stick's digital camera-preset bindings are suppressed in this mode so
the analog pan does not also cycle the classic camera.

The head-tracked hover rig is opt-in (Display → VR → Interior hover, cfg
`VrInteriorHover`, console `vrhover`). It anchors the head pose when it
engages: a head turn moves the room by exactly the angle turned in the eye's
projection, so the image stays hung in the world instead of following your
face, and leaning left/right or up/down adds a depth-correct parallax (near
planes move further than far ones). Looking far enough carries the room out
of view. Roll is held level. Enter recaptures the anchor. The head look runs
inside a camera zone too; only cinema scenes keep the authored view.

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
