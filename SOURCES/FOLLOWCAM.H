#ifndef FOLLOWCAM_H
#define FOLLOWCAM_H

#include <SYSTEM/ADELINE_TYPES.H>

struct T_SETTING; /* SETTINGS.H; a pointer to a table needs no more */

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
 * The main loop drives the camera through the calls below rather than touching
 * the smoothing state directly; that state stays private to the .CPP. */

/* Per-frame update, exterior only. Spring-arm zoom, lazy orbit and pan drift,
 * HD recompose, terrain occlusion clearance, and the manual-override fade. */
extern void UpdateFollowCameraExt(void);

/* A scene change has put the hero in a new outdoor scene and the camera has to
 * be put somewhere. With cam_scene_carry on and the Auto camera holding the
 * view, this aims it behind him at the pitch the player already had, and at his
 * pan too when the scene left was also outdoors (out of an interior the pan
 * goes home), and returns TRUE, and ChangeCube skips its own CameraCenter. In
 * every other case it does nothing and returns FALSE: the setting off, the
 * classic camera, an interior, or a load, whose camera comes from the save
 * file. */
extern S32 FollowCamCarryIntoScene(void);

/* For camtrace: the followed object was just drawn fully hidden, and what was
 * done about it (1 nothing, 2 the clearance hurried, 3 recentred). The largest
 * value since the last trace line is the one it prints. */
extern void FollowCamNoteHidden(S32 what);

/* cam_obstruct. TRUE while the Auto camera's eased clearance stands in for the
 * classic obstruction search: the setting is on and the Auto camera has the
 * view outdoors. CameraCenter then leaves SearchCameraPos out, so nothing
 * writes a pitch into AlphaCam that the player did not set. */
extern S32 FollowCamObstructOwnsSearch(void);

/* cam_obstruct. The pitch to add to AlphaCam wherever the exterior camera is
 * set from the globals: CameraCenter and the scene render. 0 unless the setting
 * is on and the Auto camera has the view outdoors, so those callers are
 * unchanged without it. */
extern S32 FollowCamRenderExtraPitch(void);

/* Set the exterior camera for the frame from the globals: CameraCenter and the
 * scene render call this instead of SetFollowCamera. With cam_shot_blend_ms on
 * and a scripted shot having just taken the view or given it back, it sets the
 * blend between the view before and the one the globals describe; otherwise
 * exactly the globals, with cam_obstruct's extra pitch added. */
extern void FollowCamSetView(void);

/* cam_shot_blend_ms. A camera zone is about to write its shot into the globals:
 * blend into it from the view on screen, and blend out of it on the next update
 * the Auto camera runs. Called before the zone writes anything. */
extern void FollowCamShotTookView(void);

/* cam_shot_blend_ms. The shot just applied is to be shown whole on the next
 * frame, with no blend into it: a sign's shot, drawn once and then held still
 * under its message. The blend out of it, when the Auto camera takes the view
 * back, is unaffected. */
extern void FollowCamShotAtOnce(void);

/* cam_shot_blend_ms. TRUE while a blend runs. The scene is redrawn whole on
 * every frame of one, since the view moves whether or not the hero does. */
extern S32 FollowCamBlending(void);

/* cam_obstruct. A scripted shot has just been applied. It starts as written:
 * the extra and any floor learnt for the view before it are dropped. */
extern void FollowCamObstructShotFired(void);

/* cam_obstruct. A re-aim or an arrival has just placed the camera with a cut.
 * Have the clearance start the next update at what the place needs, instead of
 * easing up to it from wherever the last view left it. */
extern void FollowCamObstructSettle(void);

/* cam_obstruct. The hero has just been drawn fully hidden. TRUE when the eased
 * clearance knows what is in the way and is still raising the camera over it:
 * it then hurries, and the caller leaves the classic recentre out. FALSE when
 * it has no answer, or has given all of it and he is hidden still, and the
 * classic recentre is the only one left. */
extern S32 FollowCamObstructHidden(void);

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

/* For cam_world_anchor: AddBetaCam has just been solved against the hero's
 * facing as it is now (EXTFUNC's FollowCamAdoptAngle), so the hero's turn since
 * the last update is already in it and is not to be taken out again. */
extern void FollowCamWorldAnchorRebase(void);

/* A scripted shot's loan (cam_zone_return, on by default; cam_zone_anchor, off).
 *
 * A camera zone takes the yaw and the pitch from whoever had them. With either
 * setting on, the Auto camera remembers what it had so the update can give it
 * back when the hero leaves the shot's box, and holds the shot's own yaw while
 * he is inside it. With both off none of these does anything, and the shot's
 * leftovers stay with the camera as they always have.
 *
 * FollowCamLendToShot is called by the zone before it writes its angles, since
 * what has to be remembered is what they are about to replace. */
extern void FollowCamLendToShot(S32 shotBeta);

/* The player set that axis by hand, so it is theirs now and is not given back.
 * For the pitch, call it after the tilt has been applied and clamped: the value
 * AlphaCam holds then is remembered as the camera's own. */
extern void FollowCamPlayerTook(S32 yaw, S32 pitch);

/* The pitch the Auto camera rests at when nobody else has it. A recentre tells
 * it the preset it has just set, before the obstruction search can replace it,
 * and a load tells it the saved one. A shot's loan is taken out against this
 * and returned to it, in place of whatever AlphaCam held when the shot fired. */
extern void FollowCamNoteOwnPitch(S32 alpha);

/* That pitch, or -1 while none is known. For a caller whose recentre is the
 * engine's doing and not the player's, to put back what CameraCenter noted. */
extern S32 FollowCamOwnPitch(void);

/* Nothing is owed any more: a recentre, a scene change that recentres, a
 * cutscene, a loaded save or the Auto camera being switched off decided where
 * the camera points. Walking through a door does not: the pan and the pitch
 * survive a visit to an interior, so the loan does too. */
extern void FollowCamForgetLoan(void);

/* Told once per simulated frame whether the scene is an exterior, so the loan
 * can see the frame the hero comes out of an interior. Called from the main
 * loop after that frame's zone check, because the zone over a door fires as he
 * arrives and what it lends has to be on the books before this looks at them. */
extern void FollowCamNoteScene(S32 exterior);

/* Once a frame, beside FollowCamNoteScene: whether a cutscene has the camera.
 * A shot that fires as one ends owes back "behind the hero" for the yaw, not
 * the pan the cutscene's own shots left. */
extern void FollowCamNoteCinema(S32 cinema);

/* Once a simulation step, before the update, whether or not the update then
 * runs. The eases paced by the clock (cam_ease_ms, cam_target_ms) take the time since the
 * previous update for a step's length, and this is how the update knows that
 * the previous one was on the step before: a camera zone, a cutscene or a
 * building in between and the time is not a step's, and is not spent. Called
 * once per simulation step, so a rendered frame that skips the simulation (above
 * ~60 fps with the fixed step on) is no gap. */
extern void FollowCamNoteFrame(void);

/* Set by CheckZoneSce on each frame the followed object stands in the box of a
 * camera zone that has fired, and cleared with CameraZone at the top of the
 * frame. CameraZone itself cannot say this: for a zone that fires once it is
 * true for a single frame, however long the hero stays in the box. */
extern S32 FollowCamInShotBox;

/* Settings. Read from the cfg by ReadConfigFile and written back by
 * WriteConfigFile (both PERSO.CPP); most are also console cvars. Defaults in
 * FOLLOWCAM_CFG.H. */
extern S32 FollowCamera;                  /* the Auto camera is on (config key FollowCamera) */
extern S32 FollowCamReengageFrames;       /* frames before auto-cam re-centers after a manual pan */
extern S32 FollowCamDriftDivisor;         /* auto-cam re-center drift divisor (higher = gentler) */
extern S32 FollowCamSceneCarry;           /* start behind the hero in a new outdoor scene, pitch kept (cvar cam_scene_carry) */
extern S32 FollowCamZoneSkip;             /* leave latching camera zones unfired while the player steers (cvar cam_zone_skip) */
extern S32 FollowCamObstruct;             /* eased obstruction clearance in place of the classic search (cvar cam_obstruct) */
extern S32 FollowCamObstructHurryDivisor; /* cam_obstruct: ease divisor while the hero is hidden (cvar cam_obstruct_hurry) */
extern S32 FollowCamHDRecompose;          /* pull-in+steepen recompose at tall render heights (1=on) */
extern S32 FollowCamHDExcessCap;          /* cap on the recompose kExcess so tall res converges (0 = no cap) */
extern S32 FollowCamHDPitchGain;          /* HD recompose pitch gain (AlphaCam units per 1.0 of k-1) */
extern S32 FollowCamHDDistGain;           /* HD recompose distance pull (% boom per 1.0 of k-1) */
extern S32 FollowCamHDLeanGain;           /* HD recompose forward-lean cut (% per 1.0 of k-1) */
extern S32 FollowCamGround;               /* smooth terrain ground-clearance lift (1=on) */
extern S32 FollowCamGroundClearance;      /* keep the eye this far above the ground under it */
extern S32 FollowCamManualHoldFrames;     /* manual-override hold length (cvar cam_manual_hold) */
extern S32 FollowCamHoldAngle;            /* hold manual rotation, no auto-recenter drift (cvar cam_hold_angle) */
extern S32 FollowCamZoneReturn;           /* give a shot's yaw and pitch back on leaving its box (cvar cam_zone_return) */
extern S32 FollowCamZoneAnchor;           /* hold a latching shot's world yaw while inside its box (cvar cam_zone_anchor) */
extern S32 FollowCamWorldAnchor;          /* hold the camera's world yaw while the hero turns (cvar cam_world_anchor) */
extern S32 FollowCamWorldReturn;          /* cam_world_anchor with cam_hold_angle 0: units walked that pace the return behind the hero (cvar cam_world_return) */
extern S32 FollowCamEaseMs;               /* time constant of the yaw's ease behind the hero, ms; 0 = per-frame lerp (cvar cam_ease_ms) */
extern S32 FollowCamTargetMs;             /* time constant of the look-at point's ease, ms; 0 = pinned to the hero (cvar cam_target_ms) */
extern S32 FollowCamShotBlendMs;          /* blend into and out of a scripted shot, ms; 0 = cut (cvar cam_shot_blend_ms) */

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

/* The table itself, for what walks every setting: the recording header. */
extern const T_SETTING *FollowCam_Settings(S32 *count);

/* The console cvars and commands this module owns, registered from
 * Console_RegisterAll. Same reason as the cfg pair: a setting added here is
 * added in one file, and the console does not need to know the name of a camera
 * variable to expose it. */
extern void FollowCam_RegisterCvars(void);
extern void FollowCam_RegisterCommands(void);

#endif /* FOLLOWCAM_H */
