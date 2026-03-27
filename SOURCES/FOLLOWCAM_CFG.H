#pragma once
/* Build-time tuning constants for the FollowCamera (community addition).
 * This follow camera was never part of the original game.
 * Change these values to adjust camera feel without touching the logic. */

/* Arm length vs terrain (penetration only) ----------------------------------*/
#define FOLLOW_CAM_SPRING_MIN          200  /* minimum arm length (world units) */
#define FOLLOW_CAM_SPRING_RECOVER      200  /* recover toward zoom after penetration clears */
#define FOLLOW_CAM_CLEAR_MARGIN        400  /* require CameraY >= ground + margin at camera (X,Z) */
#define FOLLOW_CAM_PEN_STEP            320  /* max arm shorten per frame when penetrating (smaller = smoother) */
#define FOLLOW_CAM_ALPHA_ESCAPE_STEP   18   /* elevation per frame at min arm when still penetrating */

/* Rotation lag (BetaCam lerp) -----------------------------------------------*/
#define FOLLOW_CAM_ROT_DIVISOR         36   /* lerp denominator (higher = slower) */
#define FOLLOW_CAM_ROT_MIN_STEP        3    /* minimum BetaCam step per lerp tick (units) */
#define FOLLOW_CAM_ROT_THRESHOLD       10   /* snap BetaCam when within this many units of target */

/* Pan drift (AddBetaCam drift toward 0 after [ / ] released) ----------------*/
#define FOLLOW_CAM_REENGAGE_FRAMES     15   /* grace frames before follow resumes after pan */
#define FOLLOW_CAM_DRIFT_DIVISOR       16   /* drift denominator: 1/16 ≈ 6 % per frame */
#define FOLLOW_CAM_DRIFT_MIN_STEP      4    /* minimum AddBetaCam step per drift tick (units) */
#define FOLLOW_CAM_DRIFT_THRESHOLD     5    /* snap AddBetaCam to 0 within this many units */

/* Elevation (AlphaCam) — applies when FollowCamera is on -------------------*/
#define FOLLOW_CAM_ALPHA_MIN           150  /* minimum elevation angle */
#define FOLLOW_CAM_ALPHA_MAX           600  /* maximum elevation angle */
#define FOLLOW_CAM_ALPHA_STEP          5    /* elevation change per numpad +/- key press */

/* Zoom (numpad * / /) — adjusts default arm distance -----------------------*/
#define FOLLOW_CAM_ZOOM_STEP           200  /* distance change per frame while key held */
#define FOLLOW_CAM_DIST_MIN            7000 /* closest the player can zoom in (must clear hero mesh) */
#define FOLLOW_CAM_DIST_MAX            20000 /* furthest the player can zoom out */

/* Horizontal pan keys [ ] ---------------------------------------------------*/
#define FOLLOW_CAM_PAN_STEP            30   /* AddBetaCam change per [ / ] key press per frame */
