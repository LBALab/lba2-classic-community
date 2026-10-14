# Camera

Outdoors, LBA2 Classic Community has two cameras. The **classic camera** is the original game's: it sits at fixed angles and jumps when Twinsen walks off screen. The **Auto camera** is a community addition: a third-person camera that follows behind Twinsen and that you can orbit, tilt and zoom. It is off by default. Interiors keep the original isometric view with either camera.

How the cameras work inside the engine is in [CAMERA_INTERNALS.md](CAMERA_INTERNALS.md).

## Switching it on

- In the game: Options → Advanced options → **Auto camera** (the same entry reads **Classic camera** when it is off).
- In the console (`F12`): `cam_follow 1`, or `cam_follow 0` for the classic camera.
- In `lba2.cfg`: `FollowCamera: 1`.

The choice is saved and applies to every save game.

## Controls

These work outdoors with the Auto camera on.

| Action | Keyboard | Mouse | Gamepad |
| --- | --- | --- | --- |
| Orbit round Twinsen | `[` / `]` | drag with the right button | right stick left / right |
| Tilt up and down | numpad `+` / `-`, or Page Up / Page Down | drag with the right button | right stick up / down |
| Zoom in / out | numpad `/` / `*` | wheel | none |
| Put the camera behind Twinsen | Enter | middle button | B |
| Quarter turn | Backspace | none | none |

Tilt and the quarter turn can be rebound in Options → Keyboard; the orbit and zoom keys are fixed. Mouse and stick sensitivity, inversion and the dead zone are in [CONTROLLER.md](CONTROLLER.md).

The zoom is not saved: it starts at the middle distance in every session, and Enter puts it back there.

## What the Auto camera does

- **It keeps the angle you set.** Orbit the camera and it stays there as Twinsen walks, until you press Enter. With `cam_hold_angle 0` it drifts back behind him instead.
- **It comes round behind Twinsen as he turns**, in about a second, and stops exactly behind him. The view trails him a little while he runs and settles after he stops. Both follow the pace of the 2022 Classic release's auto camera.
- **Scripted camera shots.** Many outdoor places have a camera shot placed by the game's designers. A shot that holds the view takes it while Twinsen is inside its area, gliding in and out rather than cutting. When he leaves, the camera eases back to the angle and tilt you had before. Shots that only fire once do not take the view while you are steering.
- **Signs.** A sign that shows a place shows that view as soon as its text opens. When you close it, the camera glides back to Twinsen.
- **Buildings.** Coming out of a door, the camera starts straight behind Twinsen.
- **Things in the way.** When a building, a bridge or a hill comes between the camera and Twinsen, the camera tilts or rises over it while it is in the way, then eases back.
- **High resolutions.** In a tall window the camera moves in closer and looks down more steeply, so Twinsen does not shrink into a frame full of sky. It is tuned for about 720 lines; at 1080 the framing is still experimental.
- **Movie cameras.** With Options → Advanced options → **Movie camera** off, the shots that fire once are skipped entirely, with either camera.

## Settings

Each behaviour above is a setting you can switch off to get the camera as it was before it. Change one in the console with `name value`, for example `cam_shot_blend_ms 0`. Always type the value: a name typed alone switches the setting over. Console changes are written to `lba2.cfg` when the game exits. You can also edit `lba2.cfg` while the game is closed.

| Console | `lba2.cfg` key | Default | What it does | Off gives |
| --- | --- | --- | --- | --- |
| `cam_follow` | `FollowCamera` | 0 | The Auto camera itself | the classic camera |
| `cam_hold_angle` | `FollowCamHoldAngle` | 1 | Keeps the angle you orbit to | drifts back behind Twinsen as he walks |
| `cam_ease_ms` | `FollowCamEaseMs` | 300 | How quickly the camera comes round behind Twinsen, in milliseconds (600 is the earlier pace) | 0: the earlier per-frame follow, which can stop up to a degree short |
| `cam_target_ms` | `FollowCamTargetMs` | 300 | How far the view trails Twinsen, in milliseconds | 0: the view pinned to him |
| `cam_shot_blend_ms` | `FollowCamShotBlendMs` | 600 | Glide into and out of a scripted shot, in milliseconds | 0: a cut |
| `cam_zone_return` | `FollowCamZoneReturn` | 1 | Gives your angle and tilt back after a shot | the shot's angle stays until you move the camera |
| `cam_zone_skip` | `FollowCamZoneSkip` | 1 | Shots that fire once leave you alone while you steer | they take the view for a moment |
| `cam_scene_carry` | `FollowCamSceneCarry` | 1 | Starts behind Twinsen out of a building; keeps your angle between two outdoor scenes a script changes | the camera arrives to one side and swings round |
| `cam_obstruct` | `FollowCamObstruct` | 1 | Tilts over scenery only while it is in the way | a steep tilt can stay after it is clear |
| `cam_ground` | `FollowCamGround` | 1 | Rises over hills between the camera and Twinsen | a hill can hide him |
| `cam_hd` | `FollowCamHDRecompose` | 1 | Reframes at tall resolutions | Twinsen small under a lot of sky |
| `cam_world_anchor` | `FollowCamWorldAnchor` | 0 | **Experimental.** Holds a direction in the world: Twinsen turns, the camera does not. With `cam_hold_angle 0` it comes back behind him as he walks, paced by `cam_world_return` (700) | |
| `cam_zone_anchor` | `FollowCamZoneAnchor` | 0 | **Experimental.** Inside a shot that fires once, holds the shot's angle as Twinsen turns | |

`settings_before cam_` in the console switches off, at once, every behaviour added since the Auto camera first shipped: the rows from `cam_ease_ms` to `cam_obstruct` and the two experimental ones. The finer tuning values (`cam_hd_*`, `cam_ground_clear`, `cam_obstruct_hurry`, `cam_manual_hold`, `cam_recenter_*`) are described in [CAMERA_INTERNALS.md](CAMERA_INTERNALS.md), and `help <name>` in the console explains any of them.

A session recording carries the camera settings it was made with. A replay uses them for its length and then gives yours back.

## Known limitations

- Outside the Dome of the Slate, the camera can end up inside the Dome and look straight down.
- The camera can clip into scenery, and scenery that the obstruction check does not see can hide Twinsen for a few frames ([#363](https://github.com/LBALab/lba2-classic-community/issues/363)).
- Orbiting with the mouse and then holding it still stops the camera dead instead of easing ([#514](https://github.com/LBALab/lba2-classic-community/issues/514)).
- Crossing the edge between two outdoor scenes moves Twinsen and the camera a short step forward in one frame. It comes from the original game ([#707](https://github.com/LBALab/lba2-classic-community/issues/707)).
- At 1080 lines and above, the framing is still being tuned.

## See also

- [CONTROLLER.md](CONTROLLER.md): mouse and gamepad camera settings.
- [CONSOLE.md](CONSOLE.md): the console, and `camtrace` for logging what the camera does.
- [CONFIG.md](CONFIG.md): `lba2.cfg`.
- [CAMERA_INTERNALS.md](CAMERA_INTERNALS.md): how it works, for contributors.
