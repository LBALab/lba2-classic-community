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

/* Pan drift (AddBetaCam drift toward 0 after [ / ] released) ----------------*/
/* Defaults for the runtime FollowCamReengageFrames / FollowCamDriftDivisor globals
 * (cvar cam_recenter_delay / cam_recenter_drift, cfg keys of the same name). */
#define FOLLOW_CAM_REENGAGE_FRAMES_DEFAULT 60 /* grace frames before follow re-centers after a manual pan (about 1.0s at 60fps) */
#define FOLLOW_CAM_DRIFT_DIVISOR_DEFAULT 24   /* drift denominator: about 4% per frame (higher = gentler, slower ease-back) */
#define FOLLOW_CAM_DRIFT_MIN_STEP 3           /* minimum AddBetaCam step per drift tick (units) */
#define FOLLOW_CAM_DRIFT_THRESHOLD 5          /* snap AddBetaCam to 0 within this many units */

/* Elevation (AlphaCam) — applies when Auto camera (FollowCamera) is on -----*/
#define FOLLOW_CAM_ALPHA_MIN 150 /* minimum elevation angle */
#define FOLLOW_CAM_ALPHA_MAX 600 /* maximum elevation angle */
#define FOLLOW_CAM_ALPHA_STEP 5  /* elevation change per numpad +/- key press */

/* Zoom (numpad * / /) — adjusts default arm distance -----------------------*/
#define FOLLOW_CAM_ZOOM_STEP 200  /* distance change per frame while key held */
#define FOLLOW_CAM_DIST_MIN 7000  /* closest the player can zoom in (must clear hero mesh) */
#define FOLLOW_CAM_DIST_MAX 20000 /* furthest the player can zoom out */

/* Horizontal pan keys [ ] ---------------------------------------------------*/
#define FOLLOW_CAM_PAN_STEP 30 /* AddBetaCam change per [ / ] key press per frame */

/* Mouse camera --------------------------------------------------------------*/
#define MOUSE_CAM_DIVISOR_DEFAULT 4   /* pixel-to-angle scaling (lower = faster) */
#define MOUSE_CAM_SMOOTHING_DEFAULT 2 /* BetaCam lerp divisor during orbit (1=snap, higher=laggier) */
#define MOUSE_CAM_DEAD_ZONE 2         /* ignore deltas smaller than this (pixels, pre-scale) */
#define MOUSE_CAM_ZOOM_MULT 1         /* wheel zoom multiplier (applied to FOLLOW_CAM_ZOOM_STEP) */
#define MOUSE_CAM_SENS_DEFAULT 4      /* default sensitivity (1-10 range) */

/* Gamepad right-stick camera ------------------------------------------------*/
#define GAMEPAD_CAM_SENS_DEFAULT 5 /* default right-stick sensitivity (1-10 range) */
/* Defaults for the runtime GamepadCamMaxBeta / GamepadCamMaxAlpha globals
 * (cvar cam_stick_max_x / cam_stick_max_y, cfg keys of the same name). */
#define GAMEPAD_CAM_BETA_MAX_DEFAULT 36  /* orbit units per frame at full deflection, sens 10 (the ceiling) */
#define GAMEPAD_CAM_ALPHA_MAX_DEFAULT 10 /* elevation units per frame at full deflection, sens 10 (the ceiling) */
