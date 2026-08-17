#ifndef FOLLOWCAM_H
#define FOLLOWCAM_H

/* The Auto camera (user-facing name; config key FollowCamera, legacy
 * AutoCameraCenter): the third-person camera that tracks the hero in exterior
 * cubes.  Its per-frame update and the state that update carries between frames
 * live here; the tunables are in FOLLOWCAM_CFG.H and the angle arithmetic in
 * FOLLOWCAM_MATH.H.
 *
 * The main loop drives this through the four calls below rather than touching
 * the smoothing state directly, so that state stays private to the module. */

/* Per-frame update, exterior only. Spring-arm zoom, lazy orbit and pan drift,
 * HD recompose, terrain occlusion clearance, and the manual-override fade. */
extern void UpdateFollowCameraExt(void);

/* One line per frame describing where the camera is and who is deciding that
 * (console `camtrace`). Called every frame from the main loop, not from the
 * update above: the update runs only while the Auto camera holds the view, so a
 * trace written from in there could never report a camera zone, a cutscene, an
 * interior or the classic camera. */
extern void CamTrace(void);

/* Re-sync the smoothing state to the camera as it stands, after the view has
 * been recentred from outside (scene reset). Without this the arm length, the
 * rotation target and the ground lift are all left describing the old position,
 * and the first frame back glides in from it. */
extern void FollowCamResetToView(void);

/* Adopt the current BetaCam as the rotation target, for a caller that has just
 * set BetaCam itself (the quarter-turn camera key). Skipping this leaves the
 * lerp pulling back toward the pre-turn angle. */
extern void FollowCamAdoptCurrentAngle(void);

#endif /* FOLLOWCAM_H */
