/*──────────────────────────────────────────────────────────────────────────*/
/* First-person stereo body and independent hand IK.
 *
 * The gameplay body stays hidden; this module keeps a ghost copy of the real
 * Twinsen body, aligns its head joint with the tracked head, and solves a
 * two-bone arm IK per tracked hand.  Joint positions and segment lengths come
 * from the body's own group hierarchy (BODY.HQR) and current animation frame,
 * not from hardcoded rest offsets, so the normal/tunic bodies both work.
 *
 * Optical hand joints (XR_EXT_hand_tracking) drive the wrists when available;
 * otherwise the controller grip pose is the fallback.  The two hands are
 * solved independently. */

#include "C_EXTERN.H"

#include <SYSTEM/OPENXR.H>
#include <SYSTEM/LOG.H>
#include <OBJECT/AFF_OBJ.H>
#include <SVGA/CLIP.H>
#include <3D/CAMERA.H>
#include <3D/DATAMAT.H>
#include <3D/ROT3D.H>
#include <3D/ROTMAT.H>
#include <3D/IMATSTD.H>
#include <3D/IMATTRA.H>
#include <3D/COPYMAT.H>
#include <3D/ROTRALIS.H>
#include "3DEXT/VAR_EXT.H"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define VR_ANGLE_PER_RAD (2048.0f / 3.14159265358979f)
#define VR_PI 3.14159265358979f
#define VR_MAX_POINTS 550
#define VR_MAX_GROUPS 30

/* Private copy of the body group descriptor (AFF_OBJ keeps it TU-local). */
typedef struct {
    U16 OrgGroupe;
    U16 OrgPoint;
    U16 NbPts;
    U16 NbNorm;
} VR_GROUP;

/* Body group indices in the tunic/normal topology.  These are validated by
 * the group debug view; the joints themselves are measured from the body. */
#define VR_GROUP_HEAD 14
#define VR_GROUP_ARM_L_UPPER 4
#define VR_GROUP_ARM_L_FORE 5
#define VR_GROUP_ARM_R_UPPER 6
#define VR_GROUP_ARM_R_FORE 7

/* ObjectDisplay starts the group animation pointer at CurrentFrame - 1, so
 * group g (g >= 1) reads CurrentFrame[g - 1]; group 0 is the root object. */
#define VR_ANIM(g) ((g) - 1)

S32 VrFullBody = 0;
S32 VrWorldScale = 100;
S32 VrBodyHideGroups = -1; /* -1 = per-body default, else explicit mask */

static S32 s_vrActive = 0;
static S32 s_vrReady = 0;
static S32 s_vrArmValid[2] = {0, 0};
static S32 s_vrArmAngles[2][2][3];
/* Palm position of the drawn ghost hand, in world units.  The throw state
   reads last frame's palm, the same one-frame lag as the body. */
static float s_vrPalmWorld[2][3];
static S32 s_vrPalmValid[2] = {0, 0};
/* Hand target smoothed in body-local space.  Body-local is the right frame:
 * it filters tracking noise without lagging a body turn (the turn rotates
 * the frame, not the target within it). */
static float s_smoothOff[2][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
static S32 s_smoothInit[2] = {0, 0};
/* Swivel pole smoothed in the torso frame so a stick turn carries it
 * rigidly and only real wrist rotation changes it. */
static float s_poleSmooth[2][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
static S32 s_poleInit[2] = {0, 0};

static S32 s_vrBodyHqr = -1;
static T_OBJET *s_vrHero = NULL;
static T_OBJ_3D s_vrGhost;

/* When the tracked hand is farther than the model's arm can reach, draw the
   ghost from a private body copy whose arm groups are scaled along their
   bones.  The copy is refreshed from the HQR body every frame, so no state
   survives a body swap. */
#define VR_MAX_ARM_STRETCH 2.0f
#define VR_BODY_COPY_PAD 64
static U8 *s_vrBodyCopy = NULL;
static U32 s_vrBodyCopySize = 0;
static S32 s_vrBodyCopyActive = 0;
/* Body yaw eases toward the view yaw.  A rigid body that shares the camera
 * yaw is locked to the screen during a stick turn, so only the walk clip
 * appears to move; the short catch-up makes the torso and hands visibly
 * rotate into place when the view turns. */
static S32 s_vrBodyBeta = 0;
static S32 s_vrBodyBetaInit = 0;

/* Throwing hand, refreshed once per rendered frame for the simulation's
 * throw input.  Origin is in world units; velocity is world units/second;
 * aim is a world unit vector.  The throw input runs between renders, so it
 * reads the previous frame's tracking, the same one-frame lag as the body. */
static float s_vrThrowOrigin[2][3];
static float s_vrThrowVelocity[2][3];
static float s_vrThrowAim[2][3];
static S32 s_vrThrowHandValid[2] = {0, 0};
static float s_vrThrowPrevPos[2][3];
static S32 s_vrThrowPrevValid[2] = {0, 0};
static U32 s_vrThrowPrevTime = 0;
/* The press arms a physical throw; the release uses the swing captured
 * while the button was held. */
static S32 s_vrThrowArmed = 0;

/* Visual-only magic ball shown in the throwing hand while armed.  It is a
   normal extra drawn by the usual sprite pass, but flagged as waiting and
   positioned on the hand every frame; the real ball is created on release. */
static S32 s_vrHeldBall = -1;
static S32 s_vrHeldHand = -1;
#define VR_THROW_HAND_FORWARD 120.0f
static void VrCreateHeldBall(void);

/* A real swing, in engine units/second (roughly 0.8 m/s at the default
 * world scale).  Below it the throw follows the controller aim.  The
 * velocity decays over ~80 ms so a gesture that ends just before the
 * release still selects its direction. */
#define VR_THROW_SWING_SPEED 560.0f
#define VR_THROW_VEL_SMOOTH_MS 80.0f

typedef struct {
    float m[3][3];
} VR_MAT3;

typedef struct {
    TYPE_MAT mat[VR_MAX_GROUPS];
    TYPE_VT16 origin[VR_MAX_GROUPS];
    TYPE_VT16 rot[VR_MAX_POINTS];
    /* Start of each group's point slice in the flat point array.  Bodies
     * store group points consecutively in group order; OrgPoint is only the
     * pivot point index, which usually lives in an earlier group. */
    U16 slice[VR_MAX_GROUPS];
} VR_RIG;

typedef struct {
    S32 x, y, z;
} VR_INT3;

static float VrUnitsPerMetre(void) {
    S32 scale = VrWorldScale;
    if (scale < 50)
        scale = 50;
    else if (scale > 200)
        scale = 200;
    /* Twinsen's body is 1240 units tall for ~1.75 m, so the hands have to
       reach at ~700 units/m to land on his shoulders and chest.  The stereo
       IPD is deliberately stylised (30 units) and is not a body measure.
       A larger world scale makes the same physical metre span fewer units. */
    return 700.0f * 100.0f / (float)scale;
}

static void VrMatIdentity(VR_MAT3 *m) {
    memset(m, 0, sizeof(*m));
    m->m[0][0] = 1.0f;
    m->m[1][1] = 1.0f;
    m->m[2][2] = 1.0f;
}

/* Rotation taking direction r onto direction d, stored exactly as an
 * engine matrix: RotTransList applies it as a column transform (M v). */
static void VrMatFromTo(const float r[3], const float d[3], VR_MAT3 *out) {
    float rl = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    float dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    float rn[3], dn[3], axis[3], c, s;
    VR_MAT3 col;
    S32 i;

    if (rl < 1e-6f || dl < 1e-6f) {
        VrMatIdentity(out);
        return;
    }
    for (i = 0; i < 3; i++) {
        rn[i] = r[i] / rl;
        dn[i] = d[i] / dl;
    }
    c = rn[0] * dn[0] + rn[1] * dn[1] + rn[2] * dn[2];
    if (c > 1.0f) {
        c = 1.0f;
    } else if (c < -1.0f) {
        c = -1.0f;
    }
    axis[0] = rn[1] * dn[2] - rn[2] * dn[1];
    axis[1] = rn[2] * dn[0] - rn[0] * dn[2];
    axis[2] = rn[0] * dn[1] - rn[1] * dn[0];
    s = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] +
              axis[2] * axis[2]);

    if (s < 1e-6f) {
        if (c > 0.0f) {
            VrMatIdentity(out);
            return;
        }
        axis[0] = -rn[1];
        axis[1] = rn[0];
        axis[2] = 0.0f;
        if (axis[0] * axis[0] + axis[1] * axis[1] < 1e-6f) {
            axis[0] = 1.0f;
            axis[1] = 0.0f;
        }
        s = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] +
                  axis[2] * axis[2]);
        axis[0] /= s;
        axis[1] /= s;
        axis[2] /= s;
        s = 0.0f;
        c = -1.0f;
    } else {
        axis[0] /= s;
        axis[1] /= s;
        axis[2] /= s;
    }
    {
        const float t = 1.0f - c;
        const float ax = axis[0], ay = axis[1], az = axis[2];
        col.m[0][0] = c + ax * ax * t;
        col.m[0][1] = ax * ay * t - az * s;
        col.m[0][2] = ax * az * t + ay * s;
        col.m[1][0] = ay * ax * t + az * s;
        col.m[1][1] = c + ay * ay * t;
        col.m[1][2] = ay * az * t - ax * s;
        col.m[2][0] = az * ax * t - ay * s;
        col.m[2][1] = az * ay * t + ax * s;
        col.m[2][2] = c + az * az * t;
    }
    /* RotTransList applies the matrix as a column transform (M v + T), and
       InitMatrixStd stores the same column matrix, so keep col as-is. */
    *out = col;
}

static S32 VrAngle(float rad) {
    S32 a = (S32)lrintf(rad * VR_ANGLE_PER_RAD);
    return a & 4095;
}

static void VrMatToEuler(const VR_MAT3 *m, S32 *alpha, S32 *beta, S32 *gamma) {
    const float m00 = m->m[0][0], m01 = m->m[0][1], m02 = m->m[0][2];
    const float m11 = m->m[1][1], m12 = m->m[1][2];
    const float m21 = m->m[2][1];
    const float cg = sqrtf(m00 * m00 + m02 * m02);

    if (cg > 1e-6f) {
        /* InitMatrixStd stores M(Alpha)*M(Gamma)*M(Beta): beta is
           atan2(M13, M11), gamma is atan2(-M12, |Cg|), and alpha is
           atan2(M32, M22). */
        *gamma = VrAngle(atan2f(-m01, cg));
        *alpha = VrAngle(atan2f(m21, m11));
        *beta = VrAngle(atan2f(m02, m00));
    } else {
        *beta = 0;
        *alpha = VrAngle(atan2f(-m12, m11));
        *gamma = 0;
    }
}

/* Rotate a vector by a quaternion (x,y,z,w). */
static void VrQuatRotate(const float q[4], const float v[3], float out[3]) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float tx = 2.0f * (y * v[2] - z * v[1]);
    const float ty = 2.0f * (z * v[0] - x * v[2]);
    const float tz = 2.0f * (x * v[1] - y * v[0]);
    out[0] = v[0] + w * tx + (y * tz - z * ty);
    out[1] = v[1] + w * ty + (z * tx - x * tz);
    out[2] = v[2] + w * tz + (x * ty - y * tx);
}

static void VrNormalize(float v[3]) {
    float n = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (n < 1e-6f) {
        v[0] = 0.0f;
        v[1] = -1.0f;
        v[2] = 0.0f;
        return;
    }
    v[0] /= n;
    v[1] /= n;
    v[2] /= n;
}

/* Tracking space is +X right, +Y up, -Z forward, with yaw unrelated to the
 * game world.  The body maps it to body-local +X left, +Z front, then turns
 * by the body yaw; a vector uses the same mapping as a hand offset. */
static void VrTrackingToWorld(const float v[3], S32 beta, float out[3]) {
    const float a = (float)beta * (VR_PI / 2048.0f);
    const float c = cosf(a);
    const float s = sinf(a);
    const float lx = -v[0];
    const float lz = -v[2];
    out[0] = c * lx + s * lz;
    out[1] = v[1];
    out[2] = -s * lx + c * lz;
}

static void VrDropHeldBall(void) {
    if (s_vrHeldBall >= 0 && s_vrHeldBall < MAX_EXTRAS) {
        ListExtra[s_vrHeldBall].Sprite = -1;
    }
    s_vrHeldBall = -1;
    s_vrHeldHand = -1;
}

static void VrPlaceHeldBall(void) {
    if (s_vrHeldBall < 0) {
        /* The slot can be cleared by a scene change while the grip is held;
           recreate it as soon as a hand is tracked again. */
        if (s_vrThrowArmed &&
            (s_vrThrowHandValid[0] || s_vrThrowHandValid[1]))
            VrCreateHeldBall();
        return;
    }
    if (s_vrHeldBall >= MAX_EXTRAS) {
        s_vrHeldBall = -1;
        return;
    }
    T_EXTRA *held = &ListExtra[s_vrHeldBall];
    if (held->Sprite != (S16)MagicBallSprite[MagicLevel] ||
        held->Owner != 255 ||
        s_vrHeldHand < 0 || !s_vrThrowHandValid[s_vrHeldHand] ||
        !s_vrThrowArmed) {
        VrDropHeldBall();
        return;
    }
    const S32 hand = s_vrHeldHand;
    held->PosX = (S32)s_vrThrowOrigin[hand][0];
    held->PosY = (S32)s_vrThrowOrigin[hand][1];
    held->PosZ = (S32)s_vrThrowOrigin[hand][2];
}

static void VrUpdateThrowState(void) {
    s_vrThrowHandValid[0] = 0;
    s_vrThrowHandValid[1] = 0;

    static S32 s_disabled = -1;
    if (s_disabled < 0)
        s_disabled = getenv("LBA2_NO_VR_THROW") ? 1 : 0;
    if (s_disabled) {
        s_vrThrowPrevValid[0] = 0;
        s_vrThrowPrevValid[1] = 0;
        s_vrThrowArmed = 0;
        VrDropHeldBall();
        return;
    }

    if (!CameraFirstPerson || !DisplayStereo3D || !OpenXR_IsActive() ||
        CubeMode != CUBE_EXTERIEUR || CinemaMode ||
        Weapon != FLAG_BALLE_MAGIQUE) {
        s_vrThrowPrevValid[0] = 0;
        s_vrThrowPrevValid[1] = 0;
        s_vrThrowArmed = 0;
        VrDropHeldBall();
        return;
    }

    float q0[4], p0[3], q1[4], p1[3];
    if (!OpenXR_GetEyeView(0, q0, p0) || !OpenXR_GetEyeView(1, q1, p1))
        return;
    const float trackingHead[3] = {
        0.5f * (p0[0] + p1[0]),
        0.5f * (p0[1] + p1[1]),
        0.5f * (p0[2] + p1[2])};

    const S32 beta =
        s_vrBodyBetaInit ? s_vrBodyBeta : ((2048 - BetaCam) & 4095);
    const float headWorld[3] = {
        (float)(VueOffsetX + StereoCamOffsetX),
        (float)(VueOffsetY + StereoCamOffsetY),
        (float)(VueOffsetZ + StereoCamOffsetZ)};
    const float units = VrUnitsPerMetre();

    const U32 now = (U32)SDL_GetTicks();
    U32 dtMs = now - s_vrThrowPrevTime;
    if (dtMs == 0)
        dtMs = 1;
    else if (dtMs > 100)
        dtMs = 100;

    for (S32 hand = 0; hand < 2; hand++) {
        float q[4], p[3];
        if (!OpenXR_GetHandPose(hand, 1, q, p) &&
            !OpenXR_GetHandJointPose(hand, 1, q, p)) {
            s_vrThrowPrevValid[hand] = 0;
            continue;
        }

        const float off[3] = {
            p[0] - trackingHead[0],
            p[1] - trackingHead[1],
            p[2] - trackingHead[2]};
        float worldOff[3];
        /* A tracking offset is a room vector, so it maps to the world without
           a yaw of its own: the pose it comes from is already room-fixed. */
        VrTrackingToWorld(off, 0, worldOff);
        if (s_vrPalmValid[hand]) {
            /* Use the hand the player can actually see.  The palm position
               is last frame's IK, the same one-frame lag as the body. */
            s_vrThrowOrigin[hand][0] = s_vrPalmWorld[hand][0];
            s_vrThrowOrigin[hand][1] = s_vrPalmWorld[hand][1];
            s_vrThrowOrigin[hand][2] = s_vrPalmWorld[hand][2];
        } else {
            s_vrThrowOrigin[hand][0] = headWorld[0] + worldOff[0] * units;
            s_vrThrowOrigin[hand][1] = headWorld[1] + worldOff[1] * units;
            s_vrThrowOrigin[hand][2] = headWorld[2] + worldOff[2] * units;
        }

        /* The ready orb sits inside the palm point, toward the body: drawn on
           the exact palm it reads as past the hand. */
        {
            const float toBody[3] = {
                headWorld[0] - s_vrThrowOrigin[hand][0],
                headWorld[1] - s_vrThrowOrigin[hand][1],
                headWorld[2] - s_vrThrowOrigin[hand][2]};
            const float pull = 50.0f;
            const float dist = sqrtf(toBody[0] * toBody[0] +
                                     toBody[1] * toBody[1] +
                                     toBody[2] * toBody[2]);
            if (dist > pull) {
                const float k = pull / dist;
                s_vrThrowOrigin[hand][0] += toBody[0] * k;
                s_vrThrowOrigin[hand][1] += toBody[1] * k;
                s_vrThrowOrigin[hand][2] += toBody[2] * k;
            }
        }

        if (s_vrThrowPrevValid[hand]) {
            const float dt = (float)dtMs;
            const float delta[3] = {
                (p[0] - s_vrThrowPrevPos[hand][0]) * 1000.0f / dt,
                (p[1] - s_vrThrowPrevPos[hand][1]) * 1000.0f / dt,
                (p[2] - s_vrThrowPrevPos[hand][2]) * 1000.0f / dt};
            float worldVel[3];
            VrTrackingToWorld(delta, beta, worldVel);
            /* Time-based smoothing keeps a single noisy pose from deciding
               the throw while a real swing survives for ~80 ms, so pressing
               at the end of the gesture still uses the swing. */
            const float blend =
                1.0f - expf(-(float)dtMs / VR_THROW_VEL_SMOOTH_MS);
            for (S32 axis = 0; axis < 3; axis++) {
                s_vrThrowVelocity[hand][axis] =
                    s_vrThrowVelocity[hand][axis] * (1.0f - blend) +
                    worldVel[axis] * units * blend;
            }
        } else {
            s_vrThrowVelocity[hand][0] = 0.0f;
            s_vrThrowVelocity[hand][1] = 0.0f;
            s_vrThrowVelocity[hand][2] = 0.0f;
        }
        s_vrThrowPrevPos[hand][0] = p[0];
        s_vrThrowPrevPos[hand][1] = p[1];
        s_vrThrowPrevPos[hand][2] = p[2];
        s_vrThrowPrevValid[hand] = 1;

        const float forward[3] = {0.0f, 0.0f, -1.0f};
        float aim[3];
        float aimQ[4], aimP[3];
        if (OpenXR_GetHandPose(hand, 0 /* aim */, aimQ, aimP))
            VrQuatRotate(aimQ, forward, aim);
        else
            VrQuatRotate(q, forward, aim);
        VrTrackingToWorld(aim, beta, s_vrThrowAim[hand]);

        /* The palm is often closer than the near plane; push it forward
           until it projects.  Without a drawn hand, hold the ball a little
           in front of the controller instead.  The ball and the launch use
           the same point. */
        if (!s_vrPalmValid[hand]) {
            s_vrThrowOrigin[hand][0] +=
                s_vrThrowAim[hand][0] * VR_THROW_HAND_FORWARD;
            s_vrThrowOrigin[hand][1] +=
                s_vrThrowAim[hand][1] * VR_THROW_HAND_FORWARD;
            s_vrThrowOrigin[hand][2] +=
                s_vrThrowAim[hand][2] * VR_THROW_HAND_FORWARD;
        }
        for (S32 attempt = 0; attempt < 6; attempt++) {
            LongWorldRotatePoint((S32)s_vrThrowOrigin[hand][0],
                                 (S32)s_vrThrowOrigin[hand][1],
                                 (S32)s_vrThrowOrigin[hand][2]);
            if (CameraZr - Z0 >= NearClip + 80)
                break;
            s_vrThrowOrigin[hand][0] += s_vrThrowAim[hand][0] * 100.0f;
            s_vrThrowOrigin[hand][1] += s_vrThrowAim[hand][1] * 100.0f;
            s_vrThrowOrigin[hand][2] += s_vrThrowAim[hand][2] * 100.0f;
        }

        s_vrThrowHandValid[hand] = 1;
    }

    VrPlaceHeldBall();
    s_vrThrowPrevTime = now;
}

static S32 VrBody_GetThrow(S32 *x, S32 *y, S32 *z, S32 *alpha, S32 *beta) {
    if (x == NULL || y == NULL || z == NULL || alpha == NULL || beta == NULL)
        return 0;

    const S32 hand =
        s_vrThrowHandValid[1] ? 1 : (s_vrThrowHandValid[0] ? 0 : -1);
    if (hand < 0)
        return 0;

    const float vx = s_vrThrowVelocity[hand][0];
    const float vy = s_vrThrowVelocity[hand][1];
    const float vz = s_vrThrowVelocity[hand][2];
    const float speed = sqrtf(vx * vx + vy * vy + vz * vz);

    float dx, dy, dz;
    if (speed > VR_THROW_SWING_SPEED) {
        dx = vx / speed;
        dy = vy / speed;
        dz = vz / speed;
    } else {
        dx = s_vrThrowAim[hand][0];
        dy = s_vrThrowAim[hand][1];
        dz = s_vrThrowAim[hand][2];
    }

    const float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < 1e-4f)
        return 0;
    dx /= len;
    dy /= len;
    dz /= len;

    const float horiz = sqrtf(dx * dx + dz * dz);
    *beta = (S32)(atan2f(dx, dz) * (2048.0f / VR_PI)) & 4095;
    *alpha = (S32)(atan2f(dy, horiz) * (2048.0f / VR_PI)) & 4095;

    *x = (S32)s_vrThrowOrigin[hand][0];
    *y = (S32)s_vrThrowOrigin[hand][1];
    *z = (S32)s_vrThrowOrigin[hand][2];

    Log_Debug("VrBody: physical throw hand=%d dir=(%.2f,%.2f,%.2f)", (int)hand,
              (double)dx, (double)dy, (double)dz);
    return 1;
}

static void VrCreateHeldBall(void) {
    VrDropHeldBall();

    const S32 hand =
        s_vrThrowHandValid[1] ? 1 : (s_vrThrowHandValid[0] ? 0 : -1);
    if (hand < 0)
        return;

    const S32 index = ThrowExtra(
        255, (S32)s_vrThrowOrigin[hand][0], (S32)s_vrThrowOrigin[hand][1],
        (S32)s_vrThrowOrigin[hand][2], (S16)MagicBallSprite[MagicLevel], 0, 0,
        0, 0, 0);
    if (index < 0 || index >= MAX_EXTRAS)
        return;

    T_EXTRA *held = &ListExtra[index];
    held->Flags = EXTRA_HELD | EXTRA_NO_SHADOW;
    if (MagicLevel == 4)
        held->Flags |= EXTRA_TRANSPARENT;
    held->Body = -1;

    s_vrHeldBall = index;
    s_vrHeldHand = hand;
}

S32 VrBody_BeginThrow(void) {
    if (!s_vrThrowHandValid[1] && !s_vrThrowHandValid[0])
        return 0;
    if (!s_vrThrowArmed) {
        s_vrThrowArmed = 1;
        VrCreateHeldBall();
    }
    return 1;
}

S32 VrBody_EndThrow(S32 *x, S32 *y, S32 *z, S32 *alpha, S32 *beta) {
    if (!s_vrThrowArmed)
        return 0;
    s_vrThrowArmed = 0;
    const S32 ok = VrBody_GetThrow(x, y, z, alpha, beta);
    VrDropHeldBall();
    return ok;
}

static void VrMat3FromEngine(const TYPE_MAT *m, VR_MAT3 *out) {
    out->m[0][0] = m->F.M11;
    out->m[0][1] = m->F.M12;
    out->m[0][2] = m->F.M13;
    out->m[1][0] = m->F.M21;
    out->m[1][1] = m->F.M22;
    out->m[1][2] = m->F.M23;
    out->m[2][0] = m->F.M31;
    out->m[2][1] = m->F.M32;
    out->m[2][2] = m->F.M33;
}

/* Column-vector transform by the transpose (inverse of a rotation). */
static void VrVecByInverse(const VR_MAT3 *m, const float v[3], float out[3]) {
    out[0] = v[0] * m->m[0][0] + v[1] * m->m[1][0] +
             v[2] * m->m[2][0];
    out[1] = v[0] * m->m[0][1] + v[1] * m->m[1][1] +
             v[2] * m->m[2][1];
    out[2] = v[0] * m->m[0][2] + v[1] * m->m[1][2] +
             v[2] * m->m[2][2];
}

/* Standard matrix product, matching MulMatrixF's m1 * m2. */
static void VrMatMul(VR_MAT3 *out, const VR_MAT3 *a, const VR_MAT3 *b) {
    VR_MAT3 r;
    for (S32 i = 0; i < 3; i++) {
        for (S32 j = 0; j < 3; j++) {
            r.m[i][j] = a->m[i][0] * b->m[0][j] +
                        a->m[i][1] * b->m[1][j] +
                        a->m[i][2] * b->m[2][j];
        }
    }
    *out = r;
}

/* Two-bone solve in the parent's local frame.
 *
 * ObjectDisplay builds a group as Current = parent * R(angles) and
 * RotTransList applies it as a column transform (M v + T).  The child's
 * rotation therefore acts before its parent, so both rest vectors must be
 * expressed in the parent's frame: the upper arm maps its local rest vector
 * to the local elbow direction, and the forearm maps its local rest vector
 * to the local hand direction after the upper-arm rotation. */
static void VrSolveArm(const TYPE_MAT *parentMat, const float shoulder[3],
                       const float elbow[3], const float hand[3],
                       const float target[3], const float handUp[3],
                       S32 out[2][3], float *stretchOut, S32 canStretch,
                       float solvedElbow[3]) {
    float upperRest[3], foreRest[3];
    float d[3], u[3], pole[3], v[3];
    float l1, l2, dlen, a, hgt;
    float elbowOut[3], upper[3], fore[3];
    float rawUpper[3], tgtUpper[3], rawFore[3], tgtFore[3];
    VR_MAT3 parent, upperLocal, foreLocal, upperBody;

    VrMat3FromEngine(parentMat, &parent);
    *stretchOut = 1.0f;

    for (S32 i = 0; i < 3; i++) {
        upperRest[i] = elbow[i] - shoulder[i];
        foreRest[i] = hand[i] - elbow[i];
        d[i] = target[i] - shoulder[i];
    }
    l1 = sqrtf(upperRest[0] * upperRest[0] + upperRest[1] * upperRest[1] +
               upperRest[2] * upperRest[2]);
    l2 = sqrtf(foreRest[0] * foreRest[0] + foreRest[1] * foreRest[1] +
               foreRest[2] * foreRest[2]);
    const float trueDlen = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    dlen = trueDlen;
    if (dlen < 1e-3f)
        dlen = 1e-3f;
    if (canStretch && trueDlen > l1 + l2 - 1.0f) {
        *stretchOut = trueDlen / (l1 + l2);
        if (*stretchOut > VR_MAX_ARM_STRETCH)
            *stretchOut = VR_MAX_ARM_STRETCH;
        l1 *= *stretchOut;
        l2 *= *stretchOut;
    }
    if (dlen > l1 + l2 - 1.0f)
        dlen = l1 + l2 - 1.0f;
    if (dlen < fabsf(l1 - l2) + 1.0f)
        dlen = fabsf(l1 - l2) + 1.0f;
    /* Normalise against the real target, not the clamped distance: when the
       reach is capped the elbow must stay along the shoulder-to-hand line. */
    const float norm = (trueDlen > 1e-3f) ? trueDlen : 1.0f;
    for (S32 i = 0; i < 3; i++)
        u[i] = d[i] / norm;

    a = (l1 * l1 - l2 * l2 + dlen * dlen) / (2.0f * dlen);
    hgt = l1 * l1 - a * a;
    if (hgt < 0.0f)
        hgt = 0.0f;
    hgt = sqrtf(hgt);
    /* handUp is the swivel pole in the body frame, built from the wrist's
       knuckle plane.  Keeping the knob axis out of it leaves the elbow in
       that plane instead of flaring. */
    pole[0] = handUp[0];
    pole[1] = handUp[1];
    pole[2] = handUp[2];
    {
        float dot = pole[0] * u[0] + pole[1] * u[1] + pole[2] * u[2];
        float len2 = 0.0f;
        for (S32 i = 0; i < 3; i++) {
            v[i] = pole[i] - u[i] * dot;
            len2 += v[i] * v[i];
        }
        if (len2 < 1e-4f) {
            /* Pole parallel to the arm: pick a perpendicular from the world
               up (or forward when the arm is vertical) so the elbow stays on
               one side instead of flipping. */
            if (fabsf(u[1]) < 0.9f) {
                v[0] = -u[2];
                v[1] = 0.0f;
                v[2] = u[0];
            } else {
                v[0] = u[1];
                v[1] = -u[0];
                v[2] = 0.0f;
            }
        }
        VrNormalize(v);
    }
    for (S32 i = 0; i < 3; i++) {
        elbowOut[i] = shoulder[i] + u[i] * a + v[i] * hgt;
        upper[i] = elbowOut[i] - shoulder[i];
        fore[i] = target[i] - elbowOut[i];
        if (solvedElbow != NULL)
            solvedElbow[i] = elbowOut[i];
    }

    VrVecByInverse(&parent, upperRest, rawUpper);
    VrVecByInverse(&parent, upper, tgtUpper);
    VrMatFromTo(rawUpper, tgtUpper, &upperLocal);

    VrMatMul(&upperBody, &parent, &upperLocal);
    VrVecByInverse(&parent, foreRest, rawFore);
    VrVecByInverse(&upperBody, fore, tgtFore);
    VrMatFromTo(rawFore, tgtFore, &foreLocal);

    VrMatToEuler(&upperLocal, &out[0][0], &out[0][1], &out[0][2]);
    VrMatToEuler(&foreLocal, &out[1][0], &out[1][1], &out[1][2]);
}

/* Reproduce ObjectDisplay's group transform chain without drawing.  The
 * resulting origins are body-local, relative to the ghost root, with the
 * ghost's yaw applied. */
static S32 VrBuildRig(const T_OBJ_3D *obj, const T_BODY_HEADER *body,
                      VR_RIG *rig) {
    if (obj == NULL || body == NULL || body->NbGroupes <= 0 ||
        body->NbGroupes > VR_MAX_GROUPS || body->NbPoints > VR_MAX_POINTS) {
        return 0;
    }
    const VR_GROUP *groups =
        (const VR_GROUP *)((const U8 *)body + body->OffGroupes);
    const TYPE_VT16 *src =
        (const TYPE_VT16 *)((const U8 *)body + body->OffPoints);
    TYPE_MAT root;
    S32 g;
    U16 offset = 0;

    InitMatrixStd(&root, 0, obj->Beta, 0);
    CopyMatrix(&rig->mat[0], &root);
    RotTransList(&rig->mat[0], rig->rot, (TYPE_VT16 *)src, groups[0].NbPts);
    rig->slice[0] = 0;
    rig->origin[0].X = 0;
    rig->origin[0].Y = 0;
    rig->origin[0].Z = 0;
    offset = groups[0].NbPts;

    for (g = 1; g < body->NbGroupes; g++) {
        const VR_GROUP *grp = &groups[g];
        const TYPE_VT16 *parentOrigin = &rig->rot[grp->OrgPoint];
        TYPE_MAT *parent = &rig->mat[grp->OrgGroupe];
        const T_GROUP_INFO *anim = &obj->CurrentFrame[VR_ANIM(g)];

        rig->slice[g] = offset;
        rig->origin[g] = *parentOrigin;
        if (anim->Type & TYPE_TRANSLATE) {
            /* Translate groups store a translation in the angle fields.
               Rotate it by the parent matrix locally: RotatePoint writes the
               engine's X0/Y0/Z0 camera globals, and this runs before the
               eye/terrain pass, so using it here made the terrain project
               from the wrong origin and get culled. */
            const float a = (float)anim->Alpha;
            const float b = (float)anim->Beta;
            const float c = (float)anim->Gamma;
            const S32 tx = (S32)lrintf(parent->F.M11 * a +
                                       parent->F.M12 * b +
                                       parent->F.M13 * c);
            const S32 ty = (S32)lrintf(parent->F.M21 * a +
                                       parent->F.M22 * b +
                                       parent->F.M23 * c);
            const S32 tz = (S32)lrintf(parent->F.M31 * a +
                                       parent->F.M32 * b +
                                       parent->F.M33 * c);
            CopyMatrix(&rig->mat[g], parent);
            InitMatrixTrans(&rig->mat[g], tx + parentOrigin->X,
                            ty + parentOrigin->Y, tz + parentOrigin->Z);
        } else {
            RotateMatrix(&rig->mat[g], parent, anim->Alpha, anim->Beta,
                         anim->Gamma);
            InitMatrixTrans(&rig->mat[g], parentOrigin->X, parentOrigin->Y,
                            parentOrigin->Z);
        }
        RotTransList(&rig->mat[g], &rig->rot[offset],
                     (TYPE_VT16 *)&src[offset], grp->NbPts);
        offset = (U16)(offset + grp->NbPts);
    }
    return 1;
}

/* End offset of the last body section.  Used only to size the private copy;
   the format keeps all sections before the texture-handle table. */
static S32 VrBodyCopyEnd(const T_BODY_HEADER *body) {
    S32 end = body->SizeHeader;
    S32 e;

    e = body->OffGroupes + body->NbGroupes * (S32)sizeof(VR_GROUP);
    if (e > end)
        end = e;
    e = body->OffPoints + body->NbPoints * (S32)sizeof(TYPE_VT16);
    if (e > end)
        end = e;
    e = body->OffNormales + body->NbNormales * (S32)sizeof(TYPE_VT16);
    if (e > end)
        end = e;
    e = body->OffNormFaces + body->NbNormFaces * (S32)sizeof(TYPE_VT16);
    if (e > end)
        end = e;
    if (body->OffPolys > end)
        end = body->OffPolys;
    e = body->OffLines + body->NbLines * 8;
    if (e > end)
        end = e;
    e = body->OffSpheres + body->NbSpheres * 8;
    if (e > end)
        end = e;
    e = body->OffTextures + body->NbTextures * 4;
    if (e > end)
        end = e;
    if (end < (S32)sizeof(T_BODY_HEADER))
        end = (S32)sizeof(T_BODY_HEADER);
    return end;
}

static U8 *VrEnsureBodyCopy(const T_BODY_HEADER *body) {
    if (body == NULL)
        return NULL;
    const S32 end = VrBodyCopyEnd(body);
    const U32 span = (U32)end + VR_BODY_COPY_PAD;
    if (s_vrBodyCopy == NULL || s_vrBodyCopySize < span) {
        U8 *grown = (U8 *)realloc(s_vrBodyCopy, (size_t)span);
        if (grown == NULL)
            return NULL;
        s_vrBodyCopy = grown;
        s_vrBodyCopySize = span;
    }
    memcpy(s_vrBodyCopy, body, (size_t)end);
    return s_vrBodyCopy;
}

static S16 VrStretchCoord(float v) {
    if (v < -32768.0f)
        v = -32768.0f;
    else if (v > 32767.0f)
        v = 32767.0f;
    return (S16)lrintf(v);
}

/* Scale a group's points along one bone axis, leaving the thickness alone. */
static void VrStretchGroup(TYPE_VT16 *dst, const TYPE_VT16 *src, S32 slice,
                           S32 count, const TYPE_VT16 *from,
                           const TYPE_VT16 *to, float stretch) {
    if (stretch <= 1.0001f)
        return;
    float dir[3] = {(float)to->X - (float)from->X,
                    (float)to->Y - (float)from->Y,
                    (float)to->Z - (float)from->Z};
    const float len =
        sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (len < 1e-3f)
        return;
    dir[0] /= len;
    dir[1] /= len;
    dir[2] /= len;
    for (S32 i = 0; i < count; i++) {
        const TYPE_VT16 *p = &src[slice + i];
        TYPE_VT16 *q = &dst[slice + i];
        const float dx = (float)p->X - (float)from->X;
        const float dy = (float)p->Y - (float)from->Y;
        const float dz = (float)p->Z - (float)from->Z;
        const float along = dx * dir[0] + dy * dir[1] + dz * dir[2];
        const float k = along * (stretch - 1.0f);
        q->X = VrStretchCoord((float)p->X + dir[0] * k);
        q->Y = VrStretchCoord((float)p->Y + dir[1] * k);
        q->Z = VrStretchCoord((float)p->Z + dir[2] * k);
        q->Grp = p->Grp;
    }
}

/* Hand target in body-local space, relative to the solved head origin. */
/* twist: extra yaw the body carries beyond the head (the right-stick turn
 * plus any smoothing lag), in engine angle units.  The IK target lives in
 * the yawed body frame but its offset cancels the head, so without this the
 * hands stay world-facing when the body turns with the stick. */
static void VrHandTargetLocal(const float head[3], const float headPos[3],
                              const float handPos[3], S32 twist,
                              float out[3]) {
    const float units = VrUnitsPerMetre();
    const float dx = handPos[0] - headPos[0];
    const float dy = handPos[1] - headPos[1];
    const float dz = handPos[2] - headPos[2];
    /* Body-local +Z is front and +X is the model's left.  OpenXR is +X
       right, -Z front, so a tracking offset maps with both X and Z negated:
       the left controller (dx < 0) lands on +X and a hand moved forward
       (dz < 0) lands on +Z. */
    const float ox = -dx * units;
    const float oz = -dz * units;
    const float a = (float)twist * (VR_PI / 2048.0f);
    const float c = cosf(a);
    const float s = sinf(a);

    out[0] = head[0] + (c * ox + s * oz);
    out[1] = head[1] + dy * units;
    out[2] = head[2] + (-s * ox + c * oz);
}

static S32 VrGetHandTarget(S32 hand, const float head[3],
                           const float headPos[3], S32 twist, float out[3],
                           float handUp[3]) {
    float q[4], p[3];
    if (OpenXR_GetHandJointPose(hand, 1 /* wrist */, q, p) ||
        OpenXR_GetHandPose(hand, 1 /* grip fallback */, q, p)) {
        VrHandTargetLocal(head, headPos, p, twist, out);

        /* Elbow swivel from the hand frame: the bend plane contains the
           hand's back/forward axis (knuckle plane), with a downward bias.
           The local X axis is the knuckle line, so leaving it out of the
           pole keeps the elbow in that plane instead of flaring. */
        {
            const S32 side = (hand == 0) ? 1 : -1;
            const float upAxis[3] = {0.0f, 1.0f, 0.0f};
            const float backAxis[3] = {0.0f, 0.0f, 1.0f};
            const float sideIn[3] = {1.0f, 0.0f, 0.0f};
            float up[3], back[3], sideAxis[3];
            VrQuatRotate(q, upAxis, up);
            VrQuatRotate(q, backAxis, back);
            VrQuatRotate(q, sideIn, sideAxis);
            handUp[0] = -0.6f * up[0] + 0.8f * back[0] +
                        0.1f * (float)side * sideAxis[0];
            handUp[1] = -0.6f * up[1] + 0.8f * back[1] +
                        0.1f * (float)side * sideAxis[1];
            handUp[2] = -0.6f * up[2] + 0.8f * back[2] +
                        0.1f * (float)side * sideAxis[2];
        }
        {
            const float a = (float)twist * (VR_PI / 2048.0f);
            const float c = cosf(a);
            const float s = sinf(a);
            const float ux = handUp[0];
            const float uz = handUp[2];
            handUp[0] = c * ux + s * uz;
            handUp[2] = -s * ux + c * uz;
        }
        return 1;
    }
    return 0;
}

static void VrSetArmAngles(T_OBJ_3D *ghost, S32 upperGroup, S32 foreGroup,
                           const S32 angles[2][3]) {
    ghost->CurrentFrame[VR_ANIM(upperGroup)].Type = TYPE_ROTATE;
    ghost->CurrentFrame[VR_ANIM(upperGroup)].Alpha = (S16)angles[0][0];
    ghost->CurrentFrame[VR_ANIM(upperGroup)].Beta = (S16)angles[0][1];
    ghost->CurrentFrame[VR_ANIM(upperGroup)].Gamma = (S16)angles[0][2];
    ghost->CurrentFrame[VR_ANIM(foreGroup)].Type = TYPE_ROTATE;
    ghost->CurrentFrame[VR_ANIM(foreGroup)].Alpha = (S16)angles[1][0];
    ghost->CurrentFrame[VR_ANIM(foreGroup)].Beta = (S16)angles[1][1];
    ghost->CurrentFrame[VR_ANIM(foreGroup)].Gamma = (S16)angles[1][2];
}

void VrBody_Update(void) {
    VrUpdateThrowState();

    T_OBJET *hero = &ListObjet[NUM_PERSO];
    T_OBJ_3D ghost;
    const T_BODY_HEADER *body;
    VR_RIG rig;
    float headLocal[3];
    float headPos[3];
    float headWorld[3];
    float handTarget[2][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    float handPole[2][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    S32 handValid[2] = {0, 0};

    s_vrActive = 0;
    s_vrReady = 0;
    s_vrArmValid[0] = 0;
    s_vrArmValid[1] = 0;
    s_vrPalmValid[0] = 0;
    s_vrPalmValid[1] = 0;
    s_vrBodyCopyActive = 0;

    if (!VrFullBody || !CameraFirstPerson || CubeMode != CUBE_EXTERIEUR ||
        !DisplayStereo3D || !OpenXR_IsActive() || CinemaMode) {
        s_vrBodyBetaInit = 0;
        s_smoothInit[0] = 0;
        s_smoothInit[1] = 0;
        s_poleInit[0] = 0;
        s_poleInit[1] = 0;
        return;
    }
    s_vrHero = hero;
    s_vrBodyHqr = hero->Obj.Body.Num;
    body = (const T_BODY_HEADER *)(TransFctBody != NULL
                                       ? TransFctBody(hero->Obj.Body.Num)
                                       : hero->Obj.Body.Ptr);
    if (body == NULL)
        return;

    ghost = hero->Obj;
    ghost.Alpha = 0;
    ghost.Gamma = 0;
    /* Body-local +Z is the face/chest (the two single-vertex groups at -Z
       are the ponytail), so the object angle that makes it look along the
       camera view is 2048 - BetaCam, including the right-stick yaw.  Ease
       toward it so a stick turn reads as the body turning, not as a body
       nailed to the screen. */
    {
        const S32 target = (2048 - BetaCam) & 4095;
        if (!s_vrBodyBetaInit) {
            s_vrBodyBeta = target;
            s_vrBodyBetaInit = 1;
        }
        S32 diff = target - s_vrBodyBeta;
        if (diff > 2048)
            diff -= 4096;
        else if (diff < -2048)
            diff += 4096;
        /* A dead-on 180 degree difference is left signed (not folded to
           zero) so the body can never settle facing backwards.  Clamp the
           lag so a fast turn never drags the hands across the body. */
        if (diff > 192)
            diff = 192;
        else if (diff < -192)
            diff = -192;
        s_vrBodyBeta = (s_vrBodyBeta + diff / 3) & 4095;
        ghost.Beta = s_vrBodyBeta;
    }
    ghost.X = 0;
    ghost.Y = 0;
    ghost.Z = 0;

    /* The ghost is pinned to the tracked head, so the spine and hidden head
       stay at rest: an animated head displacement would drag the whole
       visible body with it.  The legs keep the game clip so the walk cycle
       (and the feet) read correctly; the arms are held at rest only while
       the rig is measured, because the IK needs the rest bone vectors. */
    for (S32 g = 1; g <= 3; g++) {
        ghost.CurrentFrame[VR_ANIM(g)].Type = 0;
        ghost.CurrentFrame[VR_ANIM(g)].Alpha = 0;
        ghost.CurrentFrame[VR_ANIM(g)].Beta = 0;
        ghost.CurrentFrame[VR_ANIM(g)].Gamma = 0;
    }
    for (S32 g = 4; g <= 7; g++) {
        ghost.CurrentFrame[VR_ANIM(g)].Type = 0;
        ghost.CurrentFrame[VR_ANIM(g)].Alpha = 0;
        ghost.CurrentFrame[VR_ANIM(g)].Beta = 0;
        ghost.CurrentFrame[VR_ANIM(g)].Gamma = 0;
    }
    for (S32 g = 14; g <= 16; g++) {
        ghost.CurrentFrame[VR_ANIM(g)].Type = 0;
        ghost.CurrentFrame[VR_ANIM(g)].Alpha = 0;
        ghost.CurrentFrame[VR_ANIM(g)].Beta = 0;
        ghost.CurrentFrame[VR_ANIM(g)].Gamma = 0;
    }

    if (!VrBuildRig(&ghost, body, &rig))
        return;

    U8 *bodyCopy = VrEnsureBodyCopy(body);

    /* Head-group centroid: eyes sit near the middle of the head mesh at
       about y=1150.  Groups 15/16 are the ponytail at the back, so they are
       not an eye reference. */
    {
        const VR_GROUP *groups =
            (const VR_GROUP *)((const U8 *)body + body->OffGroupes);
        const VR_GROUP *hg = &groups[VR_GROUP_HEAD];
        float sum[3] = {0.0f, 0.0f, 0.0f};
        S32 i;
        for (i = 0; i < hg->NbPts; i++) {
            const TYPE_VT16 *p = &rig.rot[rig.slice[VR_GROUP_HEAD] + i];
            sum[0] += (float)p->X;
            sum[1] += (float)p->Y;
            sum[2] += (float)p->Z;
        }
        if (hg->NbPts > 0) {
            headLocal[0] = sum[0] / (float)hg->NbPts;
            headLocal[1] = sum[1] / (float)hg->NbPts;
            headLocal[2] = sum[2] / (float)hg->NbPts;
        } else {
            headLocal[0] = (float)rig.origin[VR_GROUP_HEAD].X;
            headLocal[1] = (float)rig.origin[VR_GROUP_HEAD].Y;
            headLocal[2] = (float)rig.origin[VR_GROUP_HEAD].Z;
        }
    }

    /* The rendered camera is VueOffset plus the tracked lean that the stereo
       path applies in AffGrilleExt.  Use the previous frame's lean here; the
       body is one frame behind the camera, never detached from it. */
    headWorld[0] = (float)(VueOffsetX + StereoCamOffsetX);
    headWorld[1] = (float)(VueOffsetY + StereoCamOffsetY);
    headWorld[2] = (float)(VueOffsetZ + StereoCamOffsetZ);

    ghost.X = (S32)(headWorld[0] - headLocal[0]);
    /* Feet, not the eye height, carry the vertical placement: the model
       origin is the sole of the feet, so this keeps the body planted and
       stable even if the camera height formula drifts a few units. */
    ghost.Y = hero->Obj.Y;
    ghost.Z = (S32)(headWorld[2] - headLocal[2]);

    /* The body carries the right-stick turn in addition to the head; the
       hand offset is rotated by that difference so the hands swing with the
       torso instead of staying world-facing. */
    S32 bodyTwist = 0;
#ifdef LBA2_OPENXR
    {
        const S32 headBeta =
            ((2048 - BetaCam) - FirstPersonStickYaw()) & 4095;
        bodyTwist = (ghost.Beta - headBeta) & 4095;
    }
#endif

    for (S32 hand = 0; hand < 2; hand++) {
        float q[4], p[3];
        if (!OpenXR_GetHandJointPose(hand, 1, q, p) &&
            !OpenXR_GetHandPose(hand, 1, q, p)) {
            s_smoothInit[hand] = 0;
            continue;
        }
        /* head position in tracking space: mid of the eye views; the hand is
           head-relative so the body root offset cancels. */
        float eye0Q[4], eye0P[3], eye1Q[4], eye1P[3];
        if (!OpenXR_GetEyeView(0, eye0Q, eye0P) ||
            !OpenXR_GetEyeView(1, eye1Q, eye1P)) {
            s_smoothInit[hand] = 0;
            continue;
        }
        float trackingHead[3];
        trackingHead[0] = 0.5f * (eye0P[0] + eye1P[0]);
        trackingHead[1] = 0.5f * (eye0P[1] + eye1P[1]);
        trackingHead[2] = 0.5f * (eye0P[2] + eye1P[2]);
        if (!VrGetHandTarget(hand, headLocal, trackingHead, bodyTwist,
                             handTarget[hand], handPole[hand])) {
            s_smoothInit[hand] = 0;
            continue;
        }

        /* Smooth the target in body-local space, then bring it back into the
           yawed rig frame the IK solves in. */
        {
            const float ox = handTarget[hand][0] - headLocal[0];
            const float oy = handTarget[hand][1] - headLocal[1];
            const float oz = handTarget[hand][2] - headLocal[2];
            const float a = (float)(-ghost.Beta) * (VR_PI / 2048.0f);
            const float c = cosf(a);
            const float s = sinf(a);
            float bx = c * ox + s * oz;
            float bz = -s * ox + c * oz;
            float by = oy;
            if (s_smoothInit[hand]) {
                bx = s_smoothOff[hand][0] * 0.5f + bx * 0.5f;
                by = s_smoothOff[hand][1] * 0.5f + by * 0.5f;
                bz = s_smoothOff[hand][2] * 0.5f + bz * 0.5f;
            }
            s_smoothOff[hand][0] = bx;
            s_smoothOff[hand][1] = by;
            s_smoothOff[hand][2] = bz;
            s_smoothInit[hand] = 1;

            const float a2 = (float)ghost.Beta * (VR_PI / 2048.0f);
            const float c2 = cosf(a2);
            const float s2 = sinf(a2);
            handTarget[hand][0] = headLocal[0] + (c2 * bx + s2 * bz);
            handTarget[hand][1] = headLocal[1] + by;
            handTarget[hand][2] = headLocal[2] + (-s2 * bx + c2 * bz);
        }

        /* Smooth the swivel pole in the torso frame as well. */
        {
            const float a = (float)(-ghost.Beta) * (VR_PI / 2048.0f);
            const float c = cosf(a);
            const float s = sinf(a);
            float px = c * handPole[hand][0] + s * handPole[hand][2];
            float pz = -s * handPole[hand][0] + c * handPole[hand][2];
            float py = handPole[hand][1];
            if (s_poleInit[hand]) {
                px = s_poleSmooth[hand][0] * 0.6f + px * 0.4f;
                py = s_poleSmooth[hand][1] * 0.6f + py * 0.4f;
                pz = s_poleSmooth[hand][2] * 0.6f + pz * 0.4f;
            }
            s_poleSmooth[hand][0] = px;
            s_poleSmooth[hand][1] = py;
            s_poleSmooth[hand][2] = pz;
            s_poleInit[hand] = 1;

            const float a2 = (float)ghost.Beta * (VR_PI / 2048.0f);
            const float c2 = cosf(a2);
            const float s2 = sinf(a2);
            handPole[hand][0] = c2 * px + s2 * pz;
            handPole[hand][1] = py;
            handPole[hand][2] = -s2 * px + c2 * pz;
        }
        handValid[hand] = 1;
    }

    /* Solve before moving the ghost: rig origins are in ghost-local space. */
    for (S32 hand = 0; hand < 2; hand++) {
        if (!handValid[hand])
            continue;
        const S32 upper = (hand == 0) ? VR_GROUP_ARM_L_UPPER : VR_GROUP_ARM_R_UPPER;
        const S32 fore = (hand == 0) ? VR_GROUP_ARM_L_FORE : VR_GROUP_ARM_R_FORE;
        const VR_GROUP *groups =
            (const VR_GROUP *)((const U8 *)body + body->OffGroupes);
        const VR_GROUP *fg = groups + fore;
        float shoulder[3], elbow[3], handRest[3];
        float best = -1.0f;
        S32 bestIdx = -1;

        shoulder[0] = (float)rig.origin[upper].X;
        shoulder[1] = (float)rig.origin[upper].Y;
        shoulder[2] = (float)rig.origin[upper].Z;
        elbow[0] = (float)rig.origin[fore].X;
        elbow[1] = (float)rig.origin[fore].Y;
        elbow[2] = (float)rig.origin[fore].Z;
        handRest[0] = elbow[0];
        handRest[1] = elbow[1];
        handRest[2] = elbow[2];
        for (S32 i = 0; i < fg->NbPts; i++) {
            const TYPE_VT16 *p = &rig.rot[rig.slice[fore] + i];
            float dx = (float)p->X - elbow[0];
            float dy = (float)p->Y - elbow[1];
            float dz = (float)p->Z - elbow[2];
            float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 > best) {
                best = d2;
                bestIdx = i;
            }
        }
        if (bestIdx >= 0) {
            const TYPE_VT16 *p = &rig.rot[rig.slice[fore] + bestIdx];
            handRest[0] = (float)p->X;
            handRest[1] = (float)p->Y;
            handRest[2] = (float)p->Z;
        }
        float stretch = 1.0f;
        float solvedElbow[3];
        VrSolveArm(&rig.mat[groups[upper].OrgGroupe], shoulder, elbow,
                   handRest, handTarget[hand], handPole[hand],
                   s_vrArmAngles[hand], &stretch, bodyCopy != NULL,
                   solvedElbow);
        s_vrArmValid[hand] = 1;

        if (stretch > 1.0001f && bodyCopy != NULL && bestIdx >= 0) {
            const TYPE_VT16 *src = (const TYPE_VT16 *)((const U8 *)body +
                                                       body->OffPoints);
            TYPE_VT16 *dst =
                (TYPE_VT16 *)(bodyCopy + body->OffPoints);
            const VR_GROUP *ug = groups + upper;
            const TYPE_VT16 *shoulderRaw = &src[ug->OrgPoint];
            const TYPE_VT16 *elbowRaw = &src[fg->OrgPoint];
            const TYPE_VT16 *handRaw = &src[rig.slice[fore] + bestIdx];
            VrStretchGroup(dst, src, rig.slice[upper], ug->NbPts,
                           shoulderRaw, elbowRaw, stretch);
            VrStretchGroup(dst, src, rig.slice[fore], fg->NbPts, elbowRaw,
                           handRaw, stretch);
            s_vrBodyCopyActive = 1;
        }

        /* The IK target is the fingertip; move a quarter of the way back to
           the elbow so the held ball sits in the palm, not past the hand. */
        {
            const float *palmBase = (stretch > 1.0f) ? solvedElbow : elbow;
            float dx = handTarget[hand][0] - palmBase[0];
            float dy = handTarget[hand][1] - palmBase[1];
            float dz = handTarget[hand][2] - palmBase[2];
            const float len = sqrtf(dx * dx + dy * dy + dz * dz);
            float palm[3];
            if (len > 1e-3f) {
                const float back = len * 0.25f;
                palm[0] = handTarget[hand][0] - dx / len * back;
                palm[1] = handTarget[hand][1] - dy / len * back;
                palm[2] = handTarget[hand][2] - dz / len * back;
            } else {
                palm[0] = handTarget[hand][0];
                palm[1] = handTarget[hand][1];
                palm[2] = handTarget[hand][2];
            }
            /* Rig points already carry the body yaw (VrBuildRig bakes it into
               the root matrix), so the palm only needs the body translation.
               Rotating it again by Beta would swing the ball around the
               player whenever the body turns with the head or the stick. */
            s_vrPalmWorld[hand][0] = ghost.X + palm[0];
            s_vrPalmWorld[hand][1] = ghost.Y + palm[1];
            s_vrPalmWorld[hand][2] = ghost.Z + palm[2];
            s_vrPalmValid[hand] = 1;
        }
    }

    /* Apply the solved arm angles to the ghost copy.  An untracked hand
       keeps the game animation instead of freezing in the rest pose. */
    if (s_vrArmValid[0]) {
        VrSetArmAngles(&ghost, VR_GROUP_ARM_L_UPPER, VR_GROUP_ARM_L_FORE,
                       s_vrArmAngles[0]);
    } else {
        ghost.CurrentFrame[VR_ANIM(VR_GROUP_ARM_L_UPPER)] =
            hero->Obj.CurrentFrame[VR_ANIM(VR_GROUP_ARM_L_UPPER)];
        ghost.CurrentFrame[VR_ANIM(VR_GROUP_ARM_L_FORE)] =
            hero->Obj.CurrentFrame[VR_ANIM(VR_GROUP_ARM_L_FORE)];
    }
    if (s_vrArmValid[1]) {
        VrSetArmAngles(&ghost, VR_GROUP_ARM_R_UPPER, VR_GROUP_ARM_R_FORE,
                       s_vrArmAngles[1]);
    } else {
        ghost.CurrentFrame[VR_ANIM(VR_GROUP_ARM_R_UPPER)] =
            hero->Obj.CurrentFrame[VR_ANIM(VR_GROUP_ARM_R_UPPER)];
        ghost.CurrentFrame[VR_ANIM(VR_GROUP_ARM_R_FORE)] =
            hero->Obj.CurrentFrame[VR_ANIM(VR_GROUP_ARM_R_FORE)];
    }

    /* Rebuild the rig only for the ghost root, then store it for drawing.
       Arm angles are already applied; the draw path uses ObjectDisplay's own
       transform chain. */
    s_vrGhost = ghost;
    s_vrActive = 1;
    s_vrReady = 1;

    {
        static U32 s_log = 0;
        if ((s_log++ % 300) == 0) {
            Log_Debug("VrBody: body=%d head=(%d,%d,%d)/(%d,%d,%d) hands=%d/%d "
                      "scale=%d",
                      (int)s_vrBodyHqr, (int)ghost.X, (int)ghost.Y,
                      (int)ghost.Z, (int)headLocal[0], (int)headLocal[1],
                      (int)headLocal[2], (int)s_vrArmValid[0],
                      (int)s_vrArmValid[1], (int)VrWorldScale);
        }
    }
}

void VrBody_DrawFirstPerson(void *ptrobj) {
    (void)ptrobj;
    if (!s_vrActive || !s_vrReady)
        return;

    T_OBJ_3D ghost = s_vrGhost;
    const S32 oldNear = NearClip;
    const U32 oldMask = ObjHiddenGroupMask;

    /* Group 14 is the whole face/hair mesh and 15/16 are its eye details;
       the camera sits inside it, so they would fill the view.  Shoulders,
       torso and arms stay visible. */
    if (VrBodyHideGroups >= 0) {
        ObjHiddenGroupMask = (U32)VrBodyHideGroups;
    } else {
        ObjHiddenGroupMask = (1u << 14) | (1u << 15) | (1u << 16);
    }
    /* AffOneObject leaves the screen clip at the last sorted object's
       bounds.  ObjectDisplay culls and fills against that clip, so drawing
       the ghost without clearing it makes the body pop in and out as the
       last object changes. */
    UnsetClip();
    NearClip = 1;

    /* A stretched arm is drawn from the private body copy, which has the
       scaled point tables.  ObjectDisplay only takes the copy pointer when
       TransFctBody is null. */
    Func_TransNumPtr *oldTrans = TransFctBody;
    if (s_vrBodyCopyActive && s_vrBodyCopy != NULL) {
        ghost.Body.Ptr = s_vrBodyCopy;
        TransFctBody = NULL;
    }
    ObjectDisplay(&ghost);
    TransFctBody = oldTrans;

    ObjHiddenGroupMask = oldMask;
    NearClip = oldNear;
}
