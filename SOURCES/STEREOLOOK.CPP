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
static const float LOOK_UNITS_PER_METRE = 320.0f;

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

    fwd = -dzm * LOOK_UNITS_PER_METRE;
    right = dxm * LOOK_UNITS_PER_METRE;

    betaView &= 4095;
    a = (float)(2048 - betaView) * LOOK_ENGINE_RAD;
    s = sinf(a);
    c = cosf(a);
    /* view forward = (s, c); view right = (sin(a-pi/2), cos(a-pi/2)) =
       (-c, s) */
    *outX = (S32)(fwd * s - right * c);
    *outZ = (S32)(fwd * c + right * s);
    *outY = (S32)(dym * LOOK_UNITS_PER_METRE);
}
