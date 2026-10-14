#pragma once
/* Build-time tuning constants for Auto camera (FollowCamera global; community addition).
 * This follow camera was never part of the original game.
 * Change these values to adjust camera feel without touching the logic. */

/* Zoom arm recovery (FollowCamEffectiveDist vs FollowCamBaseDist) ----------*/
#define FOLLOW_CAM_SPRING_RECOVER 200 /* move effective arm toward chosen zoom per frame */

/* Rotation lag (BetaCam lerp) -----------------------------------------------*/
#define FOLLOW_CAM_ROT_DIVISOR 36   /* lerp denominator (higher = slower) */
#define FOLLOW_CAM_ROT_MIN_STEP 3   /* minimum BetaCam step per lerp tick (units) */
#define FOLLOW_CAM_ROT_THRESHOLD 10 /* snap BetaCam when within this many units of target */

/* Rotation lag paced by the clock ---------------------------------------------*/
/* The divisor above is a share of what is left per frame, so the camera takes
 * the same number of frames to come round behind the hero at any frame rate: a
 * quarter turn is within 5% in 1.7 s at 60 frames a second and 3.4 s at 30.
 * With cam_ease_ms above 0 the share follows the game clock and the turn takes
 * the same time at any rate. The value is the ease's time constant in
 * milliseconds. 300 is what the 2022 Classic release eases its auto camera
 * with, 600 is the pace the divisor gives at 60 frames a second, and 0 goes
 * back to the divisor. An orbit the player is driving keeps its own lerp. */
#define FOLLOW_CAM_EASE_MS_DEFAULT 300
#define FOLLOW_CAM_EASE_MS_MAX 5000
#define FOLLOW_CAM_EASE_DT_MAX 100  /* the longest frame an ease is paced by (ms), so a stall is not spent in one step */
#define FOLLOW_CAM_ROT_MIN_RATE 180 /* slowest the timed yaw ease goes (units a second): FOLLOW_CAM_ROT_MIN_STEP at 60 frames a second */

/* Look-at point ----------------------------------------------------------------*/
/* The camera looks at a spot DISTANCE_VISEE ahead of the hero, and that spot is
 * pinned to him: it starts and stops on the frame he does. With cam_target_ms
 * above 0 the spot the camera looks at trails it and catches up with that time
 * constant, as the 2022 Classic release's does (300 there), so the hero leads
 * the frame a little while he runs and the view settles after he stops. 0 pins
 * it to him again. */
#define FOLLOW_CAM_TARGET_MS_DEFAULT 300
#define FOLLOW_CAM_TARGET_MIN_RATE 256 /* slowest the look-at point closes on its spot (world units a second) */
/* A spot that moved further than this between two updates was not walked there:
 * the hero crossed the seam between two outdoor scenes, which renumbers the
 * world under him, or was placed somewhere. What the look-at point trails by is
 * held as it was and the camera moves with him, as it does when pinned. Two
 * bricks: at the longest update an ease is paced by, that is twenty bricks a
 * second, several times what anything in the game is carried at. */
#define FOLLOW_CAM_TARGET_JUMP 1024

/* Pan drift (AddBetaCam drift toward 0 after [ / ] released) ----------------*/
/* Defaults for the runtime FollowCamReengageFrames / FollowCamDriftDivisor globals
 * (cvar cam_recenter_delay / cam_recenter_drift, cfg keys of the same name). */
#define FOLLOW_CAM_REENGAGE_FRAMES_DEFAULT 60 /* grace frames before follow re-centers after a manual pan (about 1.0s at 60fps) */
#define FOLLOW_CAM_DRIFT_DIVISOR_DEFAULT 24   /* drift denominator: about 4% per frame (higher = gentler, slower ease-back) */
#define FOLLOW_CAM_DRIFT_MIN_STEP 3           /* minimum AddBetaCam step per drift tick (units) */
#define FOLLOW_CAM_DRIFT_THRESHOLD 5          /* snap AddBetaCam to 0 within this many units */

/* Manual-override hold (pitch / distance auto-correction) --------------------*/
/* The HD recompose and ground-clearance push pitch and distance on top of the
 * player's manual tilt/zoom. Any manual camera nudge drives the auto-assist
 * strength to 0 and it eases back to full over this many frames, so manual
 * control wins while the player is driving and the assist fades back in after.
 * Seeds the runtime FollowCamManualHoldFrames global (cvar cam_manual_hold). */
#define FOLLOW_CAM_MANUAL_HOLD_DEFAULT 45 /* frames the auto-assist yields after a manual camera nudge (~0.75s) */
/* Ceiling on the above. Ten seconds at 60fps, matching the re-engage delay's
 * bound, and low enough that the fade's `hold * 100` cannot leave S32. */
#define FOLLOW_CAM_MANUAL_HOLD_MAX 600

/* Scene changes ---------------------------------------------------------------*/
/* What the Auto camera does on arriving in an outdoor scene, other than by
 * walking across the seam between two of them (that already carries over).
 * Off, an ordered change (a script's, the console's) recentres: the pan goes to
 * zero and the pitch to the preset, whatever the player had set. A door out of
 * an interior leaves the yaw where the last outdoor scene left it, so the camera
 * swings round to the hero, and takes its pitch from the classic obstruction
 * search's one-off answer. On, the scene change leaves the pitch alone, keeps
 * the pan between two outdoor scenes and sends it home out of an interior, and
 * the camera starts behind the hero at that pan, with nothing left to swing
 * through. On by default. cvar cam_scene_carry. */
#define FOLLOW_CAM_SCENE_CARRY_DEFAULT 1

/* Camera zones ----------------------------------------------------------------*/
/* Whether a scripted shot that only latches takes the view from a player who is
 * steering. Off, it does, as under the classic camera: the camera cuts to the
 * shot once on entry and hold-angle keeps it there. On, such a zone is left
 * unfired while the Auto camera has the view outdoors and the hero is under
 * manual control. A forced zone, an interior's, and one entered by a hero a
 * script is moving fire as before. On by default. cvar cam_zone_skip. */
#define FOLLOW_CAM_ZONE_SKIP_DEFAULT 1

/* Hold-angle (free-camera) mode ---------------------------------------------*/
/* When on, the manual camera rotation (AddBetaCam) is never auto-recentered
 * while walking; it holds until the scene changes or the player hits Center
 * camera (Enter / gamepad B), like a free third-person camera. Off restores the
 * classic lazy drift-back. cvar cam_hold_angle. */
#define FOLLOW_CAM_HOLD_ANGLE_DEFAULT 1

/* World anchor (prototype) ---------------------------------------------------*/
/* What the camera's horizontal angle is held against. Off, it is an offset from
 * the hero's facing (AddBetaCam), so when he turns the camera comes round with
 * him once any grace after a pan has run out, and hold-angle holds that offset.
 * On, it holds a direction in the world: the hero turns under the camera and it
 * stays where it is, and only the player turns it (orbit, the pan keys, the
 * quarter-turn key, Center camera). The pan drift back behind the hero is off
 * with it, whatever cam_hold_angle says. Off by default. cvar cam_world_anchor. */
#define FOLLOW_CAM_WORLD_ANCHOR_DEFAULT 0

/* Scripted shots (camera zones) --------------------------------------------*/
/* A camera zone re-aims the view to an authored shot. The Auto camera keeps what
 * the shot left behind: the yaw as an offset from the hero's facing, and the
 * pitch as authored, until the player moves the camera or the scene changes.
 * These two treat the shot as a loan instead. cam_zone_return is on by default;
 * cam_zone_anchor is a prototype and off. With cam_zone_skip on as well, the
 * shots this reaches in play are the ones that hold the view, and one that
 * latches only when a script is moving the hero.
 *
 * cam_zone_return: when the hero leaves the shot's box, ease the yaw offset and
 * the pitch back to what the Auto camera had before the shot fired. Whatever the
 * player set by hand in the meantime is theirs and is not given back.
 *
 * cam_zone_anchor: while the hero is inside the box of a shot that fires once
 * and latches, hold the shot's authored world yaw as the classic camera does,
 * rather than carrying it round as an offset from wherever the hero is facing. */
#define FOLLOW_CAM_ZONE_RETURN_DEFAULT 1
#define FOLLOW_CAM_ZONE_ANCHOR_DEFAULT 0
#define FOLLOW_CAM_RETURN_YAW_MIN_STEP 3   /* minimum AddBetaCam step per frame of the return (units) */
#define FOLLOW_CAM_RETURN_PITCH_MIN_STEP 1 /* minimum AlphaCam step per frame of the return (units) */

/* Elevation (AlphaCam) — applies when Auto camera (FollowCamera) is on -----*/
#define FOLLOW_CAM_ALPHA_MIN 150 /* minimum elevation angle */
#define FOLLOW_CAM_ALPHA_MAX 600 /* maximum elevation angle */
#define FOLLOW_CAM_ALPHA_STEP 5  /* elevation change per numpad +/- key press */

/* Zoom (numpad * / /) — adjusts default arm distance -----------------------*/
#define FOLLOW_CAM_ZOOM_STEP 200  /* distance change per frame while key held */
#define FOLLOW_CAM_DIST_MIN 7000  /* closest the player can zoom in (must clear hero mesh) */
#define FOLLOW_CAM_DIST_MAX 20000 /* furthest the player can zoom out */
/* Resting arm length: the midpoint of the two VueCamera presets
 * (DefVueDistance[0]=10500, DefVueDistance[1]=17000). Set on the first frame and
 * restored by Center camera, so this is the zoom a player returns to. Unlike the
 * rest of this header it expands to engine state, so it is usable only where
 * DefVueDistance is declared. */
#define FOLLOW_CAM_INITIAL_DIST ((DefVueDistance[0] + DefVueDistance[1]) / 2)

/* Horizontal pan keys [ ] ---------------------------------------------------*/
#define FOLLOW_CAM_PAN_STEP 30 /* AddBetaCam change per [ / ] key press per frame */

/* Orbit follow-through ------------------------------------------------------*/
/* Percentage of the orbit speed that survives each frame once the player stops
 * driving: the camera eases to a stop over a few frames instead of halting on
 * the frame the stick centres. 0 disables it (halt on release). */
#define FOLLOW_CAM_ORBIT_GLIDE_DEFAULT 75
#define FOLLOW_CAM_ORBIT_GLIDE_MAX 95 /* above this the tail outstays its welcome */
#define FOLLOW_CAM_ORBIT_GLIDE_STOP 2 /* drop the tail once it decays below this (units/frame) */

/* Mouse camera --------------------------------------------------------------*/
#define MOUSE_CAM_DIVISOR_DEFAULT 4   /* pixel-to-angle scaling (lower = faster) */
#define MOUSE_CAM_SMOOTHING_DEFAULT 2 /* BetaCam lerp divisor during orbit (1=snap, higher=laggier) */
#define MOUSE_CAM_DEAD_ZONE 2         /* ignore deltas smaller than this (pixels, pre-scale) */
#define MOUSE_CAM_ZOOM_MULT 1         /* wheel zoom multiplier (applied to FOLLOW_CAM_ZOOM_STEP) */
#define MOUSE_CAM_SENS_DEFAULT 4      /* default sensitivity (1-10 range) */

/* HD vertical-FOV recompose (Auto camera only; community) -------------------*/
/* The exterior projection has a fixed focal (ChampX/ChampZ = 600); a taller
 * framebuffer therefore reveals more vertical field of view rather than zooming,
 * so the frame fills with sky and the hero shrinks (see docs/WIDESCREEN.md
 * "Vertical framing at HD" and docs/CAMERA.md). The follow camera answers this
 * by pulling its boom in and steepening its pitch ("recompose, not crop").
 * k = ModeDesiredY / 480; every lever scales with (k-1), expressed as the
 * hundredths excess kExcess = ModeDesiredY*100/480 - 100 (0 at 480, 125 at
 * 1080), so all three are exact no-ops at the original 480 height. Gains below
 * seed runtime globals; tune live via the cam_hd_* console cvars then bake. */
#define FOLLOW_CAM_HD_RECOMPOSE_DEFAULT 1    /* master enable (cam_hd) */
#define FOLLOW_CAM_HD_EXCESS_CAP_DEFAULT 0   /* cap on kExcess so tall res converges (0 = no cap / pure linear, the current behavior); e.g. 70 ~= 816p */
#define FOLLOW_CAM_HD_PITCH_GAIN_DEFAULT 180 /* AlphaCam units added per 1.0 of (k-1) */
#define FOLLOW_CAM_HD_DIST_GAIN_DEFAULT 27   /* % of boom pulled in per 1.0 of (k-1) */
#define FOLLOW_CAM_HD_LEAN_GAIN_DEFAULT 45   /* % of forward lean removed per 1.0 of (k-1) */
#define FOLLOW_CAM_HD_DIST_MAX_PULL 50       /* clamp: never pull the boom in past this % */
#define FOLLOW_CAM_HD_LEAN_MAX_CUT 70        /* clamp: never cut the forward lean past this % */

/* Ground / occlusion clearance (Auto camera only; community) -----------------*/
/* A smooth port of the classic SearchCameraPos terrain awareness onto the
 * follow path. A few points are sampled along the eye->hero line; if terrain
 * there would rise above the line of sight (a hill between the camera and the
 * hero, or the eye sinking into rising ground), the eye lifts just enough to
 * clear the worst occluder and re-aims at the hero. Unlike the classic recenter
 * (a one-shot snap, which is why an earlier always-on attempt fought the orbit
 * and was reverted), the lift eases toward its target every frame. Active at
 * every resolution: this is world awareness, not HD-specific. */
#define FOLLOW_CAM_GROUND_DEFAULT 1             /* master enable (cam_ground) */
#define FOLLOW_CAM_GROUND_CLEARANCE_DEFAULT 600 /* keep the sight line this far above terrain */
#define FOLLOW_CAM_GROUND_MAX_LIFT 8000         /* clamp the total eye lift (units) */
#define FOLLOW_CAM_GROUND_EASE 8                /* lift lerp divisor (higher = lazier ease) */
#define FOLLOW_CAM_GROUND_MIN_STEP 40           /* minimum lift change per eased frame (units) */
#define FOLLOW_CAM_GROUND_SNAP 40               /* snap lift to target within this many units */

/* Obstruction clearance (Auto camera only; community) -------------------------*/
/* The classic camera asks SearchCameraPos, once per recentre, how steeply it has
 * to look down for the boom to clear the terrain, the scenery boxes and the
 * scripted objects between it and the hero, and the answer is written into
 * AlphaCam. The classic camera gives that pitch back at its next off-screen
 * recentre. The Auto camera has no such recentre, so the pitch stays as though
 * the player had set it. On, the Auto camera asks the same question on every
 * update without letting it write anything, and carries what the place needs
 * above its own pitch as an extra that eases up and down and is added for the
 * render only. The recentres' own searches stand down while it has the view
 * outdoors, and so does the hidden-hero recentre for as long as the extra is
 * still on its way up. On by default. cvar cam_obstruct. */
#define FOLLOW_CAM_OBSTRUCT_DEFAULT 1
#define FOLLOW_CAM_OBSTRUCT_EASE 10 /* extra-pitch lerp divisor (higher = lazier ease) */
/* The same while the hero is drawn fully hidden (cvar cam_obstruct_hurry). Lower
 * shows him sooner and tilts harder to do it, and 1 is a cut. The default is
 * the ordinary ease, which is no hurry at all: played at 1080p, 3 closed a
 * third of the gap in the one frame he was hidden and read as a pop of ten
 * degrees, and 10 did not show. */
#define FOLLOW_CAM_OBSTRUCT_HURRY_DEFAULT 10
#define FOLLOW_CAM_OBSTRUCT_MIN_STEP 4     /* minimum change per eased frame (AlphaCam units) */
#define FOLLOW_CAM_OBSTRUCT_PITCH_MAX 1000 /* never render steeper than this; 1024 is straight down */
/* Where the search finds nothing and the hero is drawn hidden all the same, the
 * render's word is taken over the search's: the extra is raised by a step for
 * each such frame, and that floor holds until he has walked this far from where
 * it was last raised. The step is taken whole, not eased: it is small enough not
 * to read as a pop, and eased at the ordinary rate it left him hidden for a
 * second at a time by the covered bridge. */
#define FOLLOW_CAM_OBSTRUCT_HIDDEN_STEP 20   /* extra pitch added per frame he is still hidden (AlphaCam units) */
#define FOLLOW_CAM_OBSTRUCT_FLOOR_REACH 2048 /* distance on either axis over which a floor is kept (world units, four bricks) */

/* Gamepad right-stick camera ------------------------------------------------*/
#define GAMEPAD_CAM_SENS_DEFAULT 5 /* default right-stick sensitivity (1-10 range) */
/* Defaults for the runtime GamepadCamMaxBeta / GamepadCamMaxAlpha globals
 * (cvar cam_stick_max_x / cam_stick_max_y, cfg keys of the same name). */
#define GAMEPAD_CAM_BETA_MAX_DEFAULT 36  /* orbit units per frame at full deflection, sens 10 (the ceiling) */
#define GAMEPAD_CAM_ALPHA_MAX_DEFAULT 10 /* elevation units per frame at full deflection, sens 10 (the ceiling) */
