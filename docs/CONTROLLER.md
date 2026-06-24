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

### Gamepad right stick (next)

The gamepad stack ([SOURCES/JOYSTICK.CPP](../SOURCES/JOYSTICK.CPP)) and the
Controller options menu already ship; today the right stick drives camera
elevation and the classic camera turn digitally through the binding system. The
planned step routes the raw right-stick axes through this same
`ApplyManualCameraNudge` routine for smooth analog orbit and elevation in Auto
camera mode, with a digital fallback in classic mode.
