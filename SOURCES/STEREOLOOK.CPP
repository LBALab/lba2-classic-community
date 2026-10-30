/*──────────────────────────────────────────────────────────────────────────*/
/* Quest-stereo head-look camera math.
 *
 * The tracked pose reaches the renderer as a layer on top of the
 * game-controlled rig: the rig keeps orbiting, zooming and streaming through
 * SetFollowCamera, and per eye the render orientation is the tracked view
 * (the StereoEye* angles, applied in AffGrilleExt) plus the tracked head
 * translation (StereoCamOffset*, a world-space offset on the rig position).
 * PERSO.CPP feeds these helpers from the OpenXR eye views; EXTFUNC.CPP
 * applies the result after the rig's SetFollowCamera.
 *
 * World-axis conventions match the engine's own 2D rotation (LongRotate,
 * 4096 angles per turn): a camera at BetaCam B looks along world direction
 * (sin(2048-B), cos(2048-B)) in (X, Z) — which is what the follow camera's
 * orbit and the first-person walk-where-you-look both already assume — and
 * view-space +X is the view's right (the rolled IPD shift in PERSO.CPP
 * rides on that same axis). */
#include "C_EXTERN.H"

#include <math.h>

/* Engine angles are 4096 per full turn. */
static const float LOOK_ENGINE_RAD = 3.14159265358979f / 2048.0f;
static const float LOOK_RAD_TO_ENGINE = 2048.0f / 3.14159265358979f;

/* World units per metre of tracked head motion.  The stereo IPD (30 world
   units, PERSO.CPP) is documented as ~1.5x natural IPD at LBA2's scale,
   which puts the metre at ~317 units. */
static float LookUnitsPerMetre(void) {
    S32 scale = VrWorldScale;
    if (scale < 50)
        scale = 50;
    else if (scale > 200)
        scale = 200;
    return 320.0f * 100.0f / (float)scale;
}

/* The tracked translation rides on the rig position, so it is a lean, not a
   teleport: clamp each metre axis before scaling. */
static const float LOOK_MAX_TRACKED_M = 0.5f;

/* Tracked head translation as a world-space offset on the rig camera
   position (SOURCES-side; EXTFUNC.CPP consumes it in AffGrilleExt). */
S32 StereoCamOffsetX = 0;
S32 StereoCamOffsetY = 0;
S32 StereoCamOffsetZ = 0;

/* View yaw for the tracked head: the rig framing captured when the look
   session engaged, plus the head's world-relative offset (radians). */
S32 StereoLookViewBeta(S32 baseBeta, float yawOff) {
    return (S32)((float)baseBeta + yawOff * LOOK_RAD_TO_ENGINE) & 4095;
}

/* View pitch for the tracked head: same composition with the sign the rig
   uses (raising AlphaCam looks further down). */
S32 StereoLookViewAlpha(S32 baseAlpha, float pitchOff) {
    return (S32)((float)baseAlpha - pitchOff * LOOK_RAD_TO_ENGINE);
}

/* Tracked head translation to a world-space camera offset.  Input in metres
   on the OpenXR tracking axes (X right, Y up, Z back); output in world
   units on the engine axes, expressed in the view whose yaw is betaView:
   forward lean goes along the view direction, sideways lean along the
   view's right.  Both frames are right-handed with Y up, so a pure yaw maps
   one onto the other, and that yaw is derived from the same BetaCam the eye
   override renders with — the lean stays consistent with what the head sees
   in every mode. */
void StereoLookHeadOffsetToWorld(float dxm, float dym, float dzm,
                                 S32 betaView, S32 *outX, S32 *outY,
                                 S32 *outZ) {
    float fwd, right, a, s, c;

    if (dxm > LOOK_MAX_TRACKED_M)
        dxm = LOOK_MAX_TRACKED_M;
    else if (dxm < -LOOK_MAX_TRACKED_M)
        dxm = -LOOK_MAX_TRACKED_M;
    if (dym > LOOK_MAX_TRACKED_M)
        dym = LOOK_MAX_TRACKED_M;
    else if (dym < -LOOK_MAX_TRACKED_M)
        dym = -LOOK_MAX_TRACKED_M;
    if (dzm > LOOK_MAX_TRACKED_M)
        dzm = LOOK_MAX_TRACKED_M;
    else if (dzm < -LOOK_MAX_TRACKED_M)
        dzm = -LOOK_MAX_TRACKED_M;

    fwd = -dzm * LookUnitsPerMetre();
    right = dxm * LookUnitsPerMetre();

    betaView &= 4095;
    a = (float)(2048 - betaView) * LOOK_ENGINE_RAD;
    s = sinf(a);
    c = cosf(a);
    /* view forward = (s, c); view right = (sin(a-pi/2), cos(a-pi/2)) =
       (-c, s) */
    *outX = (S32)(fwd * s - right * c);
    *outZ = (S32)(fwd * c + right * s);
    *outY = (S32)(dym * LookUnitsPerMetre());
}

/*──────────────────────────────────────────────────────────────────────────*/
/* Interior hover rig.
 *
 * An interior has no perspective camera: the room is a parallel projection
 * of a fixed isometric composite, so there is no eye to move inside it.  The
 * hover rig treats the head as a camera flying over that composite.  The
 * engine draws the room into the eye, so the image is locked to the face;
 * to hang it in the world instead, the view moves by exactly the angle the
 * head turned, in the eye's own projection: focal * tan(angle).  A fixed
 * gain cancels only part of the turn and the room still follows the head.
 * A head lean becomes a depth-correct parallax through the scene's own
 * single-pass eye delta, so near planes move further than far ones. */

/* Pixels of parallax per metre of tracked head lean.  Kept near the interior
   stereo baseline: a lean is a small viewpoint change, and a larger shift
   tears the painted wall/floor joins the per-pixel warp cannot fill. */
static const float HOVER_PAN_PX_PER_METRE = 48.0f;

/* tan() grows without bound at a right angle; past this the room is behind
   the viewer anyway, and the projection just needs a finite push. */
static const float HOVER_MAX_LOOK_RAD = 1.48352986f; /* 85 degrees */

/* Map a head pose, relative to the pose captured when the hover session
   engaged, to the isometric view's pan.  (focalX, focalY) are the eye's
   projection focal in pixels, so the shift is exactly the angle turned and
   the room stays where it was hung.  Signs: turning or leaning right moves
   the image left, looking or leaning up moves it down. */
void StereoHoverPoseToPan(float yawOff, float pitchOff, float dxm, float dym,
                          float focalX, float focalY, S32 frameW, S32 frameH,
                          S32 *outPanX, S32 *outPanY) {
    if (yawOff > HOVER_MAX_LOOK_RAD)
        yawOff = HOVER_MAX_LOOK_RAD;
    else if (yawOff < -HOVER_MAX_LOOK_RAD)
        yawOff = -HOVER_MAX_LOOK_RAD;
    if (pitchOff > HOVER_MAX_LOOK_RAD)
        pitchOff = HOVER_MAX_LOOK_RAD;
    else if (pitchOff < -HOVER_MAX_LOOK_RAD)
        pitchOff = -HOVER_MAX_LOOK_RAD;

    float panX = -focalX * tanf(yawOff) - dxm * HOVER_PAN_PX_PER_METRE;
    float panY = focalY * tanf(pitchOff) + dym * HOVER_PAN_PX_PER_METRE;
    /* The image is allowed to leave the view - that is what looking away
       from a fixed object does - but keep the projection arithmetic finite. */
    S32 limitX = frameW * 4;
    S32 limitY = frameH * 4;

    if (limitX < 0)
        limitX = 0;
    if (limitY < 0)
        limitY = 0;
    if (panX > (float)limitX)
        panX = (float)limitX;
    else if (panX < -(float)limitX)
        panX = -(float)limitX;
    if (panY > (float)limitY)
        panY = (float)limitY;
    else if (panY < -(float)limitY)
        panY = -(float)limitY;

    if (outPanX != NULL)
        *outPanX = (S32)panX;
    if (outPanY != NULL)
        *outPanY = (S32)panY;
}

/* Convert a hover pan in pixels to the world-space delta the single-pass
   eye stages consume.  A point at refAz then moves by exactly the pan,
   nearer pixels move further and farther ones less, which is what makes a
   lean read as depth rather than as a slide.  Zero when the projection has
   no usable focal (mono, or a degenerate frame). */
void StereoHoverPanToWorld(S32 panX, S32 panY, float focalX, float focalY,
                           float zScale, S32 refAz, S32 *outX, S32 *outY) {
    if (outX != NULL)
        *outX = 0;
    if (outY != NULL)
        *outY = 0;
    if (focalX <= 0.0f || zScale <= 0.0f || refAz <= 0)
        return;
    if (outX != NULL)
        *outX = (S32)(-(float)panX * (float)refAz / (focalX * zScale));
    if (outY != NULL && focalY != 0.0f)
        *outY = (S32)(-(float)panY * (float)refAz /
                      (focalX * focalY * zScale));
}
