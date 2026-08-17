#ifndef FOLLOWCAM_H
#define FOLLOWCAM_H

#include <SYSTEM/ADELINE_TYPES.H>

/* The Auto camera (user-facing name; config key FollowCamera, legacy
 * AutoCameraCenter): the third-person camera that tracks the hero in exterior
 * cubes.  Its per-frame update, the state that update carries between frames,
 * and the settings that tune it live in FOLLOWCAM.CPP; the tunable defaults are
 * in FOLLOWCAM_CFG.H and the angle arithmetic in FOLLOWCAM_MATH.H.
 *
 * This header declares exactly what FOLLOWCAM.CPP defines, so a caller that
 * reaches for Auto-camera state includes this and the dependency is visible in
 * its include list.  The manual camera sources that feed it (mouse drag,
 * gamepad right stick) are EXTFUNC.CPP's, and their globals are still declared
 * in C_EXTERN.H.
 *
 * The main loop drives the camera through the four calls below rather than
 * touching the smoothing state directly; that state stays private to the .CPP. */

/* Per-frame update, exterior only. Spring-arm zoom, lazy orbit and pan drift,
 * HD recompose, terrain occlusion clearance, and the manual-override fade. */
extern void UpdateFollowCameraExt(void);

/* One line per frame describing where the camera is and who is deciding that
 * (console `camtrace`). Called every frame from the main loop, not from the
 * update above: the update runs only while the Auto camera holds the view, so a
 * trace written from in there could never report a camera zone, a cutscene, an
 * interior or the classic camera. */
extern void FollowCamTrace(void);

/* Re-sync the smoothing state to the camera as it stands, after the view has
 * been recentred from outside (scene reset). Without this the arm length, the
 * rotation target and the ground lift are all left describing the old position,
 * and the first frame back glides in from it.
 *
 * This also puts the arm back to FOLLOW_CAM_INITIAL_DIST, discarding whatever
 * the player had zoomed to. That is what Center camera is expected to do, but it
 * makes this the wrong call for anything that merely wants the smoothing state
 * re-synced: a cube transition using it would reset every player's zoom on every
 * scene change. */
extern void FollowCamResetToView(void);

/* Adopt the current BetaCam as the rotation target, for a caller that has just
 * set BetaCam itself (the quarter-turn camera key). Skipping this leaves the
 * lerp pulling back toward the pre-turn angle.
 *
 * Not to be confused with EXTFUNC's FollowCamAdoptAngle(), which solves the
 * other way round: it writes AddBetaCam so the pan offset matches where the
 * camera already is. This one writes the target the lerp is heading for. */
extern void FollowCamSyncTarget(void);

/* Settings. Read from the cfg by ReadConfigFile and written back by
 * WriteConfigFile (both PERSO.CPP); most are also console cvars. Defaults in
 * FOLLOWCAM_CFG.H. */
extern S32 FollowCamera;              /* the Auto camera is on (config key FollowCamera) */
extern S32 FollowCamReengageFrames;   /* frames before auto-cam re-centers after a manual pan */
extern S32 FollowCamDriftDivisor;     /* auto-cam re-center drift divisor (higher = gentler) */
extern S32 FollowCamHDRecompose;      /* pull-in+steepen recompose at tall render heights (1=on) */
extern S32 FollowCamHDExcessCap;      /* cap on the recompose kExcess so tall res converges (0 = no cap) */
extern S32 FollowCamHDPitchGain;      /* HD recompose pitch gain (AlphaCam units per 1.0 of k-1) */
extern S32 FollowCamHDDistGain;       /* HD recompose distance pull (% boom per 1.0 of k-1) */
extern S32 FollowCamHDLeanGain;       /* HD recompose forward-lean cut (% per 1.0 of k-1) */
extern S32 FollowCamGround;           /* smooth terrain ground-clearance lift (1=on) */
extern S32 FollowCamGroundClearance;  /* keep the eye this far above the ground under it */
extern S32 FollowCamManualHoldFrames; /* manual-override hold length (cvar cam_manual_hold) */
extern S32 FollowCamHoldAngle;        /* hold manual rotation, no auto-recenter drift (cvar cam_hold_angle) */

/* Live state other TUs read or write. The rest of the smoothing state is
 * private to FOLLOWCAM.CPP. */
extern S32 FollowCamBaseDist;      /* player's chosen zoom distance (0 = uninitialised) */
extern S32 FollowCamReengageDelay; /* frames before auto-center resumes after manual pan */
extern S32 FollowCamManualHold;    /* frames the pitch/distance auto-assist yields after a manual nudge */

/* Console `camtrace`: log the Auto-camera angle state each frame. */
extern S32 DbgCamTrace;

/* The cfg keys this module owns: read them into the settings above, clamped, and
 * write them back. CONFIG_FILE.CPP calls these instead of naming the keys
 * itself, so a setting added here needs no edit there and its clamp cannot end
 * up in a different file from the arithmetic that requires it.
 *
 * Not the whole set: the `FollowCamera` master toggle keeps its own position in
 * the file, because these run as one contiguous block and moving it would
 * reorder every player's lba2.cfg on the next write. */
extern void FollowCam_ReadConfig(void);
extern void FollowCam_WriteConfig(void);

#endif /* FOLLOWCAM_H */
