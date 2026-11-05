#include "C_EXTERN.H"
#include "GAMEMENU_MENUS.H"
#include <math.h> /* cosf/sinf - rolled stereo eye offset */
#include "IMAGE_LOAD.H"

/* Temporary perf instrumentation from RENDER_GL.CPP */
extern "C" S32 GL_BatchFlushCount;

#include "BUILD_INFO.h" /* LBA2_VERSION_STRING (for --version flag) */
#include "INITADEL.H"
#include "DIRECTORIES.H"
#include "RES_DISCOVERY.H"
#include "RES_PICKER.H"
#include "INPUT.H"
#include "CLI_ARGS.H"
#include "CONTROL.H"
#include "RES_SWITCH.H"
#include "CONSOLE/CONSOLE.H"
#include "EXIT_SCREEN.H"
#include "PERFTRACE.H"
#include "TOUCH_INPUT.H"
#include "VRBODY.H"
#include <SYSTEM/OPENXR.H>
#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/LOGPRINT.H> /* CreateLog (boot log brought up in main) */
#include "ASSET_PREFLIGHT.H"
#ifdef USE_GPURENDERER
#include <GL/RENDER_GL.H>
#endif
#include <SYSTEM/EVENTS.H>
#include <SVGA/VIDEO.H>
#include <FILEIO/SAVEPNG.H>

#include <SVGA/SCREEN.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include "FOLLOWCAM_CFG.H"
#include <SYSTEM/STRING.H>
#include <SYSTEM/WINDOW.H>

#ifdef LBA2_OPENXR
#include <stdio.h>
#include <stdlib.h>

/* Interior is an isometric projection centred on the framebuffer, so a
 * camera offset is a centre offset; it moves the room under a fixed camera
 * rather than moving a perspective eye.  The left stick keeps its ordinary
 * locomotion bindings so walking and steering are unchanged. */
static S32 s_InteriorCamPanX = 0;
static S32 s_InteriorCamPanY = 0;
static S32 s_InteriorCamAppliedPanY = 0;
static S32 s_InteriorCamLastCube = -1;
static U32 s_InteriorCamLastZoom = 0;
/* The thumbrest zoom is a view adjustment for the room, so the display
   setting it borrowed is restored when the room is left.  Without this a
   zoom taken in a house follows the player outside, where VrFovScale is a
   different thing (the eye's field of view). */
static S32 s_InteriorZoomActive = 0;
static S32 s_InteriorZoomSaved = 100;

/* Head-tracked hover rig.  The anchor is the pose captured when the rig
 * engaged, so entering the mode never jumps the view; the pan is the view
 * offset the current pose asks for. */
static S32 s_HoverAnchored = 0;
static float s_HoverAnchorYaw = 0.0f;
static float s_HoverAnchorPitch = 0.0f;
static float s_HoverAnchorX = 0.0f;
static float s_HoverAnchorY = 0.0f;
static float s_HoverAnchorZ = 0.0f;
/* Last applied parallax delta; a change needs a full redraw, not an
   incremental dirty-box patch. */
static S32 s_HoverBaseX = 0;
static S32 s_HoverBaseY = 0;
static S32 s_HoverMoved = 0;

static S32 InteriorHoverFakeActive(void) {
    static S32 s_fake = -1;
    if (s_fake < 0) {
        const char *env = getenv("LBA2_FAKE_HOVER");
        s_fake = (env != NULL && *env != '\0') ? 1 : 0;
    }
    return s_fake;
}

/* The desktop stereo harness renders the pair without a session, so the
   hover rig's stereo gate has to accept it too. */
static S32 InteriorHoverFakeStereo(void) {
    static S32 s_fake = -1;
    if (s_fake < 0) {
        const char *env = getenv("LBA2_FAKE_STEREO");
        s_fake = (env != NULL && *env != '\0') ? 1 : 0;
    }
    return s_fake;
}

/* Latest tracked head pose: yaw/pitch in radians, position in metres on the
   tracking axes.  LBA2_FAKE_HOVER ("yawDeg,pitchDeg[,x,y,z]") injects one on
   a desktop build so the stereo pair can be reviewed without a headset.
   Returns 0 until the runtime has located a view, so the rig never captures
   a neutral anchor from an all-zero placeholder pose. */
static S32 InteriorHoverHeadPose(float *yaw, float *pitch, float *hx,
                                 float *hy, float *hz) {
    const char *fake = getenv("LBA2_FAKE_HOVER");
    *yaw = 0.0f;
    *pitch = 0.0f;
    *hx = 0.0f;
    *hy = 0.0f;
    *hz = 0.0f;
    if (fake != NULL && *fake != '\0') {
        float yawDeg = 0.0f, pitchDeg = 0.0f;
        sscanf(fake, "%f,%f,%f,%f,%f", &yawDeg, &pitchDeg, hx, hy, hz);
        *yaw = yawDeg * (3.14159265358979f / 180.0f);
        *pitch = pitchDeg * (3.14159265358979f / 180.0f);
        return 1;
    }
    {
        float q[4];
        float p[3];
        if (!OpenXR_GetEyeView(0, q, p))
            return 0;
        *hx = p[0];
        *hy = p[1];
        *hz = p[2];
    }
    *yaw = OpenXR_HeadYawDeg() * (3.14159265358979f / 180.0f);
    *pitch = OpenXR_HeadPitchDeg() * (3.14159265358979f / 180.0f);
    return 1;
}

static float InteriorHoverWrapPi(float a) {
    while (a > 3.14159265f)
        a -= 2.0f * 3.14159265f;
    while (a < -3.14159265f)
        a += 2.0f * 3.14159265f;
    return a;
}

/* Aim the isometric view from the tracked head pose relative to the anchor.
   A turn moves the room by exactly the angle turned in the eye's projection;
   a lean becomes the scene's own eye delta so it lands as depth (near planes
   move further than far ones) instead of sliding the whole diorama. */
static void UpdateInteriorHover(S32 *outPanX, S32 *outPanY) {
    *outPanX = 0;
    *outPanY = 0;
    s_HoverMoved = 0;
#ifdef USE_GPURENDERER
    GL_SetSinglePassBaseDelta(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    GL_SetSubPixelOffset(0.0f, 0.0f);
#endif
    if (!VrInteriorHover || (!DisplayStereo3D && !InteriorHoverFakeStereo()) ||
        CinemaMode) {
        s_HoverBaseX = 0;
        s_HoverBaseY = 0;
        return;
    }
    if (!OpenXR_IsActive() && !InteriorHoverFakeActive()) {
        s_HoverBaseX = 0;
        s_HoverBaseY = 0;
        return;
    }

    float yaw, pitch, hx, hy, hz;
    if (!InteriorHoverHeadPose(&yaw, &pitch, &hx, &hy, &hz)) {
        s_HoverBaseX = 0;
        s_HoverBaseY = 0;
        return;
    }
    if (!s_HoverAnchored) {
        /* The injected desktop pose is already an offset from neutral; the
           device pose is absolute, so it becomes the neutral anchor. */
        if (InteriorHoverFakeActive()) {
            s_HoverAnchorYaw = 0.0f;
            s_HoverAnchorPitch = 0.0f;
            s_HoverAnchorX = 0.0f;
            s_HoverAnchorY = 0.0f;
            s_HoverAnchorZ = 0.0f;
        } else {
            s_HoverAnchorYaw = yaw;
            s_HoverAnchorPitch = pitch;
            s_HoverAnchorX = hx;
            s_HoverAnchorY = hy;
            s_HoverAnchorZ = hz;
        }
        s_HoverAnchored = 1;
    }

    const float yawOff = InteriorHoverWrapPi(yaw - s_HoverAnchorYaw);
    const float pitchOff = pitch - s_HoverAnchorPitch;
    const float dx = hx - s_HoverAnchorX;
    const float dy = hy - s_HoverAnchorY;

    /* The eye passes override the focal from the headset after this update,
       so convert with what they will use.  The eye's vertical focal is the
       horizontal one times the projection's |y ratio|. */
    float focalX = FRatioX;
    float focalYRatio = FRatioY;
    if (StereoFocalX > 0 && StereoFocalY > 0) {
        focalX = (float)StereoFocalX;
        focalYRatio = -(float)StereoFocalY / focalX;
    }
    const float focalY =
        focalX * (focalYRatio < 0.0f ? -focalYRatio : focalYRatio);

    StereoHoverPoseToPan(yawOff, pitchOff, 0.0f, 0.0f, focalX, focalY,
                         (S32)ModeDesiredX, (S32)ModeDesiredY, outPanX,
                         outPanY);

#ifdef USE_GPURENDERER
    /* The projection centre is an integer, so the pan's fractional part is
       lost and the room steps by a pixel at a time.  Carry that remainder
       into the present offset: the pan's sign moves the image opposite to
       the centre, and a UV offset moves it opposite to the sample. */
    if (DisplaySubPixel) {
        float panFX = 0.0f;
        float panFY = 0.0f;
        StereoHoverPoseToPanF(yawOff, pitchOff, 0.0f, 0.0f, focalX, focalY,
                              (S32)ModeDesiredX, (S32)ModeDesiredY, &panFX,
                              &panFY);
        GL_SetSubPixelOffset(
            -(panFX - (float)*outPanX) / (float)ModeDesiredX,
            -(panFY - (float)*outPanY) / (float)ModeDesiredY);
    }
#endif

#ifdef USE_GPURENDERER
    {
        S32 leanX = 0, leanY = 0, wx = 0, wy = 0;
        const float zScale = (float)((double)Fill_ZBuffer_Factor / 65536.0);
        StereoHoverPoseToPan(0.0f, 0.0f, dx, dy, 0.0f, 0.0f,
                             (S32)ModeDesiredX, (S32)ModeDesiredY, &leanX,
                             &leanY);
        StereoHoverPanToWorld(leanX, leanY, focalX, focalYRatio, zScale,
                              STEREO_HOVER_REF_AZ, &wx, &wy);
        GL_SetSinglePassBaseDelta((float)wx, (float)wy, 0.0f, 0.0f, 0.0f);
        if (wx != s_HoverBaseX || wy != s_HoverBaseY) {
            s_HoverBaseX = wx;
            s_HoverBaseY = wy;
            s_HoverMoved = 1;
        }
    }
#endif
}

static S32 InteriorCamStickStep(S16 axis) {
    S32 mag = (axis < 0) ? -(S32)axis : (S32)axis;
    if (mag <= JoystickDeadzone)
        return 0;
    S32 den = 32767 - JoystickDeadzone;
    if (den < 1)
        den = 1;
    S32 step = ((mag - JoystickDeadzone) * 6) / den;
    if (step < 1)
        step = 1;
    return (axis < 0) ? -step : step;
}

/* Quest interior camera: the right stick moves the isometric view in the
 * frame plane, X across and Y up/down.  Resting the left thumb on its thumb
 * rest (or on the stick without moving it) turns the right stick's vertical
 * axis into VR world zoom instead.  The head-tracked hover rig is a separate,
 * opt-in layer on top of the same projection centre. */
static void UpdateInteriorCameraControl(void) {
    if (CubeMode != CUBE_INTERIEUR) {
        /* Leaving the room ends its zoom: give the display setting back the
           value it had before the room was entered, so a zoom taken indoors
           does not follow the player outside. */
        if (s_InteriorZoomActive) {
            s_InteriorZoomActive = 0;
            if (VrFovScale != s_InteriorZoomSaved) {
                VrFovScale = s_InteriorZoomSaved;
                FirstTime = AFF_ALL_FLIP;
            }
        }
        s_HoverAnchored = 0;
        s_HoverBaseX = 0;
        s_HoverBaseY = 0;
#ifdef USE_GPURENDERER
        GL_SetSinglePassBaseDelta(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        GL_SetSubPixelOffset(0.0f, 0.0f);
#endif
        return;
    }
    if ((!OpenXR_IsActive() && !InteriorHoverFakeActive()) ||
        (!DisplayStereo3D && !InteriorHoverFakeStereo()) || CinemaMode) {
        s_HoverAnchored = 0;
        s_HoverBaseX = 0;
        s_HoverBaseY = 0;
#ifdef USE_GPURENDERER
        GL_SetSinglePassBaseDelta(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        GL_SetSubPixelOffset(0.0f, 0.0f);
#endif
        return;
    }

    if (s_InteriorCamLastCube != (S32)NumCube) {
        s_InteriorCamLastCube = (S32)NumCube;
        s_InteriorCamPanX = 0;
        s_InteriorCamPanY = 0;
        s_HoverAnchored = 0;
    }
    if (!s_InteriorZoomActive) {
        /* First room of this run indoors: remember the display zoom so the
           exit can restore it. */
        s_InteriorZoomActive = 1;
        s_InteriorZoomSaved = VrFovScale;
    }
    if (Input & I_RETURN) {
        s_InteriorCamPanX = 0;
        s_InteriorCamPanY = 0;
        s_HoverAnchored = 0;
        /* Recentering returns the view to its natural scale as well, so the
         * key undoes the stick zoom as well as the pan. */
        if (VrFovScale != 100) {
            VrFovScale = 100;
            FirstTime = AFF_ALL_FLIP;
        }
    }

    const S32 baseX = (S32)ModeDesiredX / 2 - 9;
    const S32 baseY = (S32)ModeDesiredY / 2;
    const S32 oldPanX = s_InteriorCamPanX;
    const S32 oldPanY = s_InteriorCamPanY;
    S32 hoverPanX = 0, hoverPanY = 0;
    UpdateInteriorHover(&hoverPanX, &hoverPanY);

    /* The right stick moves the isometric view; the Y axis has a second job
     * only while the left thumb rests on the capacitive area by the buttons. */
    {
        const S32 zoomMode = OpenXR_LeftThumbrestTouched;
        const S32 stepX = InteriorCamStickStep(OpenXR_RightStickX);
        const S32 stepY = InteriorCamStickStep(OpenXR_RightStickY);

        s_InteriorCamPanX -= stepX;

        if (zoomMode) {
            if (stepY != 0 && PERSO_StereoRenderActive() &&
                (U32)(TimerRefHR - s_InteriorCamLastZoom) >= 40) {
                /* Stick up zooms in: shrinking the FOV percentage widens the
                 * rendered world into the same panel. */
                if (stepY > 0)
                    VrFovScale++;
                else
                    VrFovScale--;
                if (VrFovScale < 50)
                    VrFovScale = 50;
                else if (VrFovScale > 120)
                    VrFovScale = 120;
                s_InteriorCamLastZoom = TimerRefHR;
                FirstTime = AFF_ALL_FLIP;
            }
        } else {
            /* Stick Y is SDL-style (positive down): pushing up moves the
               view up, which moves the room down the screen. */
            s_InteriorCamPanY -= stepY;
        }
    }

    const S32 limitX = (S32)ModeDesiredX / 4;
    const S32 limitY = (S32)ModeDesiredY / 4;
    if (s_InteriorCamPanX < -limitX)
        s_InteriorCamPanX = -limitX;
    else if (s_InteriorCamPanX > limitX)
        s_InteriorCamPanX = limitX;
    if (s_InteriorCamPanY < -limitY)
        s_InteriorCamPanY = -limitY;
    else if (s_InteriorCamPanY > limitY)
        s_InteriorCamPanY = limitY;

    /* The head aim is not clamped to the stick's quarter frame: the room is
     * a fixed object, so a large enough turn must be able to carry it out of
     * view.  StereoHoverPoseToPan bounds it at four frames. */
    const S32 panX = s_InteriorCamPanX + hoverPanX;
    const S32 panY = s_InteriorCamPanY + hoverPanY;

    if (panX != oldPanX || panY != oldPanY ||
        hoverPanY != s_InteriorCamAppliedPanY || s_HoverMoved)
        FirstTime = AFF_ALL_FLIP;
    s_InteriorCamAppliedPanY = hoverPanY;

    /* Re-apply every frame: Init3DView and authored camera changes reset the
     * projection centre, and the pan must survive those without being baked
     * into the scene's own camera value. */
    SetIsoProjection(baseX + panX, baseY + panY);
}
#endif

#ifdef DEBUG_TOOLS
static inline U32 DosAvailableMem(void) { return 0; }
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SYSTEM/ANDROID.H>
#include <ctype.h>
#include <stdarg.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*#ifndef DEMO
#define THOMAS  1
#endif
*/

#ifdef DEMO
extern char *Version;
#else
extern const char *Version;
#endif

extern T_OBJ_3D InvObjTwinsen[];

extern S32 PersoFlashTimer; // pour quand le perso eh ben y meurt !!!

S32 EscTimer;

/* FollowCamera refreshes terrain with AFF_ALL_FLIP every movement frame.
 * AFF_ALL_FLIP runs the full AffScene preamble (MemoClipWindow / UnsetClipWindow
 * / ClearImpactRain) before the render, preventing black edges from active clip
 * regions. Wall-clock time during the render+vsync wait is correctly credited
 * to TimerRefHR by ManageTime() after the LIB386/SYSTEM/TIMER.CPP generalised
 * fix; see docs/TIMING.md. Vsync naturally caps terrain refreshes at the
 * display rate (~60 Hz). When the hero is idle, all work is skipped entirely. */
/* Default resting distance for the spring arm: the closer authored preset. */
#define FOLLOW_CAM_INITIAL_DIST FOLLOW_CAM_DEFAULT_DIST
static S32 FollowCamPrevHeroX = 0;
static S32 FollowCamPrevHeroY = 0;
static S32 FollowCamPrevHeroZ = 0;
static S32 FollowCamPrevHeroBeta = 0;
static S32 FollowCamPrevAddBeta = 0;    /* dirty check: last seen AddBetaCam */
static S32 FollowCamEffectiveDist = 0;  /* spring arm: smoothed distance (0 = uninitialised) */
static S32 FollowCamTargetBeta = 0;     /* rotation lag: desired BetaCam */
S32 FollowCamReengageDelay = 0;         /* frames before auto-center resumes after manual pan */
S32 FollowCamManualHold = 0;            /* frames the pitch/distance auto-assist yields after a manual nudge */
static S32 FollowCamPrevBaseDist = -1;  /* dirty: zoom from GereExtKeys (numpad * /) */
static S32 FollowCamPrevAlphaCam = -1;  /* dirty: numpad +/- elevation while follow */
static S32 FollowCamEyeLift = 0;        /* ground clearance: eased eye lift above terrain */
static S32 FollowCamGroundSettling = 0; /* ground clearance: lift still easing (keeps the dirty check live while the hero stands still) */

/* HD vertical-FOV recompose strength for the Auto camera (community).  Returns
 * kExcess = ModeDesiredY*100/480 - 100: the hundredths by which the render
 * height exceeds the original 480 (0 at 480, 125 at 1080).  The follow-cam apply
 * path scales its boom pull-in, pitch steepen, and forward-lean cut by this, so
 * every recompose term is an exact no-op at 480.  Returns 0 when the recompose
 * is disabled (cam_hd 0); callers also pass 0 inside camera zones so authored /
 * scripted shots are never recomposed.  Tuning in FOLLOWCAM_CFG.H. */
static S32 FollowCamHDExcess(void) {
    if (!FollowCamHDRecompose)
        return 0;
    /* With the exterior focal floored (Init3DExtView), every render height
     * above 720 projects exactly the 720p view, so the recompose strength
     * freezes at the 720p excess instead of growing with the height. */
    S32 k100 = GpuFocalCapped ? 150 : (S32)ModeDesiredY * 100 / 480;
    S32 e = (k100 > 100) ? (k100 - 100) : 0;
    /* The correction grows linearly with render height, which over-steepens at
     * 1080p (it reads as stronger than 720p). Cap the effective excess so tall
     * resolutions converge to a fixed correction rather than growing without
     * bound. cam_hd_cap 0 disables the cap (pure linear). */
    if (FollowCamHDExcessCap > 0 AND e > FollowCamHDExcessCap)
        e = FollowCamHDExcessCap;
    return e;
}

/* The follow camera re-renders when the hero moved or turned, the player panned,
 * BetaCam still has catch-up pending, the zoom/elevation changed, or the
 * ground-lift / manual-hold are still settling (the last two keep this true so
 * they finish easing while the hero stands still). */
static S32 FollowCamNeedsUpdate(T_OBJ_3D *ptr3d, S32 catchupDiff) {
    return ptr3d->X != FollowCamPrevHeroX OR ptr3d->Y != FollowCamPrevHeroY OR ptr3d->Z != FollowCamPrevHeroZ OR ptr3d->Beta != FollowCamPrevHeroBeta OR AddBetaCam != FollowCamPrevAddBeta OR(catchupDiff > FOLLOW_CAM_ROT_THRESHOLD OR catchupDiff < -FOLLOW_CAM_ROT_THRESHOLD) OR FollowCamBaseDist != FollowCamPrevBaseDist OR AlphaCam != FollowCamPrevAlphaCam OR FollowCamGroundSettling OR FollowCamManualHold;
}

/* Per-frame Auto-camera update (exterior). Extracted from the main loop:
 * spring-arm zoom, lazy orbit + pan drift, HD recompose, terrain occlusion
 * clearance, and the manual-override fade. Reads/writes the FollowCam* globals
 * and ListObjet[NumObjFollow]. */
static void UpdateFollowCameraExt(void) {
    T_OBJ_3D *ptr3d = &ListObjet[NumObjFollow].Obj;
    S32 manualPan = CheckKey(K_LBRACKET) OR CheckKey(K_RBRACKET);
    S32 heroMoving = (ptr3d->X != FollowCamPrevHeroX OR ptr3d->Z != FollowCamPrevHeroZ);

    /* First-frame init: sync to current camera state.
                     * FollowCamTargetBeta defaults to 0 but BetaCam starts ~2048
                     * after CameraCenter(1), causing a bogus initial spin. */
    if (FollowCamBaseDist == 0) {
        FollowCamBaseDist = FOLLOW_CAM_INITIAL_DIST;
        AlphaCam = FOLLOW_CAM_DEFAULT_ALPHA;
    }
    if (FollowCamEffectiveDist == 0) {
        FollowCamEffectiveDist = FollowCamBaseDist;
        FollowCamTargetBeta = BetaCam;
        FollowCamPrevBaseDist = FollowCamBaseDist;
        FollowCamPrevAlphaCam = AlphaCam;
    }

    /* --- AddBetaCam drift ---
                     * Auto-recenter the manual pan back to behind the hero while
                     * walking. Disabled when FollowCamHoldAngle (cam_hold_angle,
                     * default on): the camera then holds whatever angle the player
                     * set until the scene changes or they hit Center camera
                     * (both go through CameraCenter(1), which zeroes AddBetaCam),
                     * matching a free third-person camera. */
    if (manualPan) {
        FollowCamReengageDelay = FollowCamReengageFrames;
    } else if (heroMoving) {
        if (FollowCamReengageDelay > 0) {
            FollowCamReengageDelay--;
        } else if (!FollowCamHoldAngle AND AddBetaCam != 0) {
            S32 drift = AddBetaCam;
            if (drift > 2048)
                drift -= 4096;
            S32 driftDiv = FollowCamDriftDivisor;
            if (driftDiv < 1)
                driftDiv = 1; /* console can set 0; never divide by zero */
            S32 step = drift / driftDiv;
            if (step > 0 AND step < FOLLOW_CAM_DRIFT_MIN_STEP)
                step = FOLLOW_CAM_DRIFT_MIN_STEP;
            if (step<0 AND step> - FOLLOW_CAM_DRIFT_MIN_STEP)
                step = -FOLLOW_CAM_DRIFT_MIN_STEP;
            if (drift > FOLLOW_CAM_DRIFT_THRESHOLD OR drift < -FOLLOW_CAM_DRIFT_THRESHOLD)
                AddBetaCam = (AddBetaCam - step) & 4095;
            else
                AddBetaCam = 0;
        }
    }

    /* --- Dirty check ---
                     * Triggers on position, facing, pan, or pending BetaCam
                     * catchup.  Hero Beta IS included: the camera always lazily
                     * orbits behind the hero whether running, turning in place,
                     * or standing.  Smoothness comes from the lerp speed, not
                     * from skipping updates. */
    S32 catchupDiff = FollowCamTargetBeta - BetaCam;
    if (catchupDiff > 2048)
        catchupDiff -= 4096;
    if (catchupDiff < -2048)
        catchupDiff += 4096;

    if (FollowCamNeedsUpdate(ptr3d, catchupDiff)) {
        S32 newbeta = AddBetaCam + ptr3d->Beta;
        newbeta &= 4095;

        /* HD recompose strength (kExcess): 0 in camera zones, at
                         * 480, or when disabled. Shared by the forward-lean cut
                         * here and the boom pull-in + pitch steepen at the apply
                         * below, so the whole recompose is a no-op at 480. */
        S32 hdExcess = CameraZone ? 0 : FollowCamHDExcess();

        /* Manual-override fade: a manual nudge set FollowCamManualHold
                         * (EXTFUNC), driving the auto-assist strength to 0 so the
                         * player's tilt/orbit wins; it eases back to full as the hold
                         * counts down. Scales the recompose pitch steepen and the
                         * ground-lift re-aim below (the two that fight a manual tilt);
                         * 100 (full assist) when no hold. */
        if (FollowCamManualHold > 0)
            FollowCamManualHold--;
        S32 autoFactor = 100;
        if (FollowCamManualHoldFrames > 0)
            autoFactor = 100 - FollowCamManualHold * 100 / FollowCamManualHoldFrames;
        if (autoFactor < 0)
            autoFactor = 0;
        if (autoFactor > 100)
            autoFactor = 100;

        /* Use the camera's current angle for the forward lean,
                         * not the hero's facing.  This keeps the orbit center
                         * stable when the hero turns in place; the lean point
                         * drifts smoothly as BetaCam lerps toward the target,
                         * instead of snapping with the hero's Beta. */
        S32 leanBeta = (2048 - BetaCam) & 4095;
        S32 leanDist = DISTANCE_VISEE;
        if (hdExcess) {
            /* Shorten the forward lean so the hero is not pushed
                             * toward the bottom edge of the taller frame. */
            S32 cut = hdExcess * FollowCamHDLeanGain / 100;
            if (cut > FOLLOW_CAM_HD_LEAN_MAX_CUT)
                cut = FOLLOW_CAM_HD_LEAN_MAX_CUT;
            leanDist = DISTANCE_VISEE * (100 - cut) / 100;
        }
        Rotate(0, leanDist, leanBeta);

        VueOffsetX = ptr3d->X + X0;
        VueOffsetY = ptr3d->Y;
        VueOffsetZ = ptr3d->Z + Z0;

        StartXCube = VueOffsetX / SIZE_BRICK_XZ;
        StartYCube = VueOffsetY / SIZE_BRICK_Y;
        StartZCube = VueOffsetZ / SIZE_BRICK_XZ;

        /* Rotation lag with distance-based inertia.
                         * The camera always tracks "behind the hero".  The lerp
                         * divisor scales with the spring arm length: closer camera
                         * (arm shortened near terrain) → tighter follow; full
                         * distance → lazy, cinematic drift.  Floor at divisor/3
                         * to avoid jitter when very close. */
        FollowCamTargetBeta = (2048 - newbeta) & 4095;

        if (manualPan) {
            BetaCam = FollowCamTargetBeta;
        } else if (ManualCameraOrbitActive OR FollowCamReengageDelay == 0) {
            S32 diff = FollowCamTargetBeta - BetaCam;
            if (diff > 2048)
                diff -= 4096;
            if (diff < -2048)
                diff += 4096;
            if (diff > FOLLOW_CAM_ROT_THRESHOLD OR diff < -FOLLOW_CAM_ROT_THRESHOLD) {
                /* Manual orbit (mouse/stick): tight lerp for responsive feel.
                                 * Auto-follow: distance-scaled inertia for cinematic drift. */
                S32 dynamicDiv;
                if (ManualCameraOrbitActive) {
                    dynamicDiv = ManualCameraSmoothing;
                } else {
                    dynamicDiv = FOLLOW_CAM_ROT_DIVISOR * FollowCamEffectiveDist / FollowCamBaseDist;
                    if (dynamicDiv < FOLLOW_CAM_ROT_DIVISOR / 3)
                        dynamicDiv = FOLLOW_CAM_ROT_DIVISOR / 3;
                }
                S32 step = diff / dynamicDiv;
                // Clamp to a minimum magnitude toward the target. Integer
                // division truncates to 0 whenever |diff| < dynamicDiv (e.g.
                // diff -13 / div 36 = 0), and diff is still past the snap
                // threshold here, so without this the camera neither steps
                // nor snaps: BetaCam sticks ~threshold..dynamicDiv units off
                // target forever, the dirty check keeps firing, and the
                // exterior re-renders the full terrain (AFF_ALL_FLIP) every
                // idle frame. Handle step == 0 too, not just the < MIN_STEP
                // cases, so the camera always closes the gap and settles.
                if (step >= 0 AND step < FOLLOW_CAM_ROT_MIN_STEP)
                    step = (diff > 0) ? FOLLOW_CAM_ROT_MIN_STEP : -FOLLOW_CAM_ROT_MIN_STEP;
                else if (step<0 AND step> - FOLLOW_CAM_ROT_MIN_STEP)
                    step = -FOLLOW_CAM_ROT_MIN_STEP;
                BetaCam = (BetaCam + step) & 4095;
            } else {
                BetaCam = FollowCamTargetBeta;
            }
        }

        /* --- Zoom distance: converge FollowCamEffectiveDist toward
                         * FollowCamBaseDist (numpad * /).  VueDistance must match
                         * FollowCamEffectiveDist before CameraCenter(3) so terrain
                         * render (AffGrilleExt) stays consistent. */
        if (!CameraZone) {
            if (FollowCamEffectiveDist < FollowCamBaseDist) {
                FollowCamEffectiveDist += FOLLOW_CAM_SPRING_RECOVER;
                if (FollowCamEffectiveDist > FollowCamBaseDist)
                    FollowCamEffectiveDist = FollowCamBaseDist;
            } else if (FollowCamEffectiveDist > FollowCamBaseDist) {
                FollowCamEffectiveDist -= FOLLOW_CAM_SPRING_RECOVER;
                if (FollowCamEffectiveDist < FollowCamBaseDist)
                    FollowCamEffectiveDist = FollowCamBaseDist;
            }

            /* HD recompose (render-time only; logical AlphaCam and
                             * FollowCamBaseDist are left untouched and restored
                             * below). Pull the boom in so the subject fills the
                             * taller frame instead of dead sky, and steepen the
                             * pitch to trade sky for ground. No-op when hdExcess
                             * is 0 (480 / disabled / camera zone). */
            S32 renderDist = FollowCamEffectiveDist;
            S32 saveAlphaCam = AlphaCam;
            if (hdExcess) {
                S32 pull = hdExcess * FollowCamHDDistGain / 100;
                if (pull > FOLLOW_CAM_HD_DIST_MAX_PULL)
                    pull = FOLLOW_CAM_HD_DIST_MAX_PULL;
                renderDist = FollowCamEffectiveDist * (100 - pull) / 100;

                /* Only the pitch steepen fades under manual control
                                 * (autoFactor): it is what fights the player's tilt.
                                 * The boom pull-in and lean do not fight tilt/orbit,
                                 * so they stay put and the camera does not pop out
                                 * when the player grabs the stick. */
                AlphaCam += hdExcess * FollowCamHDPitchGain / 100 * autoFactor / 100;
                if (AlphaCam > FOLLOW_CAM_ALPHA_MAX)
                    AlphaCam = FOLLOW_CAM_ALPHA_MAX;
            }
            VueDistance = renderDist;
            CameraCenter(3);
            AlphaCam = saveAlphaCam;

            /* Terrain occlusion clearance: a smooth port of the
                             * classic SearchCameraPos awareness. CameraCenter(3)
                             * left the eye at CameraX/Y/Z; march a few points
                             * along the eye->hero line and, where terrain would
                             * rise above the line of sight (a hill between camera
                             * and hero, or the eye sinking into rising ground),
                             * raise the eye just enough to clear the worst
                             * occluder, then re-aim at the hero with the same lib
                             * calls the classic recenter uses. The lift eases
                             * every frame instead of snapping, so it never fights
                             * the orbit (which is what sank the earlier always-on
                             * attempt). Skipped out-of-cube, where
                             * CalculAltitudeObjet is invalid. */
            if (FollowCamGround
                    AND CameraX >= 0 AND CameraX <= 32768 AND CameraZ >= 0 AND CameraZ <= 32768) {
                S32 hx = ptr3d->X, hy = ptr3d->Y, hz = ptr3d->Z;
                S32 ex = CameraX, ey = CameraY, ez = CameraZ;
                S32 targetEyeY = ey; /* never push the eye lower */

                /* Sample fractions along hero(0)->eye(100). Start
                                 * past the hero so the ground it stands on is not
                                 * read as an occluder; 100 is the under-eye case. */
                static const S32 marchFrac[] = {40, 55, 70, 85, 100};
                for (S32 mi = 0; mi < 5; mi++) {
                    S32 f = marchFrac[mi];
                    S32 sx = hx + (ex - hx) * f / 100;
                    S32 sz = hz + (ez - hz) * f / 100;
                    if (sx < 0 OR sx > 32768 OR sz < 0 OR sz > 32768)
                        continue;
                    S32 g = PtrCalculAltitudeY(sx, sz);
                    S32 sightY = hy + (ey - hy) * f / 100;
                    if (g + FollowCamGroundClearance > sightY) {
                        /* eye height that drops this occluder just
                                         * under the eye->hero line (pivot at hero) */
                        S32 reqEyeY = hy + (g + FollowCamGroundClearance - hy) * 100 / f;
                        if (reqEyeY > targetEyeY)
                            targetEyeY = reqEyeY;
                    }
                }

                S32 targetLift = targetEyeY - ey;
                if (targetLift < 0)
                    targetLift = 0;
                if (targetLift > FOLLOW_CAM_GROUND_MAX_LIFT)
                    targetLift = FOLLOW_CAM_GROUND_MAX_LIFT;

                S32 d = targetLift - FollowCamEyeLift;
                if (d > FOLLOW_CAM_GROUND_SNAP OR d < -FOLLOW_CAM_GROUND_SNAP) {
                    S32 step = d / FOLLOW_CAM_GROUND_EASE;
                    if (step >= 0 AND step < FOLLOW_CAM_GROUND_MIN_STEP)
                        step = FOLLOW_CAM_GROUND_MIN_STEP;
                    else if (step<0 AND step> - FOLLOW_CAM_GROUND_MIN_STEP)
                        step = -FOLLOW_CAM_GROUND_MIN_STEP;
                    FollowCamEyeLift += step;
                } else {
                    FollowCamEyeLift = targetLift;
                }

                /* Keep the dirty check live until the lift
                                 * settles, so it converges even while the hero
                                 * stands still (e.g. at the foot of a hill). */
                FollowCamGroundSettling = (FollowCamEyeLift != targetLift) ? 1 : 0;

                /* Fade the lift + re-aim out under manual control
                                 * (autoFactor 0): the re-aim would otherwise pin
                                 * the pitch to the geometry and override the
                                 * player's tilt. At autoFactor 0 effLift is 0, so
                                 * the player's own angle from CameraCenter stands. */
                S32 effLift = FollowCamEyeLift * autoFactor / 100;
                if (effLift > 0) {
                    SetPosCamera(ex, ey + effLift, ez);
                    SetTargetCamera(VueOffsetX, VueOffsetY, VueOffsetZ, GammaCam);
                }
            } else {
                FollowCamGroundSettling = 0;
            }
        } else {
            VueDistance = FollowCamEffectiveDist;
            CameraCenter(3);
            /* Clear the ground-lift keep-alive too: otherwise a lift left
             * mid-ease when the hero enters a camera zone keeps the dirty
             * check true forever, re-rendering the terrain every idle frame. */
            FollowCamGroundSettling = 0;
        }

        FirstTime = AFF_ALL_FLIP;

        FollowCamPrevHeroX = ptr3d->X;
        FollowCamPrevHeroY = ptr3d->Y;
        FollowCamPrevHeroZ = ptr3d->Z;
        FollowCamPrevHeroBeta = ptr3d->Beta;
        FollowCamPrevAddBeta = AddBetaCam;
        FollowCamPrevBaseDist = FollowCamBaseDist;
        FollowCamPrevAlphaCam = AlphaCam;
    }
}

/*══════════════════════════════════════════════════════════════════════════*/
char NamePcxSave[ADELINE_MAX_PATH];
S32 NumPcxSave = 0;

#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
/*══════════════════════════════════════════════════════════════════════════*/

U32 MemoMemory;
U32 MemEnd;
U32 MemoDosMemory;
U32 MemoMinDosMemory;

S32 MinNbf = 1000;
S32 MaxNbf = 0;

S32 NbNbf = 0;
S32 TotalNbf = 0;

/*══════════════════════════════════════════════════════════════════════════*/

void AffDebugMenu() {
    S32 lig = 0;

    CoulText(LBAWHITE, 0);

    GraphPrintf(FALSE, 0, lig += 9, "Island: %d", Island);
    GraphPrintf(FALSE, 0, lig += 9, "Cube: %d", NumCube);
    GraphPrintf(FALSE, 0, lig += 9, "Chapter: %d", ListVarGame[FLAG_CHAPTER]);

    GraphPrintf(FALSE, 0, lig += 9, "CmptMemoTimer: %d", CmptMemoTimerRef);
    GraphPrintf(FALSE, 0, lig += 9, "Nb Objs: %d", NbObjets);
    GraphPrintf(FALSE, 0, lig += 9, "Nb Zones: %d", NbZones);
    GraphPrintf(FALSE, 0, lig += 9, "Nb Tracks: %d", NbBrickTrack);

    GraphPrintf(FALSE, 0, lig += 10, "NbFPS: %d", NbFramePerSecond);

    GraphPrintf(FALSE, 0, lig += 9, "Free(K): %d", (AvailableMem()) / 1024);
    GraphPrintf(FALSE, 0, lig += 10, "Memory at start: %d Ko", MemoMemory / 1024);

    GraphPrintf(FALSE, 0, lig += 9, "Total Extend Memory used: %d Ko", (MemoMemory - (AvailableMem())) / 1024);

    lig += 9;

    GraphPrintf(FALSE, 0, lig += 9, "CameraY     : %d", CameraY);

    lig += 9;

    GraphPrintf(FALSE, 0, lig += 9, "AlphaCam     : %d", AlphaCam);
    GraphPrintf(FALSE, 0, lig += 9, "VueDistance  : %d", VueDistance);
}
#endif

/*══════════════════════════════════════════════════════════════════════════*/

// Pour eviter d'ecraser les anciennes copies d'ecran !!!!
void GetNumPcxSave() {
    struct dirent *file;
    DIR *dir;
    S32 num;
    char shootDirPath[ADELINE_MAX_PATH];
    char pathname[ADELINE_MAX_PATH];

    GetShootPath(shootDirPath, ADELINE_MAX_PATH, NULL);
    dir = opendir(shootDirPath);
    if (dir != NULL) {
        while ((file = readdir(dir)) != NULL) {
            bool startsLBA = (ade_strcasestr(file->d_name, "LBA") == file->d_name);
            bool hasPCX = (ade_strcasestr(file->d_name, ".PCX") != NULL);
            if (startsLBA && hasPCX) {
                GetShootPath(pathname, ADELINE_MAX_PATH, file->d_name);

                struct stat st = {};
                stat(pathname, &st);
                if (S_ISREG(st.st_mode)) {
                    num = atoi(&file->d_name[3]);
                    if (num >= NumPcxSave) {
                        NumPcxSave = num + 1;
                    }
                }
            }
        }
        closedir(dir);
    }
}

void CheckSavePcx() {
    if (Key == K_F9) {
        S32 saveflagfade;

        SaveTimer();
        MemoClipWindow();
        UnsetClipWindow();

        BackupScreen(FALSE);
        CopyScreen(Log, Screen);

        GetNumPcxSave();

        char PCXFilename[ADELINE_MAX_PATH];
        snprintf(PCXFilename, sizeof(PCXFilename), "LBA%05d.PCX", NumPcxSave);
        GetShootPath(NamePcxSave, ADELINE_MAX_PATH, PCXFilename);

        CoulText(LBAWHITE, 0);
        GraphPrintf(TRUE, 50, 0, "Saving Screen into %s", NamePcxSave);

        SavePCX(NamePcxSave, Screen, ModeDesiredX, ModeDesiredY, PtrPal);

        WaitNoInput();

        // RestoreScreen without BoxUpdate()
        CopyScreen(ScreenAux, Screen);
        CopyScreen(ScreenAux, Log);

        RestoreTimer();
        SaveTimer();

        saveflagfade = FlagFade;
        FlagFade = FALSE;

        RestoreClipWindow();

        BoxReset();
        BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

        AffScene(AFF_OBJETS_NO_FLIP);
        FixeCinemaMode(FALSE);

        FlagFade = saveflagfade;
        GraphPrintf(FALSE, 50, 0, "Saving Screen into %s", NamePcxSave);
        BoxBlit();
        RestoreTimer();
    }
}

void CheckSaveLogPcx(U8 *pal) {
    if (Key == K_F9) {
        SaveTimer();

        GetNumPcxSave();

        char PCXFilename[ADELINE_MAX_PATH];
        snprintf(PCXFilename, sizeof(PCXFilename), "LBA%05d.PCX", NumPcxSave);
        GetShootPath(NamePcxSave, ADELINE_MAX_PATH, PCXFilename);

        SavePCX(NamePcxSave, Log, ModeDesiredX, ModeDesiredY, pal);

        CoulText(LBAWHITE, 0);
        GraphPrintf(TRUE, 50, 0, "Saving Screen into %s", NamePcxSave);

        WaitNoInput();

        RestoreTimer();
    }
}

//****************************************************************************
/*══════════════════════════════════════════════════════════════════════════*
                          █    ██▄ █  █    ▀▀█▀▀ ██▀▀▀
                          ██   ██▀██  ██     ██  ▀▀▀▀█
                          ▀▀   ▀▀  ▀  ▀▀     ▀▀  ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

void LoadListAnim3DS(void) {
    U32 handle, buffer;
    S32 n;
    char tmpFilePath[ADELINE_MAX_PATH];

    // recupere nombre d'anim dans la banque
    handle = OpenRead(HQRPtrAnim3DS->Name);
    if (handle) {
        Read(handle, &buffer, 4);
        Close(handle);
        n = (buffer / 4) - 2;
    }

    // Charge table de description des anims
    GetResPath(tmpFilePath, ADELINE_MAX_PATH, ANIM3DS_HQR_NAME);
    ListAnim3DS = (T_ANIM_3DS *)LoadMalloc_HQR(tmpFilePath, n);
    if (!ListAnim3DS)
        TheEndCheckFile(tmpFilePath);
}

/*──────────────────────────────────────────────────────────────────────────*/
void InitGameLists() {
    T_INCRUST_DISP *ptrdisp = ListIncrustDisp;
    S32 i;

    // Peut-etre inutile car fait ailleurs (a verifier ?)
    ClearExtra();

    for (i = 0; i < MAX_INCRUST_DISP; i++, ptrdisp++) {
        ptrdisp->Num = -1;
    }

    memset(ListVarCube, 0, MAX_VARS_CUBE * sizeof(ListVarCube[0]));
    memset(ListVarGame, 0, MAX_VARS_GAME * sizeof(ListVarGame[0]));

    ResetBuggy();
    ResetArdoise();

    // Init VarGames
    InitInventory();

    for (i = 0; i < 4; i++) {
        SampleAmbiance[i] = -1;
        SampleRepeat[i] = 1;
        SampleRnd[i] = 1;
    }

    NbObjets = 0;
    NbZones = 0;
    NbBrickTrack = 0;
}

/*══════════════════════════════════════════════════════════════════════════*/

void InitGame(int argc) {
    /*-------------------------------------------------------------------------*/
    UnsetClip();

    AlphaLight = 896 * MUL_ANGLE;
    BetaLight = 950 * MUL_ANGLE;

    PtrInit3DGame();

    InitGameLists();

    InitPerso();
    InitHoloMap();

    SceneStartX = 16 * SIZE_BRICK_XZ;
    SceneStartY = 24 * SIZE_BRICK_Y;
    SceneStartZ = 16 * SIZE_BRICK_XZ;

    /*-------------------------------------------------------------------------*/
    /* init FIRST scene files */

    NewCube = 0;
    NumCube = -1;

    FlagTheEnd = -1;
    MagicLevel = 0;
    MagicPoint = 0;
    NbGoldPieces = 0;
    NbZlitosPieces = 0;
    NbLittleKeys = 0;
    ListVarGame[FLAG_CHAPTER] = 0;
    Weapon = FLAG_BALLE_MAGIQUE;
    Island = 0;
    ListVarGame[FLAG_CLOVER] = NB_CLOVER_DEFAULT;
    NbCloverBox = NB_CLOVER_BOX_DEFAULT;
    FlagWater = FALSE;
    NumObjFollow = NUM_PERSO;
    SaveComportement = Comportement = C_NORMAL;

    // Mettre a jour ici pour commencer le jeu directement en mode cinema
    CinemaMode = FALSE;

    if (argc == -1) {
        FlagLoadGame = TRUE;
        LoadGameNumCube();
    } else {
        FadeToBlack(PtrPal);
        Cls();
        BoxUpdate();
    }
}

/*──────────────────────────────────────────────────────────────────────────*/
void GamePaused(U32 flags) {
    T_INCRUST_DISP *ptrdisp = NULL;
    S32 n;
    S32 endtimer;
    S32 fin = FALSE;

    SaveTimer();

    if (flags & PAUSE_CLOVER) {
        n = InitIncrustDisp(INCRUST_OBJ | INCRUST_YCLIP,
                            BODY_3D_CLOVER,
                            0, 0,
                            0, 0, 3);

        if (n != -1) {
            ptrdisp = &ListIncrustDisp[n];
        }

        PersoFlashTimer = TimerRefHR;
    }

    if (flags & PAUSE_MESSAGE) {
        ColorFont(LBAWHITE);
        Font(5, ClipWindowYMax - 33, "Pause");

        BoxMovingAdd(5, ClipWindowYMax - 33, 100, ClipWindowYMax);
        BoxBlit();
    }

    InitWaitNoInput(ALL_INPUTS);
    InitWaitNoKey();

    MyGetInput();

    endtimer = TimerRefHR + 1000 * 5; // Wait 5 sec

    // Magouille pour ne pas avoir la gestion des extras, pof ...
    FlagAnimWhoSpeak = TRUE;

    while (!fin) {
        ManageTime();

        if (ptrdisp) {
            ptrdisp->TimerEnd = TimerRefHR + 3 * 1000; // 3 s
            AffScene(AFF_OBJETS_NO_FLIP);
            BoxUpdate();
        }

        MyGetInput();

        if (MyKey OR Input)
            fin = TRUE;

        if (flags & PAUSE_CLOVER AND TimerRefHR > endtimer)
            fin = TRUE;
    }

    FlagAnimWhoSpeak = FALSE;

    RestoreTimer();

    if (ptrdisp) {
        ptrdisp->TimerEnd = TimerRefHR + 1000;
    }

    PersoFlashTimer = 0;

    ClearWaitNoInput();
}

/*══════════════════════════════════════════════════════════════════════════*
             █▄ ▄█ █▀▀▀█  █    ██▄ █       █     █▀▀▀█ █▀▀▀█ █▀▀▀█
             ██▀ █ ██▀▀█  ██   ██▀██       ██    ██  █ ██  █ ██▀▀▀
             ▀▀  ▀ ▀▀  ▀  ▀▀   ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

// en attendant les Input 64 bits (Watcom 11 ?)
U8 WaitNoKey32 = FALSE;
U8 WaitNoKey33 = FALSE;
U8 WaitNoKey34 = FALSE;
U8 WaitNoKey35 = FALSE;

/*──────────────────────────────────────────────────────────────────────────*/

// True if the keyboard (DefKeys) OR gamepad (GamepadKeys) binding for input
// slot n is held. The four spell/gadget slots (32-35) sit outside the Input
// bitfield (INPUT_TABLE_SLOTS == 32, and a 1<<i mask can't reach them), so both
// sources are polled directly here. Reading GamepadKeys too is what lets the
// pad's D-pad bindings actually cast, now that the D-pad is its own input
// source rather than an alias of the left stick (issue #360).
static S32 SpellKeyDown(S32 n) {
    return CheckKey(DefKeys[n].Key1) OR CheckKey(DefKeys[n].Key2)
        OR CheckKey(GamepadKeys[n].Key1) OR CheckKey(GamepadKeys[n].Key2);
}

static S32 CameraSwitchKeyDown(void) {
    return CheckKey(DefKeys[36].Key1) OR CheckKey(DefKeys[36].Key2)
        OR CheckKey(GamepadKeys[36].Key1) OR CheckKey(GamepadKeys[36].Key2);
}

static U8 WaitNoKey36 = FALSE;

/*──────────────────────────────────────────────────────────────────────────*/

#ifdef LBA2_OPENXR
/* Right-stick steering for first-person stereo.  The raw axis is integrated
 * here rather than through the digital I_CAMERA keys: those recenter the
 * stereo look instead of rotating it, and the head-tracked camera overrides
 * the hero's own turn.  Integrating on wall time keeps the turn rate stable
 * across fixed-sim skip frames. */
static S32 s_FirstPersonStickYaw = 0;
static U32 s_FirstPersonStickTick = 0;

S32 FirstPersonStickYaw(void) {
    return s_FirstPersonStickYaw;
}

static void UpdateFirstPersonStickYaw(void) {
    const U32 now = (U32)SDL_GetTicks();
    U32 dt = now - s_FirstPersonStickTick;
    s_FirstPersonStickTick = now;
    if (dt > 100)
        dt = 100;
    if (!(CameraFirstPerson && DisplayStereo3D && OpenXR_IsActive() &&
          CubeMode == CUBE_EXTERIEUR && !CinemaMode)) {
        s_FirstPersonStickYaw = 0;
        return;
    }
    S32 axis = OpenXR_RightStickX;
    S32 mag = axis < 0 ? -axis : axis;
    if (mag <= JoystickDeadzone)
        return;
    S32 den = 32767 - JoystickDeadzone;
    if (den < 1)
        den = 1;
    S32 rate = ((mag - JoystickDeadzone) * 2048) / den; /* 180 deg/s */
    S32 delta = (rate * (S32)dt) / 1000;
    if (axis < 0)
        delta = -delta;
    s_FirstPersonStickYaw = (s_FirstPersonStickYaw - delta) & 4095;
}

/* Camera-relative movement for first-person stereo.  The hero's walk
 * animation carries its displacement in a frame rotated by Obj.Beta, so
 * pointing Beta along the stick vector and holding I_UP makes the existing
 * walk/collision path strafe, reverse and run diagonals without bypassing
 * its collision checks.  The body is behind the camera in first person, so
 * facing the movement vector for one frame is invisible. */
static void ApplyFirstPersonStickInput(void) {
    UpdateFirstPersonStickYaw();
    if (!(CameraFirstPerson && DisplayStereo3D && OpenXR_IsActive() &&
          CubeMode == CUBE_EXTERIEUR && !CinemaMode))
        return;
    const S32 ax = OpenXR_LeftStickX;
    const S32 ay = OpenXR_LeftStickY;
    const S32 dz = JoystickDeadzone;
    if ((S32)ax * ax + (S32)ay * ay < dz * dz)
        return;
    const float yaw = OpenXR_HeadYawDeg();
    const S32 viewBeta =
        (((S32)(-yaw * 4096.0f / 360.0f)) + s_FirstPersonStickYaw) & 4095;
    /* 0 = forward, +pi/2 = right, pi = backward.  Right of the gaze is a
       negative Beta offset (the same sign the view yaw uses). */
    const float angle = atan2f((float)ax, (float)(-ay));
    const S32 turn = (S32)(angle * 4096.0f / 6.283185307179586f);
    ListObjet[NumObjFollow].Obj.Beta = (viewBeta - turn) & 4095;
    Input |= I_UP;
}
#endif

/* First-person (VR): the camera sits at the hero's eye height.  In an
 * immersive session the head owns the view direction (absolute yaw/pitch);
 * without it the camera looks where the hero faces. */
static void UpdateFirstPersonCamera(void) {
    T_OBJ_3D *ptr3d = &ListObjet[NumObjFollow].Obj;
    S32 beta = ptr3d->Beta;
    S32 alpha = 0; /* horizontal */
    /* Declared before the OpenXR block so its head-roll assignment binds to
       this local: Android's math.h also declares double gamma(double), and
       a bare "gamma" above this point resolves to that function. */
    S32 gamma = 0;
#ifdef LBA2_OPENXR
    if (OpenXR_GetSwapchainWidth()) {
        float yaw = OpenXR_HeadYawDeg();
        float pitch = OpenXR_HeadPitchDeg();
        beta = (S32)(-yaw * 4096.0f / 360.0f) & 4095;
        beta = (beta + s_FirstPersonStickYaw) & 4095;
        alpha = (S32)(-pitch * 4096.0f / 360.0f);
        /* Roll is damped: a flat mono image cannot represent roll without
         * edge distortion, so tilt applies softened (40%) with a +-3
         * degree dead zone - deliberate feel, no size pumping. */
        float roll = OpenXR_HeadRollDeg();
        if (roll > 3.0f)
            roll -= 3.0f;
        else if (roll < -3.0f)
            roll += 3.0f;
        else
            roll = 0.0f;
        gamma = (S32)(roll * 0.4f * 4096.0f / 360.0f);
    }
#endif
    S32 eye = 0;
    { /* head height from the hero ZV (feet at Y, head at Y + YMax) */
        T_OBJET *po = &ListObjet[NumObjFollow];
        eye = (po->YMax > 0) ? po->YMax - 96 : 900;
    }
    VueDistance = 0;
    BetaCam = (2048 - beta) & 4095;
    AlphaCam = alpha;

    Rotate(0, 0, beta);
    VueOffsetX = ptr3d->X + X0;
    VueOffsetY = ptr3d->Y + eye; /* eye height above the hero base */
    VueOffsetZ = ptr3d->Z + Z0;
    StartXCube = VueOffsetX / SIZE_BRICK_XZ;
    StartYCube = VueOffsetY / SIZE_BRICK_Y;
    StartZCube = VueOffsetZ / SIZE_BRICK_XZ;
    /* Walk where you look: the hero turns to the head/camera heading, so
     * "up" goes along the gaze.  This also runs while standing: the body
     * ghost is pinned to the view, and carried sprites (the hero's hands and
     * held items) follow Obj.Beta, so leaving it stale after a right-stick
     * turn keeps them facing the old direction. */
    ListObjet[NumObjFollow].Obj.Beta = beta;
#ifdef LBA2_OPENXR
    GammaCam = gamma;
#endif
    CameraCenter(3);
}

/*──────────────────────────────────────────────────────────────────────────*/
/* Classic/auto camera head-look (stereo).  The rig keeps its game-driven
 * position; the view orientation is the rig framing captured when the look
 * session engages, plus the head's world-relative rotation 1:1 — the rig's
 * later swings (auto re-framing behind the hero, zone cameras) do not pan
 * the view, and head turns read against the world, not against Twinsen,
 * like the first-person path.  In the auto camera, hero movement hands the
 * neutral direction back to the rig (the follow behaviour), so running
 * keeps Twinsen framed at the follow distance; standing still freezes the
 * base into a world-anchored look again.  The tracked head translation
 * rides on the rig position as a clamped lean (StereoCamOffset*).  The
 * camera-center key recaptures the anchor on the current rig framing. */
#ifdef LBA2_OPENXR
static float LookWrapPi(float a) {
    while (a > 3.14159265f)
        a -= 2.0f * 3.14159265f;
    while (a < -3.14159265f)
        a += 2.0f * 3.14159265f;
    return a;
}

static float LookQuatYaw(const float *q) {
    const float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
    const float tx = -2.0f * qy, ty = 2.0f * qx;
    const float cx = -qz * ty;
    const float cz = qx * ty - qy * tx;
    return atan2f(qw * tx + cx, 1.0f - cz);
}

static float LookQuatPitch(const float *q) {
    const float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
    const float tx = -2.0f * qy, ty = 2.0f * qx;
    const float cy = qz * tx;
    float fy = qw * ty + cy;
    if (fy > 1.0f)
        fy = 1.0f;
    else if (fy < -1.0f)
        fy = -1.0f;
    return asinf(fy);
}

static float LookAnchorYaw = 0.0f;   /* neutral heading (rad) */
static float LookAnchorPitch = 0.0f; /* neutral elevation (rad) */
static S32 LookBaseAlpha = 0;        /* rig view angles captured with the anchor */
static S32 LookBaseBeta = 0;
static S32 LookBaseGamma = 0;
static float LookRefPos[3];     /* tracked head centre (m) at capture */
static S32 LookPosMode = -1;    /* which camera the pose ref belongs to */
static S32 LookAnchored = 0;    /* anchor captured for this look session */
static S32 LookRecenterReq = 0; /* set by the recenter key */

/* 1 while the classic/auto head-look override owns the view orientation
 * (stereo session, exterior, non-FP camera, fresh eye views). */
S32 StereoHeadLookActive(void) {
    float q[4], p[3];
    if (CameraFirstPerson || CubeMode != CUBE_EXTERIEUR) {
        return 0;
    }
    if (!OpenXR_GetEyeView(0, q, p) || !OpenXR_GetEyeView(1, q, p)) {
        return 0;
    }
    return 1;
}

/* Stick tilt during a look session: move the session's neutral elevation.
   The rig nudge no longer shows while the world-anchored view owns the
   orientation, so the tilt feeds the anchor instead.  Positive dAlpha is
   the rig's "tilt down". */
void StereoLookShiftAnchorPitch(S32 dAlpha) {
    if (!LookAnchored)
        return;
    LookAnchorPitch += (float)dAlpha * (3.14159265358979f / 2048.0f);
    if (LookAnchorPitch > 1.5f)
        LookAnchorPitch = 1.5f;
    else if (LookAnchorPitch < -1.5f)
        LookAnchorPitch = -1.5f;
}

/* Per-frame head-look offsets (radians) relative to the session anchor.
   (hx,hy,hz) is the tracked head centre in metres; on capture it becomes
   the pose reference the camera translation offsets lean around. */
static void LookComputeOffsets(float *outYaw, float *outPitch, float hx,
                               float hy, float hz) {
    float q[4], p[3];
    float yaw, pitch;
    OpenXR_GetEyeView(0, q, p);
    yaw = LookQuatYaw(q);
    pitch = LookQuatPitch(q);

    if (!LookAnchored || LookRecenterReq) {
        LookAnchorYaw = yaw;
        LookAnchorPitch = pitch;
        LookBaseAlpha = AlphaCam;
        LookBaseBeta = BetaCam;
        LookBaseGamma = GammaCam;
        LookRefPos[0] = hx;
        LookRefPos[1] = hy;
        LookRefPos[2] = hz;
        LookPosMode = 0;
        LookAnchored = 1;
        LookRecenterReq = 0;
    }

    /* Auto camera: while the hero moves, the rig owns the neutral direction
       — the base tracks the live follow angles, so the view keeps framing
       Twinsen at the follow distance while running.  The head's offsets
       stay world-relative 1:1 on top, and standing still freezes the base
       back into a fully world-anchored look. */
    if (FollowCamera AND(Input & (I_UP | I_DOWN))) {
        LookBaseAlpha = AlphaCam;
        LookBaseBeta = BetaCam;
        LookBaseGamma = GammaCam;
    }

    *outYaw = LookWrapPi(yaw - LookAnchorYaw);
    *outPitch = pitch - LookAnchorPitch;
}
#else
S32 StereoHeadLookActive(void) {
    (void)0;
    return 0;
}
void StereoLookShiftAnchorPitch(S32 dAlpha) {
    (void)dAlpha;
}
#endif

/*──────────────────────────────────────────────────────────────────────────*/
/* Cycle the camera setting: classic -> auto -> first person -> classic.
 * Shared by the Advanced-options camera row and the camera-switch input
 * (right stick press). */
void CycleCameraSetting(void) {
#ifdef LBA2_OPENXR
    s_FirstPersonStickYaw = 0;
#endif
    if (CameraFirstPerson) {
        CameraFirstPerson = 0;
        FollowCamera = 0;

        /* Entering the classic camera starts from the hero, not from the
           first-person eye pose the frame before. */
        Rotate(0, DISTANCE_VISEE, ListObjet[NumObjFollow].Obj.Beta);
        StartXCube = (ListObjet[NumObjFollow].Obj.X + X0) / SIZE_BRICK_XZ;
        StartYCube =
            ((ListObjet[NumObjFollow].Obj.Y + SIZE_BRICK_Y) / SIZE_BRICK_Y);
        StartZCube = (ListObjet[NumObjFollow].Obj.Z + Z0) / SIZE_BRICK_XZ;
        CameraCenter(1);
    } else if (FollowCamera) {
        FollowCamera = 0;
        CameraFirstPerson = 1;
    } else {
        FollowCamera = 1;
        /* Default auto-camera framing: closer and lower than the classic rig. */
        FollowCamBaseDist = FOLLOW_CAM_INITIAL_DIST;
        FollowCamEffectiveDist = FollowCamBaseDist;
        AlphaCam = FOLLOW_CAM_DEFAULT_ALPHA;
        FollowCamPrevBaseDist = -1;
        FollowCamPrevAlphaCam = -1;
    }
    FirstTime = AFF_ALL_FLIP;
}

/* True when PERSO_RenderStereo renders the scene once per eye and therefore
   owns the frame's present. */
S32 PERSO_StereoRenderActive(void) {
#if defined(LBA2_OPENXR) && defined(__ANDROID__)
    return DisplayStereo3D AND OpenXR_IsActive() AND GPURendererActive;
#else
    return 0;
#endif
}

/* Depth factor for the interior stereo baseline: 1.0 is the full IPD, a
   smaller value flattens the room's own parallax.  LBA2_INTERIOR_STEREO is a
   percentage, for comparing builds without a rebuild. */
static float InteriorStereoScale(void) {
    static float scale = -1.0f;
    if (scale < 0.0f) {
        const char *env = getenv("LBA2_INTERIOR_STEREO");
        float v = (env != NULL) ? (float)atoi(env) / 100.0f : 0.55f;
        if (v < 0.1f)
            v = 0.1f;
        if (v > 1.0f)
            v = 1.0f;
        scale = v;
        Log_Info("Interior stereo depth scale: %.2f", (double)scale);
    }
    return scale;
}

/* Render one scene frame in stereo when the headset owns the view, or as a
   single mono frame otherwise.  Shared by MainLoop and the modal dialogue
   path so dialogue keeps the world in stereo. */
void PERSO_RenderStereo(S32 flagflip) {
    VrBody_Update();
#if defined(LBA2_OPENXR) && defined(__ANDROID__)
    if (PERSO_StereoRenderActive()) {
        /* Modal dialogue renders outside MainLoop's OpenXR frame start.
           Begin the frame here; EnsureFrameBegun is a one-shot, so the
           main loop's later BeginFrame is a no-op inside this same frame. */
        OpenXR_BeginFrame();
        /* Native stereo: render the scene once per eye. Full redraws per
             * eye — the incremental dirty-box path would carry the other
             * eye's leftovers. The left eye's composite is captured offscreen
             * (present + clock step suppressed); the right eye renders
             * through the normal path and its OpenXR_PresentMono submits
             * both eye images. */
        extern S32 VueDistance;
        extern S32 ChampX;
        extern S32 StereoCamShiftX;
        extern S32 StereoCamShiftY;
        extern S32 StereoProjShiftX;
        /* Eye separation in world units (~1.5x natural IPD at LBA2's
             * scale, for a moderate depth pop). */
        const S32 stereoIPD = 30;
        const float eyeSep = stereoIPD * 0.5f;
        /* True per-eye poses: each eye renders from its tracked view —
             * the compositor time-warps each submitted image from exactly
             * the pose declared in the projection layer, so rendering from
             * that same (one-frame-stale) pose turns head rotation into a
             * clean reprojection instead of a warp from an unrelated rig
             * camera, and the IPD split along the head's right axis makes
             * the stereo depth real (no convergence shear, no flat-image
             * roll damping).  Exterior only: interior room cameras sit
             * inside walls, where free head look would show geometry from
             * behind.  No cached views (first frame, mono session) falls
             * back to the rig-offset pair below. */
        float eyeQ[2][4];
        float eyeP[2][3];
        /* Both camera modes render the tracked view 1:1, anchored to the
             * world: first person from the exact head pose, the classic and
             * auto cameras from their rig framing captured when the look
             * session engages, plus the head's world-relative rotation — the
             * rig's later swings around Twinsen do not pan the view.  With
             * no cached views (first frame, mono session) everything falls
             * back to the plain rig-offset pair below. */
        S32 haveViews = OpenXR_GetEyeView(0, eyeQ[0], eyeP[0]) AND
            OpenXR_GetEyeView(1, eyeQ[1], eyeP[1]);
        S32 truePose = CameraFirstPerson AND
                           CubeMode == CUBE_EXTERIEUR AND haveViews;
        S32 lookPose = !CameraFirstPerson AND
                           CubeMode == CUBE_EXTERIEUR AND haveViews;
        /* OpenXR eye views are parallel on every runtime this ships on:
               the two poses share one orientation and differ only by the IPD
               translation.  When that holds, the per-eye difference IS a pure
               translation, so the single-pass stereo path can render both
               eyes from one scene pass.  Single-pass applies the eye delta as
               a translation only, so ANY orientation difference would leave
               the second eye rotated by that angle — several pixels on a
               high-frequency transparent texture, and it changes with the
               gaze.  Require the two quaternions to be effectively identical;
               a runtime that toes the eyes in keeps the exact two-pass
               renderer. */
        S32 eyesParallel = 0;
        float eyeQd = 1.0f;
        if (haveViews) {
            float qd = eyeQ[0][0] * eyeQ[1][0] +
                       eyeQ[0][1] * eyeQ[1][1] +
                       eyeQ[0][2] * eyeQ[1][2] +
                       eyeQ[0][3] * eyeQ[1][3];
            if (qd < 0.0f)
                qd = -qd;
            eyeQd = qd;
            eyesParallel = (qd > 0.9999995f);
        }
        {
            static S32 s_eyeLog = 0;
            if (!s_eyeLog) {
                Log_Info("[VR] Eye quaternion dot %.8f -> %s stereo",
                         (double)eyeQd,
                         eyesParallel ? "single-pass" : "two-pass");
                s_eyeLog = 1;
            }
        }
        float lookYawOff = 0.0f;
        float lookPitchOff = 0.0f;
        /* Head-centre tracked position: mid of the two eye views, metres
             * on the tracking axes (X right, Y up, Z back). */
        float headX = 0.0f;
        float headY = 0.0f;
        float headZ = 0.0f;
        if (haveViews) {
            headX = 0.5f * (eyeP[0][0] + eyeP[1][0]);
            headY = 0.5f * (eyeP[0][1] + eyeP[1][1]);
            headZ = 0.5f * (eyeP[0][2] + eyeP[1][2]);
        }
        if (lookPose) {
            LookComputeOffsets(&lookYawOff, &lookPitchOff,
                               headX, headY, headZ);
        } else {
            LookAnchored = 0; /* re-capture the anchor on re-entry */
        }
        /* Tracked head translation relative to the pose reference
             * captured when this camera mode engaged (or on recenter): a
             * lean around the rig position, not a distance from the room's
             * tracking origin. */
        float headDx = 0.0f;
        float headDy = 0.0f;
        float headDz = 0.0f;
        if (truePose) {
            if (LookPosMode != 1 || LookRecenterReq) {
                LookRefPos[0] = headX;
                LookRefPos[1] = headY;
                LookRefPos[2] = headZ;
                LookPosMode = 1;
                LookRecenterReq = 0;
            }
            headDx = headX - LookRefPos[0];
            headDy = headY - LookRefPos[1];
            headDz = headZ - LookRefPos[2];
        } else if (lookPose) {
            if (LookPosMode != 0) {
                LookRefPos[0] = headX;
                LookRefPos[1] = headY;
                LookRefPos[2] = headZ;
                LookPosMode = 0;
            }
            headDx = headX - LookRefPos[0];
            headDy = headY - LookRefPos[1];
            headDz = headZ - LookRefPos[2];
        }
        S32 conv = 0;
        S32 dx = 0;
        S32 dy = 0;
        if (!truePose && !lookPose) {
            /* The zero-disparity plane sits at the convergence distance.
                 * First person: the camera IS the hero's eyes, so
                 * VueDistance is 0 - converging there would shear the whole
                 * terrain.  Keep the comfort plane at walking distance. */
            S32 convDist = VueDistance;
            if (CameraFirstPerson AND convDist < 4000)
                convDist = 4000;
            conv = stereoIPD * ChampX / (2 * (convDist + 512));
            if (conv < 0)
                conv = 0;
            /* The eyes roll with the head: rotate the IPD offset by the
                 * head roll so the per-eye disparity direction matches what
                 * a rolled head actually sees.  Without this, rolling pumps
                 * the apparent size of objects (the offset stays
                 * screen-level). */
            const float rollRad = -GammaCam * 3.14159265358979f / 2048.0f;
            dx = (S32)(eyeSep * cosf(rollRad));
            dy = (S32)(eyeSep * sinf(rollRad));
        }
        /* The projection spans the headset's per-eye field (the values
             * OPENXR.CPP derived from the runtime's eye FOVs), overriding
             * the classic focal floors for these two passes only — restored
             * after the pair so menus and the flat pipeline keep the game
             * focal. */
        const S32 gameLFactorX = LFactorX;
        const S32 gameLFactorY = LFactorY;
        const float gameFRatioX = FRatioX;
        const float gameFRatioY = FRatioY;
        U32 stereoT0 = SDL_GetTicks();
        U32 stereoT1 = stereoT0;
        /* Eye 1's camera setup runs once before the draw passes so
               eye 0 can supply its world/projection delta to the per-eye
               single-pass shaders. */
        float eye1ShiftX = 0.0f, eye1ShiftY = 0.0f, eye1ShiftZ = 0.0f;
        float eye1ProjX = 0.0f;
        S32 eye1Captured = 0;
        for (S32 pass = 0; pass < 3; pass++) {
            const S32 eye = (pass == 0) ? 1 : (pass - 1);
            if (pass > 0) {
                GL_SetStereoEye(eye);
            }
            if (truePose) {
                const float *q = eyeQ[eye];
                /* Orientation from the tracked quaternion, read the
                     * convention-proof way: forward = q rotated (0,0,-1),
                     * yaw/pitch off the forward vector, roll off the head
                     * up vector's lean toward cross(forward, world up). */
                const float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
                const float tx = -2.0f * qy, ty = 2.0f * qx;
                const float cx = -qz * ty;
                const float cy = qz * tx;
                const float cz = qx * ty - qy * tx;
                const float fx = qw * tx + cx;
                float fy = qw * ty + cy;
                const float fz = -1.0f + cz;
                if (fy > 1.0f)
                    fy = 1.0f;
                else if (fy < -1.0f)
                    fy = -1.0f;
                const float yaw = atan2f(fx, -fz);
                const float pitch = asinf(fy);
                /* Head up = q rotated (0,1,0). */
                const float ux = 2.0f * qx * qy - 2.0f * qw * qz;
                const float uy = 1.0f - 2.0f * qx * qx - 2.0f * qz * qz;
                const float uz = 2.0f * qw * qx + 2.0f * qy * qz;
                const float roll = atan2f(ux * (-fz) + uz * fx, uy);
                /* Engine angles: 4096 per turn, BetaCam = 2048 faces
                     * straight ahead (same signs the first-person camera
                     * uses for the same head pose).  The tracked angles go
                     * into the stereo eye override, which AffGrilleExt
                     * applies AFTER the rig's own SetFollowCamera: the rig
                     * position stays the game-controlled one (plus the
                     * tracked lean below) while the render orientation is
                     * the exact tracked pose — the pose the compositor
                     * time-warps from, which is what keeps head rotation
                     * artifact-free.  Anything set only on the renderer
                     * state before AffScene would be wiped by that
                     * mid-frame SetFollowCamera. */
                const float K = 2048.0f / 3.14159265358979f;
                StereoEyeOverride = 1;
                /* Right-stick steering is a Beta offset, not a head
                       rotation: the base camera already carries it, and the
                       per-eye override would otherwise render straight from
                       the tracked pose and ignore the stick. */
                StereoEyeBeta =
                    ((S32)(2048.0f + yaw * K) - s_FirstPersonStickYaw) &
                    4095;
                StereoEyeAlpha = (S32)(-pitch * K);
                StereoEyeGamma = (S32)(roll * K);
                /* The camera rolls with the head, so the head's right
                     * axis IS view-space X: the IPD split is a plain lateral
                     * shift.  StereoCamShift rides inside SetCamera, so it
                     * also survives the mid-frame re-application. */
                StereoCamShiftX = (eye == 0) ? -(S32)eyeSep : (S32)eyeSep;
                StereoCamShiftY = 0;
                StereoProjShiftX = 0;
                /* 6dof: the tracked translation leans the rig position
                     * along the view this eye renders with. */
                StereoLookHeadOffsetToWorld(headDx, headDy, headDz,
                                            StereoEyeBeta,
                                            &StereoCamOffsetX,
                                            &StereoCamOffsetY,
                                            &StereoCamOffsetZ);
            } else if (lookPose) {
                /* World-anchored head look: the rig framing captured at
                     * anchor time plus the tracked view 1:1 (warp-free — the
                     * applied orientation tracks the head exactly).  The
                     * rig's later swings around Twinsen no longer pan the
                     * view; the camera-center key recaptures the anchor on
                     * the current framing.  Same surviving-injection rules
                     * as the first-person path. */
                StereoEyeOverride = 1;
                StereoEyeAlpha = StereoLookViewAlpha(LookBaseAlpha,
                                                     lookPitchOff);
                StereoEyeBeta = StereoLookViewBeta(LookBaseBeta,
                                                   lookYawOff);
                StereoEyeGamma = LookBaseGamma;
                StereoCamShiftX = (eye == 0) ? -(S32)eyeSep : (S32)eyeSep;
                StereoCamShiftY = 0;
                StereoProjShiftX = 0;
                /* 6dof: the tracked translation leans the rig position
                     * along the view this eye renders with. */
                StereoLookHeadOffsetToWorld(headDx, headDy, headDz,
                                            StereoEyeBeta,
                                            &StereoCamOffsetX,
                                            &StereoCamOffsetY,
                                            &StereoCamOffsetZ);
            } else {
                StereoCamShiftX = (eye == 0) ? -dx : dx;
                StereoCamShiftY = (eye == 0) ? -dy : dy;
                StereoProjShiftX = (eye == 0) ? -conv : conv;
                StereoCamOffsetX = 0;
                StereoCamOffsetY = 0;
                StereoCamOffsetZ = 0;
            }
            if (pass == 0) {
                eye1ShiftX = (float)StereoCamShiftX + StereoCamOffsetX;
                eye1ShiftY = (float)StereoCamShiftY + StereoCamOffsetY;
                eye1ShiftZ = StereoCamOffsetZ;
                eye1ProjX = (float)StereoProjShiftX;
                eye1Captured = 1;
                continue;
            }
            if (pass == 1) {
                /* A tracked per-eye orientation is not a pure
                       translation; two passes are only needed when the eye
                       quaternions actually differ. */
                if (eye1Captured && (!truePose || eyesParallel) &&
                    GL_SinglePassStereoSupported()) {
                    const float e0x =
                        (float)StereoCamShiftX + StereoCamOffsetX;
                    const float e0y =
                        (float)StereoCamShiftY + StereoCamOffsetY;
                    const float e0z = StereoCamOffsetZ;
                    /* An isometric room is a painter-ordered
                           composite, not real geometry: the full baseline
                           separates a near wall from the wall behind it and
                           opens a strip mono never showed.  Scaling the eye
                           delta keeps that strip closed, and because every
                           surface (background, models, flows) takes the same
                           factor, nothing slides against anything else. */
                    const float depthScale =
                        (CubeMode == CUBE_INTERIEUR)
                            ? InteriorStereoScale()
                            : 1.0f;
                    /* The single-pass shader maps the content into one
                           half with cx*0.5, which also halves the parallax.
                           First person needs the full tracked IPD, so its
                           delta is doubled here and the half-mapping cancels
                           it; classic/auto keep their subtler stereo. */
                    const float fpEyeScale = truePose ? 2.0f : 1.0f;
                    GL_SetSinglePassEyeDelta(
                        (eye1ShiftX - e0x) * depthScale * fpEyeScale,
                        (eye1ShiftY - e0y) * depthScale * fpEyeScale,
                        (eye1ShiftZ - e0z) * depthScale * fpEyeScale,
                        (eye1ProjX - (float)StereoProjShiftX) *
                            depthScale * fpEyeScale,
                        0.0f);
                    GL_SetSinglePassStereo(1);
                } else {
                    GL_SetSinglePassStereo(0);
                }
            }
            if (StereoFocalX > 0 && StereoFocalY > 0) {
                LFactorX = StereoFocalX;
                LFactorY = StereoFocalY;
                FRatioX = (float)LFactorX;
                FRatioY = -(float)LFactorY / FRatioX;
            }
            if (pass > 0 && eye == 1)
                stereoT1 = SDL_GetTicks();
            if (eye == 0) {
                /* Opt-in: hold the swapchain image across both eye passes
                     * and render straight into it.  Failure leaves the normal
                     * double-wide target and blit path untouched. */
                U32 directTexture = 0;
                U32 directWidth = 0;
                U32 directHeight = 0;
                if (OpenXR_AcquireStereoImage(&directTexture, &directWidth,
                                              &directHeight)) {
                    if (!GL_BeginDirectStereo(directTexture, directWidth,
                                              directHeight)) {
                        OpenXR_AbortDirectStereo();
                    }
                }
            }
            {
                S32 eyePhase = (eye == 0) ? PERFTRACE_PHASE_EYE0
                                          : PERFTRACE_PHASE_EYE1;
                Perftrace_PhaseBegin(eyePhase);
                AffScene(AFF_ALL_FLIP);
                Perftrace_PhaseEnd(eyePhase);
            }
        }
        /* Present normally releases the held image.  This abort covers a
             * frame that left the eye loop without presenting. */
        OpenXR_AbortDirectStereo();
        U32 stereoT2 = SDL_GetTicks();
        LFactorX = gameLFactorX;
        LFactorY = gameLFactorY;
        FRatioX = gameFRatioX;
        FRatioY = gameFRatioY;
        StereoEyeOverride = 0;
        StereoCamShiftX = 0;
        StereoCamShiftY = 0;
        StereoProjShiftX = 0;
        StereoCamOffsetX = 0;
        StereoCamOffsetY = 0;
        StereoCamOffsetZ = 0;
        {
            static U32 s_stereoPerfFrame = 0;
            s_stereoPerfFrame++;
            if (s_stereoPerfFrame % 300 == 150) {
                Log_Info("StereoPerf: eye0=%dms eye1=%dms frame=%dms flushes=%d truepose=%d",
                         stereoT1 - stereoT0, stereoT2 - stereoT1,
                         stereoT2 - stereoT0, GL_BatchFlushCount,
                         truePose);
            }
        }
        GL_SetSinglePassStereo(0);
        GL_SetStereoEye(-1);
        return;
    }
#endif
    /* Mono frames still call this after stereo was switched off so
             * the double-wide target is released before the scene draws into
             * what it expects to be a single-wide FBO. */
#ifdef USE_GPURENDERER
    GL_SetStereoEye(-1);
#endif
    AffScene(flagflip);
}

/* Advance the visible world while a modal dialogue blocks the main loop.
   The hero and speaker are owned by the dialogue itself; other bodies keep
   animating, and extras/rain/water/camera stay live. */
void PERSO_DialogUpdateWorld(void) {
    /* Per-eye IPD shift/lean belongs to the eye passes PERSO_RenderStereo
       sets up; a value left from the previous frame must not reach the
       camera update below. */
    StereoCamShiftX = 0;
    StereoCamShiftY = 0;
    StereoProjShiftX = 0;
    StereoCamOffsetX = 0;
    StereoCamOffsetY = 0;
    StereoCamOffsetZ = 0;
    StereoEyeOverride = 0;

    if (CameraFirstPerson AND CubeMode == CUBE_EXTERIEUR) {
        UpdateFirstPersonCamera();
    } else if (FollowCamera AND CubeMode == CUBE_EXTERIEUR) {
        UpdateFollowCameraExt();
    }

#ifdef LBA2_OPENXR
    /* Interior: the hover rig and the right-stick pan/zoom stay live while a
       dialogue blocks the main loop, as the exterior cameras above do.  The
       room keeps its own projection centre, so a look or a pan still moves
       the scene behind the box. */
    if (!CinemaMode && CubeMode == CUBE_INTERIEUR)
        UpdateInteriorCameraControl();
#endif

    StepFalling = GetDeltaMove(&RealFalling);
    StepShifting = GetDeltaMove(&RealShifting);
    S32 pansample = GetDeltaMove(&SampleAlwaysMove);
    (void)pansample;

    CameraZone = FALSE;
    GereAmbiance();
    CheckNextMusic();
    GereExtras();
    AnimAllFlow();

    if (CubeMode == CUBE_EXTERIEUR) {
        if (Island == 0 AND TEMPETE_ACTIVE AND RainEnable)
            GereRain();
        DoAnimatedPolys();
    }

    for (S32 i = 0; i < NbObjets; i++) {
        T_OBJET *o = &ListObjet[i];
        if (i == NUM_PERSO || i == (S32)NumObjDial)
            continue;
        if (o->WorkFlags & OBJ_DEAD)
            continue;
        if (o->CarryBy == -1)
            o->OldBeta = o->Obj.Beta;
        DoDir((U8)i);
        o->OldPosX = o->Obj.X;
        o->OldPosY = o->Obj.Y;
        o->OldPosZ = o->Obj.Z;
        if (o->OffsetTrack != -1)
            DoTrack((U8)i);
        if (!FirstLoop)
            PtrDoAnim((U8)i);
    }

    ManageSystem();
}

S32 MainLoop() {
    S32 xm, ym, zm;
    T_OBJET *ptrobj;
    U8 i;
    S32 pansample;

    // #358 fixed-timestep simulation: game-clock time of the last simulated sub-step.
    static U32 LastSimRefHR = 0;
    // Cap on sub-steps per rendered frame (spiral-of-death guard). Below ~8 fps the sim
    // stops fully catching up and slows in a bounded way rather than freezing on catch-up.
    const S32 FIXED_TIMESTEP_MAX_STEPS = 8;

    SetDetailLevel(); // prise en compte des différentes options

restartloop:

    FirstTime = AFF_ALL_FLIP;
    FlagLoadGame = FALSE;

    InitWaitNoKey();
    ClearWaitNoInput();
    InitWaitNoInput(I_FIRE | I_MENUS | I_OPTIONS | I_SAVE | I_LOAD);

    /*-------------------------------------------------------------------------*/

    if (EscTimer) {
        SetTimerHR(EscTimer);
        ResumeMusic(FALSE);
    }

    EscTimer = 0;

    /*-------------------------------------------------------------------------*/

    /* vitesse de chute propor pour tous les objets */
    InitMove(&RealFalling, -2560);
    /* vitesse de decalage propor tapis roulant */
    InitMove(&RealShifting, 640);

    /* frequence de paning des samples always */
    InitMove(&SampleAlwaysMove, 2);

    /*-------------------------------------------------------------------------*/

    FirstSave = TRUE;

    while (TRUE) {
    startloop:

        Perftrace_FrameBegin();
        Perftrace_PhaseBegin(PERFTRACE_PHASE_SIMULATION);

        /* Runtime resolution-switch keep/revert dialog. Pending whenever
           cmd_resolution (or a future menu path) fires a switch; runs once
           per pending arming, then clears itself. Drives its own modal tick
           loop, so by the time control returns the user has chosen Keep or
           Revert (or the 15 s timeout fired). No-op when nothing is pending. */
        Res_RunRevertDialogIfPending();

        /*#ifdef  THOMAS
		if( DefKeys[0].Key1==0 )
		{
			PalOne( LBAWHITE, 255, 255, 255 ) ;
			CoulText( LBAWHITE, 0 ) ;
			GraphPrintf( TRUE, 0, 0, "STOP: Patch Inputs trouvé !!!", TRUE ) ;
			while( Key ) ;
			while( !Key ) ;
		}
#endif
*/
        ptrobj = &ListObjet[NUM_PERSO];

        // DEBUG pour trouver bug Alpha

        // debug patch Gamma
        if (ptrobj->Obj.Alpha != 0 OR ptrobj->Obj.Gamma != 0) {
            Message("STOP: 'j'ai trouvé le Patch_Alpha' !", TRUE);
        }

        /*-------------------------------------------------------------------------*/

        if (NewCube != -1) {
#ifdef DEMO
            if (NewDemoSave != CurrentDemoSave) {
                FlagChgCube = 0;

                DemoLogo();

                if (NewDemoSave > NB_DEMOSAVE - 1) {
                    return 1;
                } else {
                    SetDemoSaveGame(NewDemoSave);

                    StopMusic();

                    InitGame(-1);
                    ChangeCube();

                    CopyScreen(Log, ScreenAux);

                    RestoreTimer();
                    SaveTimer();
                    AffScene(AFF_ALL_NO_FLIP);
                    FixeCinemaMode(FALSE);
                    CopyScreen(Log, Screen);

                    // au cas ou le mec meurre avant de faire <ESC>
                    if (!Save_AutoDisabled())
                        CurrentSaveGame();

                    BoxReset(); // pas de flip

                    // In-game save zoom. Float to match the menu-driven sibling
                    // (GAMEMENU MainGameMenu): 80 = SCREEN_SAVE_WIDTH/2 so X tracks
                    // SAVE_THUMB_X, and Y floats with the rest of the UI. No-op at 640x480.
                    ZoomSavedGame((S32)ModeDesiredX / 2 - 80, 10 + UI_VCENTER_OFS);

                    FlagFade = FALSE;
                    RestartRainSample = TRUE;
                    HQ_ResumeSamples();

                    ClearPlasmaMenu();

                    RestoreTimer();

                    goto restartloop;
                }
            } else
#endif
            {
                ChangeCube();
            }

            FirstLoop = TRUE; // premiere boucle d'un cube
                              // ATTENTION: a verifier si
                              // cela ne pose pas de probleme !
            FlagReajustPosTwinsen = TRUE;
        }

        /* CLI control harness seam: no-op until armed. Runs --exec on the first tick
         * (scene is loaded by the block above) and finalizes after N ticks. */
        Control_TickHook();

/*-------------------------------------------------------------------------*/
#ifdef DEBUG_TOOLS
        UnsetClip();

        if (CmptMemoTimerRef != 0) {
            CoulText(LBAWHITE, 0);
            GraphPrintf(FALSE, 0, 10, "Warning, SaveTimer & RestoreTimer Decale!!!");
            GraphPrintf(FALSE, 0, 20, "Valeur CmptMemoTimerRef = %d", CmptMemoTimerRef);
        }
#endif

        /*-------------------------------------------------------------------------*/

        // en attendant une methode propre de gestion de memoire
        // des samples !
        if (RestartRainSample) {
            StartRainSample();
            LaunchAmbiance();
        }

        /*-------------------------------------------------------------------------*/

        LastInput = Input;

#ifdef LBA2_OPENXR
        JoystickFirstPersonStick =
            (CameraFirstPerson && DisplayStereo3D && OpenXR_IsActive() &&
             CubeMode == CUBE_EXTERIEUR && !CinemaMode)
                ? 1
                : 0;
#endif
        MyGetInput();
        JoystickFirstPersonStick = 0;

        // Harness input timeline (docs/INPUT_SIM_PLAN.md): OR the currently-held injected bits into
        // this frame's Input so the harness drives the hero without a keyboard. A `seq` hold is
        // metered in SIM ticks (Control_InputAdvance below, once per sim-running frame) so the
        // throttle cannot gobble it; an `fseq` event is metered in RENDERED frames (the frame clock
        // advances here, every frame) so a press can be placed on a skipped frame to reproduce the
        // throttle edge-drop.
        if (Control_InputPending()) {
            Input |= Control_InputCurrentMask();
            Control_InputFrameTick();
        }

#ifdef LBA2_OPENXR
        ApplyFirstPersonStickInput();
#endif

        ManageTime();

        if (DemoSlide) {
            // Stuck-cube watchdog (interactive play only). The attract reel is a scripted
            // showcase, but some scenes (cube 201's Desert Island bat sequence in particular)
            // can derail per-host: RNG-driven wandering creatures push Twinsen off his
            // authored track and the SET_CUBE opcode that ends the scene never fires. Bound
            // the worst-case "stuck hero on screen" exposure by ending the reel gracefully
            // if any one cube runs longer than CUBE_TIMEOUT_MS. Skipped under the CLI
            // harness so long --demo --tick runs traverse the full cube state.
            // Threshold tuned to 2x the longest natural scene measured under --fixed-dt 16
            // (cube 211 = 61s; cubes 207/210/202/214/213 in the 30-42s range; everything
            // else under 30s). 120s gives comfortable headroom for slow hosts while keeping
            // the worst-case "stuck on screen" exposure bounded at 2 minutes. Resets each
            // time NumCube changes. See issue #176.
            if (!Control_IsActive()) {
                static const U32 CUBE_TIMEOUT_MS = 120000;
                static S32 last_watchdog_cube = -1;
                static U32 watchdog_entry_time = 0;
                if (NumCube != last_watchdog_cube) {
                    last_watchdog_cube = NumCube;
                    watchdog_entry_time = TimerRefHR;
                } else if (TimerRefHR - watchdog_entry_time > CUBE_TIMEOUT_MS) {
                    HQ_StopSample();
                    SavingEnable = FALSE;
                    last_watchdog_cube = -1; // reset for the next demo entry
                    return 0;                // graceful exit, ESC-equivalent
                }
            }

            if (MyKey
#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
                    AND MyKey != K_T
#endif
            ) {
                HQ_StopSample();
                SavingEnable = FALSE;
                return (MyKey != K_ESC);
            }

            /*                      if( HigTrack>1 AND GetMusicCD()==-1 )
                        {
                                S32     num = MyRnd(HigTrack-1)+1 ;

                                PlayCdTrack( num ) ;
                        }
*/
        }

        /*-------------------------------------------------------------------------*/
        /* gestion clavier */ /* tools */

        // Faut-il le laisser ? (non, à mon avis !)
        if (DemoSlide AND MyKey == K_T) {
            SetTimerHR(TimerRefHR + 200);
        }

#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
        if (MyKey == K_D) {
            AffDebugMenu();
        }

        if (MyKey == K_F) {
            DebugFps ^= 1;
            InitWaitNoKey();
        }
#endif

#ifdef DEBUG_TOOLS
        if (MyKey == K_T) {
            SetTimerHR(TimerRefHR + 200);
        }

        if (MyKey == K_Z) {
            DrawZVObjets ^= 1;
            InitWaitNoKey();
        }

        if (MyKey == K_E) {
            FlagDrawHorizon ^= 1;
            FirstTime = AFF_ALL_FLIP;
            while (MyKey)
                MyGetInput();
        }

        if (MyKey == K_F12) {
            AsciiMode ^= 1;
            while (MyKey)
                MyGetInput();
        }

        if (MyKey == K_F10) {
            if (CubeMode == CUBE_EXTERIEUR) {
                S32 n;
                S32 lasttimer = TimerRefHR;

                SaveTimer();

                CoulText(LBAWHITE, 0);

                for (n = 0; n < 20; n++) {
                    ManageTime();

                    if (FlagDrawHorizon) {
                        DrawHorizon2ZBuf();
                    }

                    // Doit Rajouter sa boite
                    AffichageTerrainZBuf();
                    GraphPrintf(FALSE, 0, 0, "%d Drawn", n);
                    BoxStaticFullflip();
                }

                GraphPrintf(TRUE, 0, 10, "Time for 20 Loops: %d", TimerRefHR - lasttimer);

                while (MyKey)
                    MyGetInput();
                while (!MyKey)
                    MyGetInput();

                RestoreTimer();
            } else {
                LogPrintf("F10 benchmark only works in exterior scenes\n");
            }
            while (MyKey)
                MyGetInput();
        }

        if (MyKey == K_F11) {
            if (CubeMode == CUBE_EXTERIEUR) {
                S32 n;
                S32 lasttimer = TimerRefHR;

                SaveTimer();

                CoulText(LBAWHITE, 0);

                for (n = 0; n < 20; n++) {
                    ManageTime();

                    AffScene(AFF_ALL_NO_FLIP);
                    GraphPrintf(FALSE, 0, 0, "%d Drawn", n);
                    BoxUpdate();
                }

                GraphPrintf(TRUE, 0, 10, "Time for 20 Loops: %d", TimerRefHR - lasttimer);

                while (MyKey)
                    MyGetInput();
                while (!MyKey)
                    MyGetInput();

                RestoreTimer();
            } else {
                LogPrintf("F11 benchmark only works in exterior scenes\n");
            }
            while (MyKey)
                MyGetInput();
        }

        if (MyKey == K_V) {
            if (MagicBall != -1) {
                ptrobj->Obj.X = ListExtra[MagicBall].PosX;
                ptrobj->Obj.Y = ListExtra[MagicBall].PosY;
                ptrobj->Obj.Z = ListExtra[MagicBall].PosZ;
            }
        }

        if (MyKey == K_M) {
            SaveTimer();

            AffPalette();

            FirstTime = AFF_ALL_FLIP;
            RestoreTimer();
        }

        if (MyKey == K_K) {
            ExtraBonus(ptrobj->Obj.X,
                       ptrobj->Obj.Y + 2000,
                       ptrobj->Obj.Z,
                       (256 * MUL_ANGLE), 0,
                       6, 1);
        }

        if (MyKey == K_A) {
            SaveTimer();
            if (!FlagFade)
                FadeToBlackAndSamples(PtrPal);

            FlagFade = TRUE;

            PlayAcf("bu");

            if (CubeMode == CUBE_INTERIEUR) {
                InitGrille(NumCube);
            }
            RazListPartFlow();
            ChoicePalette();

            FlagFade = TRUE;
            FirstTime = AFF_ALL_FLIP;
            RestoreTimer();
        }
#endif // debug tools

#ifdef ENABLE_POLY_RECORDING
        // Scene snapshot / polygon recording capture (Alt+F9)
        if (Key == K_F9 && (SDL_GetModState() & SDL_KMOD_ALT)) {
            SnapshotRequested = 1;
            while (Key)
                MyGetInput();
        }
#endif

        CheckSavePcx(); // F9: Sauve Screen

        /* Console screenshot (PNG): performed here where Log + PtrPal are valid.
           Capture Log, the presented buffer (SDL PresentFrame uploads Log); Screen
           is the clean-background secondary and misses the per-frame 3D models and
           the foreground-brick re-blit. */
        {
            char shotPath[ADELINE_MAX_PATH];
            int is_png = 0;
            if (Console_HasPendingScreenshot(shotPath, sizeof(shotPath), &is_png) && is_png) {
#ifdef USE_GPURENDERER
                if (GPURendererActive) {
                    GL_SavePresentPNG(shotPath);
                } else
#endif
                {
                    SavePNG(shotPath, Log, ModeDesiredX, ModeDesiredY, PtrPal);
                }
                Console_ClearPendingScreenshot();
            }
        }

        /*-------------------------------------------------------------------------*/
        /* gestion clavier */ /* game */

        if (!FlagFade) {
            if (Input & I_HELP) {
                SaveTimer();
                StopSpeak();
                BackupScreen(FALSE);
                CopyScreen(Log, Screen);
                BoxReset();
                InitDial(0);

                HQ_StopSample();
                RestartRainSample = TRUE;

                InitPlasmaMenu();
                PauseMusic(TRUE);

                MenuConfig();

                ClearPlasmaMenu();

                InitDial(START_FILE_ISLAND + Island);
                PaletteSync(PtrPal);
                RestoreScreen();
                ResumeMusic(TRUE);
                RestoreTimer();
            }

            if ((MyKey == K_ESC OR Input & I_MENUS)) {
                if (CinemaMode > 1) {
                    ResetCinemaMode();
                } else {
                    StopSpeak();
                    PauseMusic(FALSE);
                    EscTimer = TimerRefHR;

                    if (Comportement == C_PROTOPACK OR Comportement == C_JETPACK) {
                        // pour pouvoir restaurer anim marche protopack
                        if (ptrobj->GenAnim == GEN_ANIM_MARCHE) {
                            TimerProto = EscTimer + TEMPO_PROTO_AUTO; // 1/2 s
                        }
                    }

                    if ((ptrobj->LifePoint > 0)
                            AND(ptrobj->Obj.Body.Num != -1)
                                AND(!(ptrobj->Flags & INVISIBLE))
                                    AND(NumCube != NUM_CUBE_PHANTOM)
                                        AND !DemoSlide
                                            AND !PLAY_THE_END) {
                        SavingEnable = TRUE;
                    } else {
                        SavingEnable = FALSE;
                    }
                    break;
                }
            }

            if (Input & I_OPTIONS) // options menu
            {
                SaveTimer();
                MemoClipWindow();
                UnsetClipWindow();

                StopSpeak();
                InitDial(0); //      SYS

                InitPlasmaMenu();

                PauseMusic(FALSE);

                OptionsMenu(FALSE, MENU_ID_RETURN_TO_GAME);

                InitDial(START_FILE_ISLAND + Island);

                ClearPlasmaMenu();
                //                        CopyScreen( ScreenAux, Screen ) ;
                //                      RestoreClipWindow() ;// fait dans OptionsMenu
                //                        BoxStaticFullflip() ;
                PaletteSync(PtrPal);

                ResumeMusic(FALSE);

                RestoreTimer();
            }

            if (Input & I_SAVE) // Sauvegarde
            {
#if !defined(DEMO) || defined(DEBUG_TOOLS)
                if ((ptrobj->LifePoint > 0)
                        AND(ptrobj->Obj.Body.Num != -1)
                            AND(!(ptrobj->Flags & INVISIBLE))
                                AND(NumCube != NUM_CUBE_PHANTOM)
                                    AND !PLAY_THE_END) {
                    S32 savenewcube = NewCube;

                    InitDial(0);

                    SaveTimer();
                    MemoClipWindow();
                    UnsetClipWindow();
                    StopSpeak();
                    BackupScreen(TRUE);
                    HQ_PauseSamples();
#ifdef DEMO
                    SetDemoSaveGame(CurrentDemoSave);
                    NumVersion = NUM_VERSION | SAVE_COMPRESS;
                    CopyScreen(Screen, Log);
                    SaveGame(TRUE);
#else
                    InitPlasmaMenu();
                    if (ChoosePlayerName(42, FALSE, TRUE)) {
                        NumVersion = NUM_VERSION | SAVE_COMPRESS;
                        CopyScreen(Screen, Log);
                        if (FileSize(OldGamePathname))
                            Delete(OldGamePathname);
                        SaveGame(TRUE);
                        // Match the main-menu Save path: resume samples for the
                        // chime, then show the "Game Saved" confirmation. Without
                        // it the quick-save writes silently, so a successful save
                        // looks identical to a failure (menu just closes).
                        HQ_ResumeSamples();
                        ShowGameSavedConfirmation();
                    }
                    ClearPlasmaMenu();
#endif
                    RestoreClipWindow();
                    RestoreScreen();
                    NewCube = savenewcube;
                    InitDial(START_FILE_ISLAND + Island);
                    HQ_ResumeSamples();
                    PaletteSync(PtrPal);
                    RestoreTimer();

                    while (MyKey OR Input)
                        MyGetInput();
                } else
                    PlayErrorSample();
#else // DEMO
                PlayErrorSample();
#endif
            }

#ifndef DEMO
            if (Input & I_LOAD) // Chargement
            {
                S32 savenewcube = NewCube;

                InitDial(0);

                SaveTimer();
                MemoClipWindow();
                UnsetClipWindow();
                StopSpeak();
                BackupScreen(TRUE);
                HQ_PauseSamples();
                InitPlasmaMenu();
                if (ChoosePlayerName(41, FALSE, FALSE)) {
                    RestoreTimer();

                    StopMusic();

                    InitGame(-1);
                    ChangeCube();

                    CopyScreen(Log, ScreenAux);

                    RestoreTimer();
                    SaveTimer();
                    AffScene(AFF_ALL_NO_FLIP);
                    FixeCinemaMode(FALSE);
                    CopyScreen(Log, Screen);

                    // au cas ou le mec meurre avant de faire <ESC>
                    if (!Save_AutoDisabled())
                        CurrentSaveGame();

                    BoxReset(); // pas de flip

                    // In-game save zoom. Float to match the menu-driven sibling
                    // (GAMEMENU MainGameMenu): 80 = SCREEN_SAVE_WIDTH/2 so X tracks
                    // SAVE_THUMB_X, and Y floats with the rest of the UI. No-op at 640x480.
                    ZoomSavedGame((S32)ModeDesiredX / 2 - 80, 10 + UI_VCENTER_OFS);

                    FlagFade = FALSE;
                    RestartRainSample = TRUE;
                    HQ_ResumeSamples();

                    ClearPlasmaMenu();

                    RestoreTimer();

                    goto restartloop;
                } else {
                    // on charge pas de nouvelle partie
                    NewCube = savenewcube;
                    ClearPlasmaMenu();
                    RestoreClipWindow();
                    RestoreScreen();
                    InitDial(START_FILE_ISLAND + Island);
                    HQ_ResumeSamples();
                    PaletteSync(PtrPal);
                    RestoreTimer();

                    while (MyKey OR Input)
                        MyGetInput();
                }
            }
#endif

#ifdef DEBUG_TOOLS
            if ((MyKey == K_G) // Sauvegarde d'un bug
                AND(ptrobj->LifePoint > 0)
                    AND(ptrobj->Obj.Body.Num != -1)
                        AND(!(ptrobj->Flags & INVISIBLE))
                            AND(NumCube != NUM_CUBE_PHANTOM)) {
                S32 savenewcube = NewCube;

                InitDial(0);

                SaveTimer();
                MemoClipWindow();
                UnsetClipWindow();
                StopSpeak();
                BackupScreen(TRUE);
                HQ_PauseSamples();
                InitPlasmaMenu();
                if (ChoosePlayerName(42 | 0x40000000, FALSE, TRUE)) {
                    NumVersion = NUM_VERSION | SAVE_COMPRESS;
                    CopyScreen(Screen, Log);
                    SaveGame(TRUE);
                }
                ClearPlasmaMenu();
                RestoreScreen();
                NewCube = savenewcube;
                InitDial(START_FILE_ISLAND + Island);
                RestoreClipWindow();
                HQ_ResumeSamples();
                PaletteSync(PtrPal);
                RestoreTimer();

                while (MyKey OR Input)
                    MyGetInput();
            }
#endif

            if (Input & I_PAUSE) {
                SaveTimer();

                if (Comportement == C_PROTOPACK OR Comportement == C_JETPACK) {
                    // pour pouvoir restaurer anim marche protopack
                    if (ptrobj->GenAnim == GEN_ANIM_MARCHE) {
                        TimerProto = TimerRefHR + TEMPO_PROTO_AUTO; // 1/2 s
                    }
                }

                StopSpeak();
                HQ_PauseSamples();
                PauseMusic(FALSE);

                GamePaused(PAUSE_MESSAGE);

                ResumeMusic(FALSE);
                HQ_ResumeSamples();
                RestoreTimer();
            }

            InventoryAction = -1;

            // Console `useitem`: inject a one-frame inventory item-use (as MenuInventory would),
            // before the throttle gate, so it exercises the exact drop/deliver path a real use hits.
            // Optional delay lets the injection land past the load large-jump on a settled skip frame.
            if (DbgUseItemInject >= 0 && DbgUseItemInjectN > 0) {
                if (DbgUseItemInjectDelay > 0) {
                    DbgUseItemInjectDelay--;
                } else {
                    InventoryAction = DbgUseItemInject;
                    if (--DbgUseItemInjectN <= 0)
                        DbgUseItemInject = -1;
                }
            }

            if ((Input & I_INVENTORY)
                    AND(PtrComportement->Flags & CF_INVENTORY)
                        AND(ptrobj->Obj.Body.Num != -1)
                            AND !(ptrobj->WorkFlags & SKATING)
                                AND(ptrobj->Move == MOVE_MANUAL)) {
                SaveTimer();
                StopSpeak();
                BackupScreen(TRUE);
                MenuInventory(0);

                switch (InventoryAction) {
                case FLAG_HOLOMAP: // holomap
                    if (PtrComportement->Flags & CF_HOLOMAP) {
                        HoloMap();
                    }
                    break;

                case FLAG_BALLE_MAGIQUE: // balle magique
                case FLAG_DART:          // Flechette
                case FLAG_SARBACANE:     // Sarbacane
                case FLAG_GANT:          // Gant (raquette)
                case FLAG_PISTOLASER:    // PistoLaser
                    Weapon = (U8)InventoryAction;
                    if (Comportement > C_DISCRET) {
                        SetComportement(C_NORMAL);
                    }
                    InitBody(GEN_BODY_TUNIQUE, NUM_PERSO);
                    break;

                case FLAG_SABRE: // SABRE MAGIQUE
                    Weapon = (U8)InventoryAction;
                    if (Comportement > C_DISCRET) {
                        SetComportement(C_NORMAL);
                    }

                    // anim degaine sabre
                    InitBody(GEN_BODY_TUNIQUE, NUM_PERSO);
                    InitAnim(GEN_ANIM_DEGAINE, ANIM_THEN, NUM_PERSO);
                    break;

                case FLAG_PROTOPACK: // protopack
                {
                    U8 comp;

                    if (TabInv[FLAG_PROTOPACK].IdObj3D == 1)
                        comp = C_JETPACK;
                    else
                        comp = C_PROTOPACK;

                    if (Comportement == comp)
                        SetComportement(C_NORMAL);
                    else
                        SetComportement(comp);
                } break;

                case FLAG_CONQUE: // Conque
                    if (Comportement == C_CONQUE)
                        SetComportement(C_NORMAL);
                    else
                        SetComportement(C_CONQUE);
                    break;

                case FLAG_MECA_PINGOUIN: // meca pingouin
                    ThrowMecaPingouin();
                    break;

                case FLAG_ARDOISE:
                    GereArdoise();
                    break;

                case FLAG_VISIONNEUSE:
                    PlayAllAcf();

                    if (CubeMode == CUBE_INTERIEUR) {
                        InitGrille(NumCube);
                    }

                    RazListPartFlow();
                    ChoicePalette();
                    FirstTime = AFF_ALL_FLIP;
                    break;

                case FLAG_ANNEAU_FOUDRE:
                    if (Comportement > C_DISCRET) {
                        SetComportement(C_NORMAL);
                    }

                    InitBody(GEN_BODY_TUNIQUE, NUM_PERSO);
                    InitAnim(GEN_ANIM_FOUDRE, ANIM_ALL_THEN, NUM_PERSO);
                    break;

                case FLAG_PROTECTION:
                    ToggleSortProtection();
                    break;

                case FLAG_NOTE_BOURGMESTRE:
                    if (FirstTime != AFF_ALL_FLIP)
                        RestoreScreen();

                    // init incdisp symbol utilisation
                    InitIncrustDisp(INCRUST_INVENTORY | INCRUST_YCLIP,
                                    FLAG_NOTE_BOURGMESTRE,
                                    0, 0,
                                    0, 0, 3);

                    InitDial(2); // FR_GAM.TXT
#ifdef CDROM
                    NumObjSpeak = NUM_PERSO;
#endif
                    NumObjDial = NUM_PERSO;
                    Dial(800, TRUE);
                    InitDial(START_FILE_ISLAND + Island);
                    break;
                }
                if (FirstTime != AFF_ALL_FLIP)
                    RestoreScreen();
                RestoreTimer();
            }

            if ((Input & I_COMPORTEMENT)
                    AND(ptrobj->Obj.Body.Num != -1)
                        AND !(ptrobj->WorkFlags & SKATING)
                            AND((ptrobj->Move == MOVE_MANUAL)
                                    OR(ptrobj->Move == MOVE_BUGGY_MANUAL))) {
                MenuComportement(FALSE);
            }

            // changement d'arme ou de comportement
            if (PtrComportement->Flags & CF_INVENTORY
                                             AND(ptrobj->Obj.Body.Num != -1)
                                                 AND !(ptrobj->WorkFlags & SKATING)
                                                     AND(ptrobj->Move == MOVE_MANUAL)) {
                if (Input & I_WEAPON_1) // Balle magique
                {
                    if (ListVarGame[FLAG_BALLE_MAGIQUE]) {
                        Weapon = FLAG_BALLE_MAGIQUE;
                        if (Comportement > C_DISCRET) {
                            SetComportement(C_NORMAL);
                        }
                        InitBody(GEN_BODY_TUNIQUE, NUM_PERSO);
                    }
                    InitWaitNoInput(I_WEAPON_1);
                }

                if (Input & I_WEAPON_2) // Fléchettes
                {
                    if (ListVarGame[FLAG_DART]) {
                        Weapon = FLAG_DART;
                        if (Comportement > C_DISCRET) {
                            SetComportement(C_NORMAL);
                        }
                        InitBody(GEN_BODY_TUNIQUE, NUM_PERSO);
                    }
                    InitWaitNoInput(I_WEAPON_2);
                }

                if (Input & I_WEAPON_3) // SARBACANE/SARBATRON
                {
                    if (ListVarGame[FLAG_SARBACANE]) {
                        Weapon = FLAG_SARBACANE;
                        if (Comportement > C_DISCRET) {
                            SetComportement(C_NORMAL);
                        }
                        InitBody(GEN_BODY_TUNIQUE, NUM_PERSO);
                    }
                    InitWaitNoInput(I_WEAPON_3);
                }

                if (Input & I_WEAPON_4) // CONQUE
                {
                    if (ListVarGame[FLAG_CONQUE]) {
                        if (Comportement == C_CONQUE)
                            SetComportement(C_NORMAL);
                        else
                            SetComportement(C_CONQUE);
                    }
                    InitWaitNoInput(I_WEAPON_4);
                }

                if (Input & I_WEAPON_5) // GANT
                {
                    if (ListVarGame[FLAG_GANT]) {
                        Weapon = FLAG_GANT;
                        if (Comportement > C_DISCRET) {
                            SetComportement(C_NORMAL);
                        }
                        InitBody(GEN_BODY_TUNIQUE, NUM_PERSO);
                    }
                    InitWaitNoInput(I_WEAPON_5);
                }

                if (Input & I_WEAPON_6) // PISTOLASER
                {
                    if (ListVarGame[FLAG_PISTOLASER] AND TabInv[FLAG_PISTOLASER].IdObj3D == 2) {
                        Weapon = FLAG_PISTOLASER;
                        if (Comportement > C_DISCRET) {
                            SetComportement(C_NORMAL);
                        }
                        InitBody(GEN_BODY_TUNIQUE, NUM_PERSO);
                    }
                    InitWaitNoInput(I_WEAPON_6);
                }

                if (Input & I_WEAPON_7) // SABRE
                {
                    if (ListVarGame[FLAG_SABRE]) {
                        S32 oldweapon = Weapon;

                        Weapon = FLAG_SABRE;

                        if (Comportement > C_DISCRET) {
                            SetComportement(C_NORMAL);
                        }

                        // anim degaine sabre
                        InitBody(GEN_BODY_TUNIQUE, NUM_PERSO);

                        if (oldweapon != FLAG_SABRE) {
                            InitAnim(GEN_ANIM_DEGAINE, ANIM_THEN, NUM_PERSO);
                        }
                    }
                    InitWaitNoInput(I_WEAPON_7);
                }

                if (SpellKeyDown(33)) { // ProtoPack/SuperJetPack
                    if (!WaitNoKey33
                            AND ListVarGame[FLAG_PROTOPACK]) {
                        U8 comp;

                        if (TabInv[FLAG_PROTOPACK].IdObj3D == 1)
                            comp = C_JETPACK;
                        else
                            comp = C_PROTOPACK;

                        if (Comportement == comp)
                            SetComportement(C_NORMAL);
                        else
                            SetComportement(comp);
                    }

                    WaitNoKey33 = TRUE;
                } else
                    WaitNoKey33 = FALSE;

                if (SpellKeyDown(32)) { // NitroMecaPingouin
                    if (!WaitNoKey32
                            AND ListVarGame[FLAG_MECA_PINGOUIN] > 0) {
                        SaveTimer();
                        InventoryAction = FLAG_MECA_PINGOUIN;
                        ThrowMecaPingouin();
                        RestoreTimer();
                    }

                    WaitNoKey32 = TRUE;
                } else
                    WaitNoKey32 = FALSE;

                if (SpellKeyDown(34)) { // Sort de Protection
                    if (!WaitNoKey34
                            AND ListVarGame[FLAG_PROTECTION] > 0 AND MagicPoint > 0) {
                        SaveTimer();
                        InventoryAction = FLAG_PROTECTION;
                        ToggleSortProtection();
                        RestoreTimer();
                    }

                    WaitNoKey34 = TRUE;
                } else
                    WaitNoKey34 = FALSE;

                if (SpellKeyDown(35)) { // Sort de Foudre
                    if (!WaitNoKey35
                            AND ListVarGame[FLAG_ANNEAU_FOUDRE] > 0 AND MagicPoint >= (MagicLevel * 20)) {
                        SaveTimer();
                        InventoryAction = FLAG_ANNEAU_FOUDRE;
                        if (Comportement > C_DISCRET) {
                            SetComportement(C_NORMAL);
                        }

                        InitBody(GEN_BODY_TUNIQUE, NUM_PERSO);
                        InitAnim(GEN_ANIM_FOUDRE, ANIM_ALL_THEN, NUM_PERSO);
                        RestoreTimer();
                    }

                    WaitNoKey35 = TRUE;
                } else
                    WaitNoKey35 = FALSE;
            }

            {
                const S32 cameraSwitchHeld = CameraSwitchKeyDown();
                /* Interiors have no first-person camera; the switch there only
                   cycles settings the room cannot show.  Still consume a held
                   press so it cannot fire on the frame the player steps out. */
                if (CubeMode == CUBE_EXTERIEUR && cameraSwitchHeld) {
                    if (!WaitNoKey36) {
                        CycleCameraSetting();
                    }
                    WaitNoKey36 = TRUE;
                } else {
                    WaitNoKey36 = (U8)(cameraSwitchHeld ? TRUE : FALSE);
                }
            }

            if ((Input & (I_NORMAL | I_SPORTIF | I_AGRESSIF | I_DISCRET))
                    AND(PtrComportement->Flags & CF_COMPORTEMENT)
                        AND(ptrobj->Obj.Body.Num != -1)
                            AND !(ptrobj->WorkFlags & SKATING)
                                AND(ptrobj->Move == MOVE_MANUAL)) {
                switch (Comportement) {
                case C_NORMAL:
                case C_SPORTIF:
                case C_AGRESSIF:
                case C_DISCRET:
                case C_PROTOPACK:
                case C_CONQUE:
                case C_JETPACK:
                    SaveTimer();

                    if (Input & I_NORMAL)
                        SetComportement(C_NORMAL);
                    else if (Input & I_SPORTIF)
                        SetComportement(C_SPORTIF);
                    else if (Input & I_AGRESSIF)
                        SetComportement(C_AGRESSIF);
                    else if (Input & I_DISCRET)
                        SetComportement(C_DISCRET);

                    IncrustComportement(Comportement);
                    RestoreTimer();
                    break;

                case C_SCAPH_INT_NORM:
                    if (Input & I_SPORTIF) {
                        SaveTimer();
                        SetComportement((U8)C_SCAPH_INT_SPOR);
                        IncrustComportement(Comportement);
                        RestoreTimer();
                    }
                    break;

                case C_SCAPH_INT_SPOR:
                    if (Input & I_NORMAL) {
                        SaveTimer();
                        SetComportement((U8)C_SCAPH_INT_NORM);
                        IncrustComportement(Comportement);
                        RestoreTimer();
                    }
                    break;

                case C_SCAPH_EXT_NORM:
                    if (Input & I_SPORTIF) {
                        SaveTimer();
                        SetComportement((U8)C_SCAPH_EXT_SPOR);
                        IncrustComportement(Comportement);
                        RestoreTimer();
                    }
                    break;

                case C_SCAPH_EXT_SPOR:
                    if (Input & I_NORMAL) {
                        SaveTimer();
                        SetComportement((U8)C_SCAPH_EXT_NORM);
                        IncrustComportement(Comportement);
                        RestoreTimer();
                    }
                    break;
                }
            }

            if ((Input & I_HOLOMAP)
                    AND(PtrComportement->Flags & (CF_HOLOMAP | CF_INVENTORY))
                        AND(ListVarGame[FLAG_HOLOMAP] == 1)
                            AND(ptrobj->Obj.Body.Num != -1)
                                AND(ptrobj->Move == MOVE_MANUAL OR ptrobj->Move == MOVE_BUGGY_MANUAL)
                                    AND !CinemaMode) {
                SaveTimer();
                StopSpeak();
                //                      PauseMusic( TRUE ) ;
                //			BackupScreen( FALSE ) ;
                InitWaitNoInput(I_HOLOMAP | I_MENUS | I_RETURN | I_ACTION);
                InitWaitNoKey();
                HoloMap();
                //                      ResumeMusic( TRUE ) ;
                //                        RestoreScreen() ;
                RestoreTimer();
            }

            if (Input & I_RETURN) /* recentre sur perso */
            {
                if (!CameraZone /* si pas camera forcée */
                        AND !CinemaMode) {
                    SaveTimer();

                    Rotate(0, DISTANCE_VISEE, ListObjet[NumObjFollow].Obj.Beta);

                    StartXCube = (ListObjet[NumObjFollow].Obj.X + X0) / SIZE_BRICK_XZ;
                    StartYCube = ((ListObjet[NumObjFollow].Obj.Y + SIZE_BRICK_Y) / SIZE_BRICK_Y);
                    StartZCube = (ListObjet[NumObjFollow].Obj.Z + Z0) / SIZE_BRICK_XZ;

                    CameraCenter(1);
                    FirstTime = AFF_ALL_FLIP;

                    /* Re-sync follow camera state so it continues
                     * smoothly from the reset position. */
                    if (FollowCamera) {
                        FollowCamBaseDist = FOLLOW_CAM_INITIAL_DIST;
                        FollowCamEffectiveDist = FollowCamBaseDist;
                        FollowCamTargetBeta = BetaCam;
                        FollowCamReengageDelay = 0;
                        FollowCamPrevBaseDist = FollowCamBaseDist;
                        FollowCamPrevAlphaCam = AlphaCam;
                        /* Drop the smoothed ground lift so the recenter starts
                         * from the clean reset position instead of gliding down
                         * from a stale terrain-occluder lift. */
                        FollowCamEyeLift = 0;
                        FollowCamGroundSettling = 0;
                    }

                    RestoreTimer();

#ifndef DEBUG_TOOL
                    // faut-il le laisser ?
                    InitWaitNoInput(I_RETURN);
#endif
                }
            }

#ifdef DEBUG_TOOLS
            if (MyKey == K_B) {
                if (Sample_Driver_Enabled) {
                    if ((SamplesEnable ^= 1) == 0) {
                        HQ_PauseSamples();
                        Message("Samples OFF", TRUE);
                    } else {
                        HQ_ResumeSamples();
                        Message("Samples ON", TRUE);
                    }
                }
            }
#endif

        } // if !flagfade

        /*-------------------------------------------------------------------------*/

        /* Fixed-timestep simulation (issue #358). When enabled (console: fixedtimestep),
           the simulation advances in fixed FixedTimestep-ms steps decoupled from the render
           rate, so movement/animation is frame-rate independent. `elapsed` is the game-time
           banked since the last simulated frame; we run `elapsed / FixedTimestep` sub-steps:
             - 0 steps (rendering faster than ~60 fps): skip the sim, re-present the settled
               scene, carry the time. High-frame-rate case (vsync off, high-refresh displays).
             - 1 step (~60 fps): the historical per-frame path.
             - N steps (below ~60 fps): sub-step so a long frame still advances the animation
               the correct amount instead of losing it to the keyframe overshoot-discard.
           Each sub-step drives TimerRefHR to its own value; the sub-step remainder carries via
           LastSimRefHR, and TimerRefHR is restored to the frame's value afterwards. Held input
           actions (jump, throw) would re-fire if their object-loop code ran every sub-step, so
           after the first sub-step only movement bits (I_JOY) are kept -- InitAnim is
           idempotent, so the walk anim does not reset. A game-clock rewind (a modal SaveTimer
           restore, a cube change) re-anchors and skips one frame. Off (0) is the historical
           per-frame path byte-for-byte, and at --fixed-dt 16 the on path is byte-identical too
           (elapsed == 16 -> exactly 1 step). See docs/MOVEMENT_FRAMERATE.md. */
        S32 nSimSteps = 1;
        S32 simStep = 0;
        U32 simBaseRef = LastSimRefHR;
        U32 simFinalRef = TimerRefHR;
        U32 simSavedInput = Input;
        U32 simSavedLastInput = LastInput; // restored after the sub-step loop (see below)
        if (FixedTimestep > 0) {
            // Plan this frame's sub-steps (count + sub-step clock anchor + the next-frame carry).
            // Timer_PlanSimSteps is the pure, unit-tested core (LIB386/SYSTEM/TIMER.CPP): it
            // delegates the normal-path count/carry to Timer_FixedStepCount and owns the rewind
            // and large-jump re-anchors. LastSimRefHR is advanced here (the carry), so the tail
            // below no longer touches it.
            U32 nextLastSim;
            nSimSteps = Timer_PlanSimSteps(TimerRefHR, LastSimRefHR, FixedTimestep,
                                           FIXED_TIMESTEP_MAX_STEPS, &simBaseRef, &nextLastSim);
            /* Force a real sim step on a would-skip frame when it carries a one-frame signal that a
               later, non-skipped frame cannot recover:

                 - InventoryAction: a quest item-use set by MenuInventory, cleared at the top of
                   every loop and consumed by LF_USE_INVENTORY in DoLife below. On high-refresh
                   displays most frames skip the sim, so without this the event is wiped before any
                   simulated frame reads it and a quest item-use (the ferry ticket, the Balsam
                   flower, the Wizard Diploma) silently fails (#358 regression, softlocks the Peddler).

                 - FirstTime == AFF_ALL_FLIP: a full-scene flip (scene load / CAM_FOLLOW). DoLife
                   runs this frame and reads FirstTime; AffScene(FirstTime) below then consumes it and
                   the tail resets FirstTime to AFF_OBJETS_FLIP. If the flip frame is skipped, AffScene
                   still runs and resets FirstTime, so the deferred DoLife reads the stale value a
                   frame later -- e.g. the demo behaviour menu (LM_COMPORTEMENT_HERO), suppressed
                   while FirstTime == AFF_ALL_FLIP, then leaks over scene setup. Forcing the flip
                   frame to step keeps DoLife and AffScene on the same frame.

                 - A hero action/fire input EDGE ((Input ^ LastInput) & I_ACTION_EDGE): melee,
                   weapon fire and dodge are edge-triggered by the object loop -- a new attack
                   starts on the press edge, weapon fire stops on the release edge -- but that loop
                   runs only on simulated frames while LastInput advances every rendered frame
                   (above). A press/release landing on a skipped frame is never seen, so the blowgun
                   keeps firing on its own and melee never starts while moving (#407). Movement
                   (I_JOY) is deliberately excluded: it is held state the throttle is correct to
                   skip, and forcing a step on every walk change would defeat the frame-rate fix.

               Promotes a would-skip frame to a single step; a no-op when a step already runs or
               nothing is pending, so the throttle and its --fixed-dt 16 byte-for-byte guarantee are
               otherwise untouched. */
            nSimSteps = Timer_ForceStepIfPending(nSimSteps,
                                                 (InventoryAction >= 0) || (FirstTime == AFF_ALL_FLIP) || (((Input ^ LastInput) & I_ACTION_EDGE) != 0),
                                                 TimerRefHR, FixedTimestep, &simBaseRef, &nextLastSim);
            LastSimRefHR = nextLastSim;
            if (nSimSteps <= 0)
                goto skip_simulation; // high-fps skip (remainder carries) or clock rewind (re-anchored)
        }

        /* Advance the harness input timeline by this frame's sim ticks (docs/INPUT_SIM_PLAN.md).
           Metered in sim ticks, not rendered frames, so a hold means the same across
           --fixed-timestep settings; a skipped frame goto's above and never reaches here, so it
           consumes no tick. nSimSteps is 1 when unthrottled. */
        Control_InputAdvance(nSimSteps);

    sim_step:
        if (FixedTimestep > 0) {
            TimerRefHR = simBaseRef + (U32)(simStep + 1) * (U32)FixedTimestep;
            if (simStep > 0) {
                /* Input is sampled once per rendered frame; a frame's sub-steps advance sim TIME, not
                   input. So every sub-step sees the frame's held input, and the press/release EDGE is
                   consumed once -- on the first sub-step, where LastInput is still the previous frame's
                   real sample. On later sub-steps we hold BOTH Input and LastInput at that sample so the
                   held bits read as steady state: no phantom press (Input == LastInput, so the object
                   loop's edge-guarded actions like the C_AGRESSIF MyRnd combo do not re-select) and no
                   phantom release (the held bits are still present, so the momentary-action reset paths
                   -- `if (LastInput & I_THROW) InitAnim(GEN_ANIM_RIEN)` and the LastMyFire idle reset in
                   OBJECT.CPP MOVE_MANUAL -- stay put instead of cancelling an in-flight throw/attack anim
                   before it reaches its action frame). The earlier `& I_JOY` mask dropped the held action
                   bits, which the loop read as a release and reset the anim every sub-step, so below
                   ~60 fps a held throw/attack never advanced to its throw/hit frame (#456). InitAnim is
                   idempotent and projectile spawns are animation-frame-gated, so re-presenting the held
                   input each sub-step does not re-fire. LastInput is restored after the loop.
                   At FixedTimestep == 0 or nSimSteps == 1 (>= ~60 fps) this branch never runs, so the
                   per-frame path is byte-for-byte unchanged. */
                Input = simSavedInput;
                LastInput = simSavedInput;
            }
        }

        /* vitesse de chute propor pour tous les objets */
        StepFalling = GetDeltaMove(&RealFalling);
        /* vitesse de decalage propor pour tous les objets */
        StepShifting = GetDeltaMove(&RealShifting);

        // doit-on repaner les samples always
        pansample = GetDeltaMove(&SampleAlwaysMove);

        /*-------------------------------------------------------------------------*/

        CameraZone = FALSE;

        GereAmbiance();
        CheckNextMusic();

        /*-------------------------------------------------------------------------*/
        /* gere les objets */

        // asm
        ptrobj = ListObjet;
        for (i = 0; i < NbObjets; i++, ptrobj++) {
            ptrobj->HitBy = 255;
        }

        GereExtras();
        AnimAllFlow(); // gere les flows de particules

        if (CubeMode == CUBE_EXTERIEUR) {
            if (Island == 0 AND TEMPETE_ACTIVE AND RainEnable)
                GereRain();

            DoAnimatedPolys();
        }

        ptrobj = ListObjet;
        for (i = 0; i < NbObjets; i++, ptrobj++) {
            if (ptrobj->WorkFlags & OBJ_DEAD)
                continue;

            // test mort d'un objet

            if (ptrobj->LifePoint == 0) {
                switch (i) {
                case NUM_PERSO: // twinsen
                    if (ListVarGame[FLAG_CLOVER] > 0) {
                        UseOneClover();
                    } else {
                        InitAnim(GEN_ANIM_MORT, ANIM_FINAL, NUM_PERSO);
                        ptrobj->Move = NO_MOVE;
                    }
                    break;

                case NUM_PINGOUIN:
                    ExplodeMecaPingouin();
                    break;

                default:
                    // sample specifique mort ?
                    HQ_3D_MixSample(SAMPLE_DEAD, 0x1000, 2000, 1,
                                    ptrobj->Obj.X, ptrobj->Obj.Y, ptrobj->Obj.Z);

                    // 'explosion' de l'objet ?
                    if ((ptrobj->OptionFlags & EXTRA_MASK)
                            AND !(ptrobj->OptionFlags & EXTRA_GIVE_NOTHING)) {
                        GiveExtraBonus(ptrobj);
                    }
                }
            }

            // Save Beta pour les objets carrier
            // peut-etre pourrait-on éviter le test
            if (ptrobj->CarryBy == -1)
                ptrobj->OldBeta = ptrobj->Obj.Beta;

            DoDir(i);

            ptrobj->OldPosX = ptrobj->Obj.X;
            ptrobj->OldPosY = ptrobj->Obj.Y;
            ptrobj->OldPosZ = ptrobj->Obj.Z;

            if (ptrobj->OffsetTrack != -1) {
                DoTrack(i);
            }

            if (!FirstLoop)
                PtrDoAnim(i);

            if (ptrobj->Flags & CHECK_ZONE) {
                CheckZoneSce(ptrobj, i);
            }

            // Samples Always
            if (pansample AND ptrobj->SampleAlways) {
                ParmSampleVolume = ptrobj->SampleVolume;

                if (!(ptrobj->SampleAlways & 0xFF000000)
                        OR !IsSamplePlaying(ptrobj->SampleAlways)) {
                    ptrobj->SampleAlways = HQ_3D_MixSample((ptrobj->SampleAlways >> 8) & 0xFFFF,
                                                           0x1000, 0, 0,
                                                           ptrobj->Obj.X, ptrobj->Obj.Y, ptrobj->Obj.Z);

                } else {
                    HQ_3D_ChangePanSample(ptrobj->SampleAlways,
                                          ptrobj->Obj.X, ptrobj->Obj.Y, ptrobj->Obj.Z);
                }
            }

            if (i == NUM_PERSO) {
                // Pour la prochaine boucle
                ptrobj->WorkFlags &= ~(FLY_JETPACK | CHECK_FALLING);

                CheckDartCol(ptrobj);

                if (ptrobj->Move == MOVE_BUGGY_MANUAL) {
                    // teste les zones giver pour le
                    // buggy au point devant le buggy
                    CheckBuggyZoneGiver(ptrobj);
                }

                // pour la demo
                if (DemoSlide)
                    ActionNormal = FALSE;
            }

            if (ptrobj->OffsetLife != -1) {
                DoLife(i);
            }

            if (FlagTheEnd != -1)
                return FlagTheEnd; // mmm violent

            // si aprés la vie on à toujours 0 point de vie
            // destruction definitive
            if (ptrobj->LifePoint <= 0) {
                if (i == NUM_PERSO) {
                    if (ptrobj->WorkFlags & ANIM_END) {
                        if (ListVarGame[FLAG_CLOVER] > 0) {
                            UseOneClover();
                            goto startloop;
                        } else // game over
                        {
                            if (FirstTime == AFF_ALL_FLIP) {
                                AffScene(AFF_ALL_FLIP);
                            }

                            GameOver();
                            return 2;
                        }
                    }
                } else // autre obj
                {
                    CheckCarrier(i);
                    ptrobj->WorkFlags |= OBJ_DEAD;
                    ptrobj->Obj.Body.Num = -1;
                    ptrobj->ZoneSce = -1;
                }
            }

            if (NewCube != -1) {
                goto startloop;
            }
        }

        /*-------------------------------------------------------------------------*/
        /* recentre sur hero (numobjfollow) */

        /* First person is exterior-only.  A scene change while it is active
           returns to the classic camera, recentred on the hero. */
#ifdef LBA2_OPENXR
        if (CameraFirstPerson && CubeMode == CUBE_INTERIEUR)
            CycleCameraSetting();
#endif

            /* The interior hover rig owns the isometric projection centre, and an
           authored camera zone only chooses the room framing, not that
           centre - so the head look runs in a zone too.  The stick pan/zoom
           inside it stays gated on the zone below. */
#ifdef LBA2_OPENXR
        if (!CinemaMode)
            UpdateInteriorCameraControl();
#endif

        if (!CameraZone OR(CameraFirstPerson AND CubeMode == CUBE_EXTERIEUR)) {
            /* First person owns the camera everywhere, camera zones
             * included: it re-applies the eye position every frame, so the
             * zone presets never take the view back while moving. */
            if (!CinemaMode) {
                // AlphaCam (touches '+' et '-' *)
                GereExtKeys(MyKey);

                if (Input & I_CAMERA) {
                    SaveTimer();
#ifdef LBA2_OPENXR
                    /* Stereo sessions: the button is a pure recenter — the
                       world-anchored look recaptures its anchor on the
                       current rig framing, putting Twinsen back in the
                       centre.  The classic 90-degree orbit would only spin
                       the base the anchored view reads against. */
                    if (DisplayStereo3D AND OpenXR_IsActive() AND
                            CubeMode == CUBE_EXTERIEUR) {
                        LookRecenterReq = 1;
                        FirstTime = AFF_ALL_FLIP;
                        InitWaitNoInput(I_CAMERA);
                        RestoreTimer();
                    } else
#endif
                    {
                        AddBetaCam = (AddBetaCam + 1024) & 4095;
                        /* FollowCamera: do not use CameraCenter(2) — it resets
                         * AlphaCam/VueDistance to VueCamera presets.  Recompute
                         * target offset and BetaCam like case 2, then apply only. */
                        if (FollowCamera AND CubeMode == CUBE_EXTERIEUR) {
                            T_OBJ_3D *ptr3d_cc = &ListObjet[NumObjFollow].Obj;
                            S32 newbeta_cc = AddBetaCam + ptr3d_cc->Beta;
                            newbeta_cc &= 4095;
                            Rotate(0, DISTANCE_VISEE, newbeta_cc);
                            VueOffsetX = ptr3d_cc->X + X0;
                            VueOffsetY = ptr3d_cc->Y;
                            VueOffsetZ = ptr3d_cc->Z + Z0;
                            StartXCube = VueOffsetX / SIZE_BRICK_XZ;
                            StartYCube = VueOffsetY / SIZE_BRICK_Y;
                            StartZCube = VueOffsetZ / SIZE_BRICK_XZ;
                            BetaCam = (2048 - newbeta_cc) & 4095;
                            FollowCamTargetBeta = BetaCam;
                            CameraCenter(3);
                        } else {
                            CameraCenter(2);
                        }
                        FirstTime = AFF_ALL_FLIP;
                        InitWaitNoInput(I_CAMERA);
                        RestoreTimer();
                    }
                }

                if (CameraFirstPerson AND CubeMode == CUBE_EXTERIEUR) {
                    UpdateFirstPersonCamera();
                    /* The eye moves with the hero and the head: incremental
                       frames would keep the stale terrain view. */
                    FirstTime = AFF_ALL_FLIP;
                } else if (FollowCamera AND CubeMode == CUBE_EXTERIEUR)
                    UpdateFollowCameraExt();
            }

            if (FirstTime == AFF_OBJETS_FLIP) {
                T_OBJET *ptrobj;

                ptrobj = &ListObjet[NumObjFollow];

                PtrProjectPoint(ptrobj->Obj.X,
                                ptrobj->Obj.Y,
                                ptrobj->Obj.Z);

                /* FollowCamera (exterior): the follow camera tracks the hero
                 * every frame — the classic off-screen snap (CameraCenter(0))
                 * would fight it and cause jarring resets near trees / decors.
                 *
                 * X margins track ClipWindowX* (the Y side already did) so the
                 * snap follows the actual visible region. No-op at 640x480
                 * (80/539 preserved); at wider widths it stops firing while
                 * the hero is still well inside the right half of the screen,
                 * which is most visible in iso scenes (iso centre shifts with
                 * ModeDesiredX, so hero screen-X tracked the static 539). */
                if ((Xp<(ClipWindowXMin + 80) OR Xp>(ClipWindowXMax - 100) OR Yp<(80 + ClipWindowYMin) OR Yp>(ClipWindowYMax - 50))
                        AND !((FollowCamera OR CameraFirstPerson)
                                  AND CubeMode == CUBE_EXTERIEUR)) {
                    SaveTimer();

                    if (CubeMode == CUBE_INTERIEUR) {
                        xm = (ptrobj->Obj.X) / SIZE_BRICK_XZ;
                        zm = (ptrobj->Obj.Z) / SIZE_BRICK_XZ;

                        StartXCube = xm + ((xm - StartXCube)) / 2;
                        StartZCube = zm + ((zm - StartZCube)) / 2;
                    } else {
                        Rotate(0, DISTANCE_VISEE, ptrobj->Obj.Beta);
                        xm = (ptrobj->Obj.X + X0) / SIZE_BRICK_XZ;
                        zm = (ptrobj->Obj.Z + Z0) / SIZE_BRICK_XZ;

                        StartXCube = xm;
                        StartZCube = zm;
                    }

                    ym = ptrobj->Obj.Y / SIZE_BRICK_Y;
                    StartYCube = ym;

                    if (StartXCube >= SIZE_CUBE_X)
                        StartXCube = SIZE_CUBE_X - 1;
                    if (StartZCube >= SIZE_CUBE_Z)
                        StartZCube = SIZE_CUBE_Z - 1;

                    if (FlagCameraForcee) {
                        FlagCameraForcee = FALSE;
                        AlphaCam = DefAlphaCam[VueCamera];
                        GammaCam = DEFAULT_GAMMA_CAM;
                        VueDistance = DefVueDistance[VueCamera];
                    }

                    CameraCenter(0);
                    FirstTime = AFF_ALL_FLIP;

                    RestoreTimer();
                }
            }
        }

        /*-------------------------------------------------------------------------*/

        if (FixedTimestep > 0 && ++simStep < nSimSteps)
            goto sim_step; // run the next fixed sub-step of this frame
        if (FixedTimestep > 0) {
            // Restore the frame's clock for rendering / the next frame and undo the per-sub-step
            // input hold. LastSimRefHR was already advanced by Timer_PlanSimSteps (the carry).
            TimerRefHR = simFinalRef;
            Input = simSavedInput;
            LastInput = simSavedLastInput; // undo the sub-step hold so the next frame's edge is real
        }

        /*-------------------------------------------------------------------------*/
        /* affiche tout */

    skip_simulation:
        Perftrace_PhaseEnd(PERFTRACE_PHASE_SIMULATION);
        OpenXR_PollEvents();
        Perftrace_PhaseBegin(PERFTRACE_PHASE_SCENE);
        Perftrace_PhaseBegin(PERFTRACE_PHASE_OPENXR);
        OpenXR_BeginFrame();
        Perftrace_PhaseEnd(PERFTRACE_PHASE_OPENXR);
        S32 renderFlip = FirstTime;
#ifdef USE_GPURENDERER
        /* The isometric world lives in the GL FBO, whose brick layer and
           object pass are only re-established on a full flip.  Incremental
           frames would leave the previous model pose in place until a menu
           or scene change forces a redraw. */
        if (GPURendererActive && CubeMode == CUBE_INTERIEUR)
            renderFlip = AFF_ALL_FLIP;
#endif
        /* AffScene presents the frame itself: its BoxBlit funnels through
         * PresentFrame -> GL_Present -> Window_GL_SwapWindow, which on the
         * Quest owns the OpenXR present (xrEndFrame + input pump). No explicit
         * present here — that would double-present. This matches the original
         * PC path where AffScene was the sole presenter. */
        PERSO_RenderStereo(renderFlip);
        Perftrace_PhaseEnd(PERFTRACE_PHASE_SCENE);

        if ((FirstLoop OR RestartMusic)
                AND !PLAY_THE_END) {
            // on attend une boucle scenarique au cas ou le
            // scenar aurait lancé une musique
            if (CubeJingle != 255) {
                PlayMusic(CubeJingle, FALSE);
            }

            RestartMusic = FALSE;
        }

        FirstTime = AFF_OBJETS_FLIP;
        FirstLoop = FALSE;

        CmptFrame++;

#ifdef DEBUG_TOOLS
        if (NbFramePerSecond < MinNbf)
            MinNbf = NbFramePerSecond;
        if (NbFramePerSecond > MaxNbf)
            MaxNbf = NbFramePerSecond;

        TotalNbf += NbFramePerSecond;
        NbNbf++;
#endif

        Perftrace_FrameEnd();
    }

    return 0;
}

/*══════════════════════════════════════════════════════════════════════════*
                      █▀▀▀▀ █▀▀▀█ ██▄ █ █▀▀▀▀  █    █▀▀▀▀
                      ██    ██  █ ██▀██ ██▀▀   ██   ██ ▀█
                      ▀▀▀▀▀ ▀▀▀▀▀ ▀▀  ▀ ▀▀     ▀▀   ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

/* Defaults for settings whose preferred value depends on the presentation
 * path. The cfg still wins: these only apply when the key is absent, so an
 * existing lba2.cfg keeps the player's choices. */
#ifdef LBA2_OPENXR
#define DEF_DISPLAY_TEXFILTER 4 /* xBR Lite */
#define DEF_DISPLAY_ANTIALIAS 2 /* MSAA x2 */
#define DEF_DISPLAY_SOFTSHADOWS TRUE
#define DEF_DISPLAY_VSYNC FALSE
#define DEF_DISPLAY_STEREO3D TRUE
#define DEF_VR_SUPERSAMPLE 2 /* 1.5x */
#define DEF_VR_INTERIOR_HOVER TRUE
#define DEF_DISPLAY_SUBPIXEL TRUE
#define DEF_VR_FULLBODY TRUE
#else
#define DEF_DISPLAY_TEXFILTER TRUE
#define DEF_DISPLAY_ANTIALIAS 0
#define DEF_DISPLAY_SOFTSHADOWS FALSE
#define DEF_DISPLAY_VSYNC TRUE
#define DEF_DISPLAY_STEREO3D FALSE
#define DEF_VR_SUPERSAMPLE 0
#define DEF_VR_INTERIOR_HOVER FALSE
#define DEF_DISPLAY_SUBPIXEL FALSE
#define DEF_VR_FULLBODY FALSE
#endif

void ReadConfigFile(void) {
    const char *ptr;

    ReadInputConfig();    // Lit Config Clavier dans LBA2.CFG
    ReadGamepadConfig();  // Lit Config Gamepad dans LBA2.CFG
    ReadVolumeSettings(); // Lit Volumes dans LBA2.CFG

    Version_US = DefFileBufferReadValue("Version_US");
    NumVersion = (U8)DefFileBufferReadValue("CompressSave");

    if (NumVersion)
        NumVersion = SAVE_COMPRESS;

    NumVersion |= NUM_VERSION;

    // LastSave
    ptr = DefFileBufferReadString("LastSave");
    if (ptr AND strcmp(ptr, ""))
        strcpy(PlayerName, ptr);

    Shadow = DefFileBufferReadValueDefault("Shadow", 3);
    AllCameras = DefFileBufferReadValueDefault("AllCameras", TRUE);
    /* User-facing name: "Auto camera" (menu/docs); config key FollowCamera; legacy AutoCameraCenter */
    FollowCamera = DefFileBufferReadValueDefault("FollowCamera",
                                                 DefFileBufferReadValueDefault("AutoCameraCenter", FALSE));

    if (FollowCamera < 0 OR FollowCamera > 1)
        FollowCamera = FALSE;

    CameraFirstPerson = DefFileBufferReadValueDefault("CameraFirstPerson", FALSE);

    if (CameraFirstPerson < 0 OR CameraFirstPerson > 1)
        CameraFirstPerson = FALSE;

    VrFullBody = DefFileBufferReadValueDefault("VrFullBody", DEF_VR_FULLBODY) ? TRUE : FALSE;
    VrWorldScale = DefFileBufferReadValueDefault("VrWorldScale", 100);
    if (VrWorldScale < 50)
        VrWorldScale = 50;
    else if (VrWorldScale > 200)
        VrWorldScale = 200;

    DisplayBrightness = DefFileBufferReadValueDefault("DisplayBrightness", 0);
    if (DisplayBrightness < -20)
        DisplayBrightness = -20;
    else if (DisplayBrightness > 20)
        DisplayBrightness = 20;

    DisplayContrast = DefFileBufferReadValueDefault("DisplayContrast", 0);
    if (DisplayContrast < -20)
        DisplayContrast = -20;
    else if (DisplayContrast > 20)
        DisplayContrast = 20;

    VrFovScale = DefFileBufferReadValueDefault("VrFovScale", 100);
    if (VrFovScale < 50)
        VrFovScale = 50;
    else if (VrFovScale > 120)
        VrFovScale = 120;

    VrSuperSample = DefFileBufferReadValueDefault("VrSuperSample", DEF_VR_SUPERSAMPLE);
    if (VrSuperSample < 0 OR VrSuperSample > 2)
        VrSuperSample = DEF_VR_SUPERSAMPLE;

    Log_Debug("VR config: fov=%d supersample=%d worldscale=%d fullbody=%d",
              (int)VrFovScale, (int)VrSuperSample, (int)VrWorldScale,
              (int)VrFullBody);

    VrInteriorHover =
        DefFileBufferReadValueDefault("VrInteriorHover", DEF_VR_INTERIOR_HOVER) ? TRUE : FALSE;

    VrPassthrough =
        DefFileBufferReadValueDefault("VrPassthrough", 0) ? TRUE : FALSE;

    DisplaySaturation = DefFileBufferReadValueDefault("DisplaySaturation", 0);
    if (DisplaySaturation < -20)
        DisplaySaturation = -20;
    else if (DisplaySaturation > 20)
        DisplaySaturation = 20;

    ReverseStereo = DefFileBufferReadValueDefault("ReverseStereo", FALSE);

    InverseStereoSample(ReverseStereo);

    DebugFps = DefFileBufferReadValueDefault("DebugFps", FALSE);

    FpsLimit = DefFileBufferReadValueDefault("FpsLimit", 0);
    if (FpsLimit < 0)
        FpsLimit = 0;
    Window_SetFrameLimit(FpsLimit);
    Log_Info("[Perf] FpsLimit=%d", FpsLimit);

    DetailLevel = DefFileBufferReadValueDefault("DetailLevel", MAX_DETAIL_LEVEL);

    if (DetailLevel < 0)
        DetailLevel = 0;
    if (DetailLevel > MAX_DETAIL_LEVEL)
        DetailLevel = MAX_DETAIL_LEVEL;

    VideoFullScreen = DefFileBufferReadValueDefault("FullScreen", TRUE);

    if (VideoFullScreen < 0 OR VideoFullScreen > 1)
        VideoFullScreen = TRUE;

    DisplayFullScreen = DefFileBufferReadValueDefault("DisplayFullScreen", FALSE);

    if (DisplayFullScreen < 0 OR DisplayFullScreen > 1)
        DisplayFullScreen = FALSE;

    SetWindowFullscreen(DisplayFullScreen != FALSE);

    DisplayVSync = DefFileBufferReadValueDefault("VSync", DEF_DISPLAY_VSYNC);

    if (DisplayVSync < 0 OR DisplayVSync > 1)
        DisplayVSync = DEF_DISPLAY_VSYNC;

    /* Renderer exists by now (SetWindowFullscreen above operates on the same
       window/renderer pair), so this applies the saved choice immediately.
       Unlike fullscreen there's no visible boot-flash to avoid, so no early
       Res_LoadBoot* pre-read is needed. */
    Window_SetVSync(DisplayVSync);

#ifdef USE_GPURENDERER
    GPURendererActive = DefFileBufferReadValueDefault("GPURenderer", FALSE);
    if (GPURendererActive < 0 OR GPURendererActive > 1)
        GPURendererActive = FALSE;

#if defined(LBA2_OPENXR) && defined(__ANDROID__)
    /* On the Quest the engine's software renderer writes to a CPU surface
      * (SDL_UpdateTexture / SDL_RenderPresent) that the OpenXR blit cannot
      * see — OpenXR_PresentMono reads from the GL FBO 0 of the EGL context
      * that the GL renderer draws into, so without the GL renderer every
      * menu / title / SaveGame thumbnail is invisible in the headset even
      * though the frame loop is running. Force the GL renderer on for
      * immersive builds; the user can still toggle it off at runtime via
      * the Renderer's menu option, and we will honour that. */
    if (!GPURendererActive) {
        Log_Info("Quest immersive mode: forcing GPURenderer on (config had it off)");
        GPURendererActive = TRUE;
    }

    /* Touch controllers have no D-pad: the stock gamepad layout puts menu
      * and hero up/down on the triggers, which is unusable there. Force the
      * left stick onto up/down (this also overrides any persisted cfg
      * bindings for those two entries; the remap screen still lets the
      * player change them again per session). GamepadKeys is indexed by the
      * log2 of the I_* bit mask, not the mask itself: slot 0 = I_UP (1<<0),
      * slot 1 = I_DOWN (1<<1). */
    GamepadKeys[0].Key1 = K_GAMEPAD_LSTICK_UP;
    GamepadKeys[0].Key2 = 0;
    GamepadKeys[1].Key1 = K_GAMEPAD_LSTICK_DOWN;
    GamepadKeys[1].Key2 = 0;

    /* The right stick orbits the camera only in the follow camera (the
      * classic camera turns 90 degrees per flick, which is disorienting in
      * a headset). Default immersive sessions to the follow camera; the
      * options menu still allows switching back per session. */
    FollowCamera = TRUE;
    InitInput();
#endif

    DisplayTexFilter = DefFileBufferReadValueDefault("DisplayTexFilter", DEF_DISPLAY_TEXFILTER);
    DisplayAntiAlias = DefFileBufferReadValueDefault("DisplayAntiAlias", DEF_DISPLAY_ANTIALIAS);
    if (DisplayAntiAlias < 0 OR DisplayAntiAlias > 3)
        DisplayAntiAlias = DEF_DISPLAY_ANTIALIAS;
    DisplayDither = DefFileBufferReadValueDefault("DisplayDither", FALSE);
    if (DisplayDither < 0 OR DisplayDither > 1)
        DisplayDither = FALSE;
    DisplaySoftShadows =
        DefFileBufferReadValueDefault("DisplaySoftShadows", DEF_DISPLAY_SOFTSHADOWS);
    if (DisplaySoftShadows < 0 OR DisplaySoftShadows > 1)
        DisplaySoftShadows = DEF_DISPLAY_SOFTSHADOWS;
    DisplayColorGrade =
        DefFileBufferReadValueDefault("DisplayColorGrade", 0);
    if (DisplayColorGrade < 0 OR DisplayColorGrade > 2)
        DisplayColorGrade = 0;
    DisplaySubPixel = DefFileBufferReadValueDefault("DisplaySubPixel", DEF_DISPLAY_SUBPIXEL);
    if (DisplaySubPixel < 0 OR DisplaySubPixel > 1)
        DisplaySubPixel = DEF_DISPLAY_SUBPIXEL;
    DisplaySharpen = DefFileBufferReadValueDefault("DisplaySharpen", 0);
    if (DisplaySharpen < 0 OR DisplaySharpen > 2)
        DisplaySharpen = 0;
    MovieUpscale = DefFileBufferReadValueDefault("MovieUpscale", 2);
    if (MovieUpscale < 0 OR MovieUpscale > 2)
        MovieUpscale = 2;
    MovieVrZoom = DefFileBufferReadValueDefault("MovieVrZoom", 70);
    if (MovieVrZoom == 85)
        MovieVrZoom = 70; /* migrate the previous default */
    if (MovieVrZoom < 50)
        MovieVrZoom = 50;
    else if (MovieVrZoom > 100)
        MovieVrZoom = 100;
#ifdef __ANDROID__
    if (DisplayAntiAlias == 3)
        DisplayAntiAlias = 2; /* MSAA×4 not offered on Android (GL_SetAntiAlias) */
#endif
    if (DisplayTexFilter < 0 OR DisplayTexFilter > 4)
        DisplayTexFilter = DEF_DISPLAY_TEXFILTER;

    DisplayStereo3D = DefFileBufferReadValueDefault("DisplayStereo3D", DEF_DISPLAY_STEREO3D);
    if (DisplayStereo3D < 0 OR DisplayStereo3D > 1)
        DisplayStereo3D = DEF_DISPLAY_STEREO3D;

    RenderQuality = DefFileBufferReadValueDefault("RenderQuality", 1);
    if (RenderQuality < 1 OR RenderQuality > 4)
        RenderQuality = 1;
    if (RenderQuality == 3)
        RenderQuality = 1; /* only 1, 2, 4 are valid */
#ifdef __ANDROID__
    /* SSAA multiplies the framebuffer per axis: quality 4 at a
      * 2240x1260 render size is a 34-megapixel framebuffer, which no
      * mobile GPU sustains. Cap headset sessions at native. */
    if (RenderQuality > 1) {
        Log_Info("Android: clamping RenderQuality %d -> 1", RenderQuality);
        RenderQuality = 1;
    }
#endif

#if defined(LBA2_OPENXR) && defined(__ANDROID__)
    /* On Quest, OpenXR must be initialized before any SDL window is created.
      * The activity's native window already has an EGL context; if we let
      * Window_RecreateWindowForGL create its own SDL window with SDL_WINDOW_OPENGL,
      * SDL_GL_CreateContext fails ("Could not initialize OpenGL / GLES library")
      * because the EGL context is already current. OpenXR uses that existing
      * context via XrGraphicsBindingOpenGLESAndroidKHR, so we init OpenXR here
      * and then skip the SDL window creation below. */
    OpenXR_Init();
    if (OpenXR_IsActive()) {
        /* TouchOverlay_Init ran before this (main orders it ahead of
          * InitProgram) and could not see the session yet. The Quest has
          * no touchscreen and the Touch controllers drive the gamepad,
          * so the on-screen buttons would only cover the picture. */
        TouchOverlay_SetActive(0);
        /* Init registered the overlay callback before the session existed;
            leaving it registered would force the CPU-converted ARGB present
            path every frame for an overlay that never draws. */
        SetOverlayPrePresentCallback(NULL);
        LogPrintf("TouchOverlay: OpenXR session active, overlay disabled "
                  "(Touch controllers)\n");
        OpenXR_SetStereoEnabled(DisplayStereo3D);
    }
#endif

    /* If the config wants GL, tear down the SW window that InitGraphics
        created and recreate it with GL. This mirrors the runtime toggle in
        GereDisplayMenu but runs once at boot. */
    if (GPURendererActive) {
#if defined(LBA2_OPENXR) && defined(__ANDROID__)
        if (OpenXR_IsActive()) {
            /* OpenXR bound the boot-time SW renderer's EGL context into the
              * session and the GL renderer adopts that same context, so the
              * GL window recreation below is skipped — creating a second GL
              * context alongside the already-current EGL one fails with
              * "Could not initialize OpenGL / GLES library". The SW window
              * and renderer stay alive as the context donor; OpenXR owns the
              * present. */
            Log_Info("Quest immersive mode: reusing boot SW window's EGL context (OpenXR owns the present)");
            /* Set the render resolution so OpenXR_PresentMono can sample the
              * default framebuffer correctly. */
            extern void Window_SetRenderResolution(U32 w, U32 h);
            Window_SetRenderResolution(ModeDesiredX, ModeDesiredY);
            if (!GL_Renderer_Init(ModeDesiredX, ModeDesiredY)) {
                GL_Renderer_Shutdown();
                GPURendererActive = FALSE;
            } else {
                Window_SetVSync(DisplayVSync);
                if (RenderQuality > 1) {
                    GL_SetRenderQuality(RenderQuality);
                }
                if (!GL_SetSuperSample(VrSuperSample)) {
                    Log_Warn("VrSuperSample %d unavailable; using native "
                             "render size",
                             VrSuperSample);
                    VrSuperSample = 0;
                }
            }
        } else
#endif
            if (Window_RecreateWindowForGL(ModeDesiredX, ModeDesiredY,
                                           DisplayFullScreen != FALSE)) {
            if (!GL_Renderer_Init(ModeDesiredX, ModeDesiredY)) {
                GL_Renderer_Shutdown();
                Window_RecreateWindowForSW(ModeDesiredX, ModeDesiredY,
                                           DisplayFullScreen != FALSE);
                GPURendererActive = FALSE;
            } else {
                /* Re-apply VSync on the new GL context — the earlier
                    Window_SetVSync call targeted the SW renderer which
                    was just destroyed. */
                Window_SetVSync(DisplayVSync);
                /* Apply render quality from config */
                if (RenderQuality > 1) {
                    GL_SetRenderQuality(RenderQuality);
                }
            }
        } else {
            GPURendererActive = FALSE;
        }
    }
#endif

    /* #358 fixed-timestep throttle. Default 16 ms (~60 Hz sim) so movement is frame-rate
       independent above 60 fps; 0 restores the historical per-frame simulation. Clamp to a
       sane range (a value near a frame time; 0 = off). See docs/MOVEMENT_FRAMERATE.md. */
    FixedTimestep = DefFileBufferReadValueDefault("FixedTimestep", 16);
    if (FixedTimestep < 0 OR FixedTimestep > 100)
        FixedTimestep = 16;

    /* Console toggle scancode (SDL scancode integer); invalid values fall back to F12 in console module. */
    Console_SetToggleScancode(DefFileBufferReadValueDefault("ConsoleToggleKey", K_F12));

    ptr = DefFileBufferReadStringDefault("FlagDisplayText", "ON");

    if (ptr AND !strcasecmp(ptr, "OFF"))
        FlagDisplayText = 0;
    else
        FlagDisplayText = 1; // default

    // version distributeur
    DistribVersion = DefFileBufferReadValueDefault("Version", UNKNOWN_VERSION);

    FlagMenuMouse = DefFileBufferReadValueDefault("MenuMouse", TRUE);
    if (FlagMenuMouse != 0)
        FlagMenuMouse = TRUE;
    else
        FlagMenuMouse = FALSE;

    FlagMouseCamera = DefFileBufferReadValueDefault("MouseCamera", TRUE);
    if (FlagMouseCamera != 0)
        FlagMouseCamera = TRUE;
    else
        FlagMouseCamera = FALSE;

    FlagMouseCameraDrag = DefFileBufferReadValueDefault("MouseCameraDrag", TRUE);
    if (FlagMouseCameraDrag != 0)
        FlagMouseCameraDrag = TRUE;
    else
        FlagMouseCameraDrag = FALSE;

    MouseCameraSensX = DefFileBufferReadValueDefault("MouseSensitivityX", MOUSE_CAM_SENS_DEFAULT);
    if (MouseCameraSensX < 1)
        MouseCameraSensX = 1;
    if (MouseCameraSensX > 10)
        MouseCameraSensX = 10;

    MouseCameraSensY = DefFileBufferReadValueDefault("MouseSensitivityY", MOUSE_CAM_SENS_DEFAULT);
    if (MouseCameraSensY < 1)
        MouseCameraSensY = 1;
    if (MouseCameraSensY > 10)
        MouseCameraSensY = 10;

    MouseCameraInvertY = DefFileBufferReadValueDefault("MouseInvertY", FALSE);
    if (MouseCameraInvertY != 0)
        MouseCameraInvertY = TRUE;
    else
        MouseCameraInvertY = FALSE;

    MouseCameraDivisor = DefFileBufferReadValueDefault("MouseCameraDivisor", MOUSE_CAM_DIVISOR_DEFAULT);
    if (MouseCameraDivisor < 1)
        MouseCameraDivisor = 1;
    if (MouseCameraDivisor > 16)
        MouseCameraDivisor = 16;

    ManualCameraSmoothing = DefFileBufferReadValueDefault("ManualCameraSmoothing", MOUSE_CAM_SMOOTHING_DEFAULT);
    if (ManualCameraSmoothing < 1)
        ManualCameraSmoothing = 1;
    if (ManualCameraSmoothing > 16)
        ManualCameraSmoothing = 16;

    FollowCamReengageFrames = DefFileBufferReadValueDefault("FollowCamReengageFrames", FOLLOW_CAM_REENGAGE_FRAMES_DEFAULT);
    if (FollowCamReengageFrames < 0)
        FollowCamReengageFrames = 0;
    if (FollowCamReengageFrames > 600)
        FollowCamReengageFrames = 600;

    FollowCamDriftDivisor = DefFileBufferReadValueDefault("FollowCamDriftDivisor", FOLLOW_CAM_DRIFT_DIVISOR_DEFAULT);
    if (FollowCamDriftDivisor < 1)
        FollowCamDriftDivisor = 1;
    if (FollowCamDriftDivisor > 200)
        FollowCamDriftDivisor = 200;

    /* Auto-camera HD recompose + ground-clearance + manual-override tuning. */
    FollowCamHDRecompose = DefFileBufferReadValueDefault("FollowCamHDRecompose", FOLLOW_CAM_HD_RECOMPOSE_DEFAULT) ? TRUE : FALSE;
    FollowCamHDExcessCap = DefFileBufferReadValueDefault("FollowCamHDExcessCap", FOLLOW_CAM_HD_EXCESS_CAP_DEFAULT);
    if (FollowCamHDExcessCap < 0)
        FollowCamHDExcessCap = 0;
    FollowCamHDPitchGain = DefFileBufferReadValueDefault("FollowCamHDPitchGain", FOLLOW_CAM_HD_PITCH_GAIN_DEFAULT);
    FollowCamHDDistGain = DefFileBufferReadValueDefault("FollowCamHDDistGain", FOLLOW_CAM_HD_DIST_GAIN_DEFAULT);
    FollowCamHDLeanGain = DefFileBufferReadValueDefault("FollowCamHDLeanGain", FOLLOW_CAM_HD_LEAN_GAIN_DEFAULT);
    FollowCamGround = DefFileBufferReadValueDefault("FollowCamGround", FOLLOW_CAM_GROUND_DEFAULT) ? TRUE : FALSE;
    FollowCamGroundClearance = DefFileBufferReadValueDefault("FollowCamGroundClearance", FOLLOW_CAM_GROUND_CLEARANCE_DEFAULT);
    if (FollowCamGroundClearance < 0)
        FollowCamGroundClearance = 0;
    if (FollowCamGroundClearance > 20000) /* keep (g+clear-hy)*100 well inside S32 in the occlusion march */
        FollowCamGroundClearance = 20000;
    FollowCamManualHoldFrames = DefFileBufferReadValueDefault("FollowCamManualHoldFrames", FOLLOW_CAM_MANUAL_HOLD_DEFAULT);
    if (FollowCamManualHoldFrames < 0)
        FollowCamManualHoldFrames = 0;
    FollowCamHoldAngle = DefFileBufferReadValueDefault("FollowCamHoldAngle", FOLLOW_CAM_HOLD_ANGLE_DEFAULT) ? TRUE : FALSE;
}

/*──────────────────────────────────────────────────────────────────────────*/
void WriteConfigFile(void) {
    DefFileBufferInit(PathConfigFile, ScreenAux, (640 * 480 + RECOVER_AREA));

    // Batch the whole save into one fsync + atomic rename. Without this, each of
    // the ~30 writes below would fsync + rename on its own, which is noticeably
    // slow on a settings change. If a batch can't start, the writes fall back to
    // the per-call atomic path and still persist correctly.
    S32 batched = DefFileBufferBeginBatch();

    WriteInputConfig();    // Sauve Config Clavier dans LBA2.CFG
    WriteGamepadConfig();  // Sauve Config Gamepad dans LBA2.CFG
    WriteVolumeSettings(); // Sauve volumes dans LBA2.CFG

    DefFileBufferWriteString("Language", GetLanguageName(Language));
#ifdef CDROM
    DefFileBufferWriteString("LanguageCD", GetLanguageName(LanguageCD));
#endif

    DefFileBufferWriteString("LastSave", PlayerName);

    DefFileBufferWriteValue("Shadow", Shadow);
    DefFileBufferWriteValue("AllCameras", AllCameras);
    DefFileBufferWriteValue("FollowCamera", FollowCamera);
    DefFileBufferWriteValue("CameraFirstPerson", CameraFirstPerson);
    DefFileBufferWriteValue("VrFullBody", VrFullBody);
    DefFileBufferWriteValue("VrWorldScale", VrWorldScale);
    DefFileBufferWriteValue("VrFovScale", VrFovScale);
    DefFileBufferWriteValue("VrSuperSample", VrSuperSample);
    DefFileBufferWriteValue("VrInteriorHover", VrInteriorHover);
    DefFileBufferWriteValue("VrPassthrough", VrPassthrough);
    DefFileBufferWriteValue("DisplayBrightness", DisplayBrightness);
    DefFileBufferWriteValue("DisplayContrast", DisplayContrast);
    DefFileBufferWriteValue("DisplaySaturation", DisplaySaturation);
    DefFileBufferWriteValue("ReverseStereo", ReverseStereo);
    DefFileBufferWriteValue("DebugFps", DebugFps);
    DefFileBufferWriteValue("FpsLimit", FpsLimit);
    DefFileBufferWriteValue("DetailLevel", DetailLevel);
    DefFileBufferWriteValue("FullScreen", VideoFullScreen);
    DefFileBufferWriteValue("DisplayFullScreen", DisplayFullScreen);
    DefFileBufferWriteValue("VSync", DisplayVSync);
#ifdef USE_GPURENDERER
    DefFileBufferWriteValue("GPURenderer", GPURendererActive);
    DefFileBufferWriteValue("DisplayTexFilter", DisplayTexFilter);
    DefFileBufferWriteValue("DisplayAntiAlias", DisplayAntiAlias);
    DefFileBufferWriteValue("DisplayDither", DisplayDither);
    DefFileBufferWriteValue("DisplaySoftShadows", DisplaySoftShadows);
    DefFileBufferWriteValue("DisplayColorGrade", DisplayColorGrade);
    DefFileBufferWriteValue("DisplaySubPixel", DisplaySubPixel);
    DefFileBufferWriteValue("DisplaySharpen", DisplaySharpen);
    DefFileBufferWriteValue("MovieUpscale", MovieUpscale);
    DefFileBufferWriteValue("MovieVrZoom", MovieVrZoom);
    DefFileBufferWriteValue("DisplayStereo3D", DisplayStereo3D);
    DefFileBufferWriteValue("RenderQuality", RenderQuality);
#endif
    DefFileBufferWriteValue("FixedTimestep", FixedTimestep);
    DefFileBufferWriteValue("ConsoleToggleKey", Console_GetToggleScancode());
    /* Render resolution. Read at boot by Res_LoadBootDimensions (CLI > cfg >
       Quest native / auto-detect > compile-time default). Persist it only when
       the player chose it explicitly (CLI/cfg at boot, or a runtime
       Res_Switch); a derived resolution is left out so it re-derives each
       launch instead of freezing into the cfg on first exit. */
    if (!Res_ResolutionIsAutoDerived()) {
        DefFileBufferWriteValue("ResolutionX", (S32)ModeDesiredX);
        DefFileBufferWriteValue("ResolutionY", (S32)ModeDesiredY);
    }

    if (FlagDisplayText)
        DefFileBufferWriteString("FlagDisplayText", "ON");
    else
        DefFileBufferWriteString("FlagDisplayText", "OFF");

    DefFileBufferWriteValue("MenuMouse", FlagMenuMouse);
    DefFileBufferWriteValue("MouseCamera", FlagMouseCamera);
    DefFileBufferWriteValue("MouseCameraDrag", FlagMouseCameraDrag);
    DefFileBufferWriteValue("MouseSensitivityX", MouseCameraSensX);
    DefFileBufferWriteValue("MouseSensitivityY", MouseCameraSensY);
    DefFileBufferWriteValue("MouseInvertY", MouseCameraInvertY);
    DefFileBufferWriteValue("MouseCameraDivisor", MouseCameraDivisor);
    DefFileBufferWriteValue("ManualCameraSmoothing", ManualCameraSmoothing);
    DefFileBufferWriteValue("FollowCamReengageFrames", FollowCamReengageFrames);
    DefFileBufferWriteValue("FollowCamDriftDivisor", FollowCamDriftDivisor);
    DefFileBufferWriteValue("FollowCamHDRecompose", FollowCamHDRecompose);
    DefFileBufferWriteValue("FollowCamHDExcessCap", FollowCamHDExcessCap);
    DefFileBufferWriteValue("FollowCamHDPitchGain", FollowCamHDPitchGain);
    DefFileBufferWriteValue("FollowCamHDDistGain", FollowCamHDDistGain);
    DefFileBufferWriteValue("FollowCamHDLeanGain", FollowCamHDLeanGain);
    DefFileBufferWriteValue("FollowCamGround", FollowCamGround);
    DefFileBufferWriteValue("FollowCamGroundClearance", FollowCamGroundClearance);
    DefFileBufferWriteValue("FollowCamManualHoldFrames", FollowCamManualHoldFrames);
    DefFileBufferWriteValue("FollowCamHoldAngle", FollowCamHoldAngle);

    if (batched) {
        DefFileBufferEndBatch();
    }
}

/*══════════════════════════════════════════════════════════════════════════*
                            █▄ ▄█ █▀▀▀█  █    ██▄ █
                            ██▀ █ ██▀▀█  ██   ██▀██
                            ▀▀  ▀ ▀▀  ▀  ▀▀   ▀▀  ▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

static S32 End_Num = -1;
static const char *End_Error;

/*══════════════════════════════════════════════════════════════════════════*/

// Erreur dans le HQRGet() qui appel cette fonction: on sort violemment

HQR_GET_CALLBACK ErrorHQRGet;

void ErrorHQRGet(char *name, S32 index) {
    char errstring[256];

    snprintf(errstring, sizeof(errstring), "HQR_Get(%s): buffer trop petit (index %d)", name, index);

    TheEnd(NOT_ENOUGH_MEM, errstring);
}

/*══════════════════════════════════════════════════════════════════════════*/

void InitProgram() {
    BoxInit(MAX_BOXES);

    Cls();
    BoxUpdate();

    MemoClipWindow(); // pour caler MemoClipWindow... sur ClipWindow...

#ifdef TRACE
    AsciiMode = TRUE;
#endif

#ifdef CDROM
    InitVoiceFile();
#endif

    ReadConfigFile();
}

/*══════════════════════════════════════════════════════════════════════════*/
LBA_NORETURN void TheEndCheckFile(const char *filename) {
    if (FileSize(filename))
        End_Num = NOT_ENOUGH_MEM;
    else
        End_Num = ERROR_NOT_FOUND_FILE;

    End_Error = filename;

    /* Always an error path -> non-zero so launchers/scripts/Steam can tell.
       (The visible message for GUI launches is shown by TheEndInfo.) */
    exit(EXIT_FAILURE);
}

/*══════════════════════════════════════════════════════════════════════════*/
void InitTheEnd(S32 num, char *error) {
    End_Num = num;
    End_Error = error;
}

/*══════════════════════════════════════════════════════════════════════════*/
void TheEnd(S32 num, const char *error) {
    Log_Info("TheEnd called: num=%d error=%s", num, error);
    End_Num = num;
    End_Error = error;

    /* Clean exit only for PROGRAM_OK; every other End_Num is a failure. */
    exit(num == PROGRAM_OK ? 0 : EXIT_FAILURE);
}

/*══════════════════════════════════════════════════════════════════════════*/
#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
void DebugHQR(const char *titre, T_HQR_HEADER *hqr) {
    /* One call per line: titre is a %s argument (not the format), so it is
       safe and stays on the same line as its stats under the line-oriented
       log fan-out (see docs/LOGGING_UNIFICATION.md). */
    if (hqr) {
        LogPrintf("%s: %7ld - used: %7ld - MaxIndex: %4ld - NbIndex: %4ld\n", titre, hqr->MaxSize, hqr->MaxSize - hqr->FreeSize, hqr->MaxIndex, hqr->NbIndex);
    } else {
        LogPrintf("%s: xxxxxxx - used: xxxxxxx - MaxIndex: xxxx - NbIndex: xxxx\n", titre);
    }
}
#endif

/*══════════════════════════════════════════════════════════════════════════*/

// Trouver le moyen de cleaner cette @#&? de fonction !!!!!!

void V_ColorLine(U8 text, U8 back, U16 ncars) {
    text = text;
    back = back;
    ncars = ncars;
}

/*══════════════════════════════════════════════════════════════════════════*/
/* Native fatal-error dialog for terminal-less launches (double-clicked
   .app/.exe, Steam Deck), where the log/terminal is invisible. No-op on a dev
   terminal run (the log is right there), under the control harness (never block
   automation), or when no display is available (headless -> log only). Mirrors
   the no-game-data dialog in RES_PICKER. */
static void ShowFatalErrorDialog(const char *title, const char *message) {
    if (isatty(fileno(stderr)))
        return;
    if (Control_IsActive())
        return;
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO))
        return;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
}

/*══════════════════════════════════════════════════════════════════════════*/
/* Fatal exit for InitAdeline init failures -- see PERSO.H for why these can't
   go through TheEnd/TheEndInfo. Logs the reason, surfaces the native dialog on
   terminal-less launches, exits non-zero. No teardown: the failing subsystem
   never finished coming up, so there is nothing safe to tear down. */
LBA_NORETURN void BootFatal(const char *fmt, ...) {
    char reason[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(reason, sizeof reason, fmt, ap);
    va_end(ap);

    Log_Error("%s", reason);

    char logPath[ADELINE_MAX_PATH] = "";
    GetLogPath(logPath, ADELINE_MAX_PATH, LOG_NAME);
    char msg[ADELINE_MAX_PATH + 640];
    snprintf(msg, sizeof msg, "%s\n\nThe game can't start.\nDetails: %s", reason,
             logPath);
    ShowFatalErrorDialog("LBA2: cannot start", msg);

    exit(EXIT_FAILURE);
}

/*══════════════════════════════════════════════════════════════════════════*/
void TheEndInfo() {
    S32 err = FALSE;

#ifdef CDROM
    ClearVoiceFile();
#endif
    StopMusic();

    OpenXR_Shutdown();

    /* No version/build footer here — the boot header already stamps version,
       build, platform, and backends at the top of every log. */

#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
    LogPrintf("* Start Extended Memory was %ld\n", MemoMemory);
    LogPrintf("* Start Dos Memory was %ld\n", MemoDosMemory);
    LogPrintf("* Min Dos Memory was %ld\n", MemoMinDosMemory);
    LogPrintf("* Size PtrZBuffer: %ld\n", ListMem[MEM_PTR_ZBUFFER].Size);

    DebugHQR("* HQR Sprite     ", HQRPtrSprite);
    DebugHQR("      Sprite raw ", HQRPtrSpriteRaw);
    DebugHQR("    Anim 3DS     ", HQRPtrAnim3DS);
    DebugHQR("      Sample     ", HQR_Samples);
    DebugHQR("        Anim     ", HQR_Anims);
    DebugHQR("        Body     ", HQR_Bodys);
    DebugHQR("      ObjFix     ", HQR_ObjFix);
    DebugHQR("  HQR_Isle_Obj   ", HQR_Isle_Obj);
    DebugHQR("  HQR_CubeInfos  ", HQR_CubeInfos);
    DebugHQR("  HQR_ListDecors ", HQR_ListDecors);
    DebugHQR("  HQR_MapPGround ", HQR_MapPGround);
    DebugHQR("  HQR_ListTexDef ", HQR_ListTexDef);
    DebugHQR("  HQR_MapSommetY ", HQR_MapSommetY);
    DebugHQR("  HQR_MapIntens  ", HQR_MapIntensity);

    LogPrintf("* End Extended Memory was %ld\n", MemEnd);
    LogPrintf("* Total Extended Memory used: %ld\n", MemoMemory - MemEnd);

    LogPrintf("\nNbTexEffects= %d\n", NbTexEffects);
    if (PtrPolySea)
        LogPrintf("Animated Sea Enable.\n");
    else
        LogPrintf("Animated Sea Disable.\n");
#endif

    LogPrintf("\n");

    /* Exit-screen easter egg: only on a genuine clean shutdown. End_Num is
       PROGRAM_OK (2) only when TheEnd(PROGRAM_OK, ...) ran; crash/error paths
       set other codes and early exits leave it at -1, so this never fires on a
       non-clean exit. Separate module — does not touch the log; silent unless
       stdout is a real TTY. See docs/BOOT_LOG_PLAN.md Phase 6. */
    if (End_Num == PROGRAM_OK) {
        PrintExitScreen();
    }

    switch (End_Num) {
    case ERROR_NOT_FOUND_FILE:
        V_ColorLine(14, 0, 130);
        LogPrintf("*** ERROR-> File not found: %s\n\n", End_Error);
        V_ColorLine(7, 0, 130);
        break;

    case NAME_NOT_FOUND:
        V_ColorLine(14, 0, 130);
        LogPrintf("*** ERROR-> Critical error: ident name not found: %s\n\n", End_Error);
        V_ColorLine(7, 0, 130);
        break;

    case NOT_ENOUGH_MEM:
        V_ColorLine(14, 0, 130);
        LogPrintf("*** ERROR-> Not Enough Memory: %s (SEE README.TXT)\n\n", End_Error);
        V_ColorLine(7, 0, 130);
        break;

    case PROGRAM_OK:
#ifdef DEBUG_TOOLS
        if (NbNbf) {
            LogPrintf("* Frame speed status:\n");
            LogPrintf("  Maximal frame rate occured: %d\n", MaxNbf);
            LogPrintf("  Average frame rate was %d\n", TotalNbf / NbNbf);
            LogPrintf("%s\n", End_Error);
        }
#endif
        if (End_Error && End_Error[0])
            LogPrintf("%s\n", End_Error);
        LogPrintf("OK.\n");
        break;
    }

    /* Make fatal errors visible when there's no terminal to read the log. */
    if (End_Num == ERROR_NOT_FOUND_FILE || End_Num == NAME_NOT_FOUND ||
        End_Num == NOT_ENOUGH_MEM) {
        char logPath[ADELINE_MAX_PATH] = "";
        GetLogPath(logPath, ADELINE_MAX_PATH, LOG_NAME);
        const char *headline =
            (End_Num == NOT_ENOUGH_MEM)   ? "The game ran out of memory."
            : (End_Num == NAME_NOT_FOUND) ? "A required game object is missing."
                                          : "Game data is missing or unreadable.";
        char msg[ADELINE_MAX_PATH + 256];
        snprintf(msg, sizeof msg,
                 "%s\n\n%s\n\nThe game can't continue.\nDetails: %s", headline,
                 End_Error ? End_Error : "", logPath);
        ShowFatalErrorDialog("LBA2: cannot start", msg);
    }

    WriteConfigFile();

    Clear3dExt();
}

/*══════════════════════════════════════════════════════════════════════════*/

#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
void Message(const char *mess, S32 flag) {
    S32 x;

    CoulText(LBAWHITE, 0);
    PalOne(LBAWHITE, 255, 255, 255);
    x = (strlen(mess) * 8) / 2;
    MemoClip();

    GraphPrintf(FALSE, (S32)ModeDesiredX / 2 - x, 236, "%s", mess);
    Rect((S32)ModeDesiredX / 2 - x - 4, 234, (S32)ModeDesiredX / 2 + x + 4, 248, LBAWHITE);

    BoxStaticAdd((S32)ModeDesiredX / 2 - x - 4, 234, (S32)ModeDesiredX / 2 + x + 4, 248);

    if (flag) {
        BoxUpdate();
        WaitNoInput();
        WaitInput();
    }
    RestoreClip();
}
#else
void Message(const char *mess, S32 flag) {
    // pour virer warnings !!!! Baaahhhh ! Que j'aime pas ça, c'est pas
    // beau !!! mais ça marche et ça ne génère pas de code !!!

    mess = mess;
    flag = flag;
}
#endif

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/

/* Issue #62 host harness: --save-load-test <path> drives InitGame(-1) + ChangeCube()
 * and exits, so a driver script can run each save in a subprocess and capture
 * { exit_code, SAVE_LOAD_TEST stage lines }.  SaveLoadTestMode is declared in
 * SAVEGAME.H so OBJECT.CPP::ChangeCube can emit a stage line on flagload;
 * SaveLoadTestPath is file-local — only the parser-and-driver path here needs it. */
S32 SaveLoadTestMode = 0;
static char SaveLoadTestPath[ADELINE_MAX_PATH] = "";

static void parse_save_load_test_flag(int *argc, char *argv[]) {
    int read = 1, write = 1;
    while (read < *argc) {
        if (!strcmp(argv[read], "--save-load-test") && read + 1 < *argc) {
            strncpy(SaveLoadTestPath, argv[read + 1], sizeof(SaveLoadTestPath) - 1);
            SaveLoadTestPath[sizeof(SaveLoadTestPath) - 1] = '\0';
            SaveLoadTestMode = 1;
            read += 2;
            continue;
        }
        argv[write++] = argv[read++];
    }
    argv[write] = NULL;
    *argc = write;
}

int main(int argc, char *argv[]) {
    /* --version: print resolved engine version (from VERSION file at the
     * repo root, optionally suffixed with -dirty) and exit. Handled before
     * any other init so it works without retail data, without SDL, and on
     * stripped-down environments (CI, AppImage builders). The same string
     * is written to ${CMAKE_BINARY_DIR}/VERSION for release pipelines to
     * cat directly — see cmake/write_version.cmake. */
    {
        int i;
        for (i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--version") == 0) {
                printf("%s\n", LBA2_VERSION_STRING);
                return 0;
            }
        }
    }

    /* --help and unknown-flag rejection, for the same reason --version is here: no SDL,
       no retail data, no window. An unknown flag exits rather than booting, so a typo
       can't quietly leave the run on a default. */
    int wantsHelp = Cli_WantsHelp(argc, argv);
    if (wantsHelp) {
        Cli_PrintUsage(argv[0], wantsHelp == 2); /* --help-all prints the full surface */
        return 0;
    }
    if (Cli_ValidateArgs(argc, argv) != 0) {
        return 2;
    }

    S32 load = FALSE;
    U8 *ptrdos4gw;
    char tmpFilePath[ADELINE_MAX_PATH];

    parse_save_load_test_flag(&argc, argv);
    Control_ParseArgs(&argc, argv);

#ifdef DEBUG_TOOLS
    MemoMemory = AvailableMem();       // memory at start
    MemoDosMemory = DosAvailableMem(); // Dos memory at start
#endif

    /* SDL filesystem APIs (GetDefaultUserDir, ResolveGameDataDir) require SDL
     * to be initialized before use. */
    if (!SDL_Init(0)) {
        LogPrintf("SDL_Init failed: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    /* Boot stopwatch for the "ready in Xs" closer (logged once the engine is up,
       just before the menu/game). */
    const U32 bootStartMs = SDL_GetTicks();

    Android_EnsureExternalStoragePermission();

    {
        char resFolderPath[ADELINE_MAX_PATH] = "";
        char userFolderPath[ADELINE_MAX_PATH] = "";

        GetDefaultUserDir(userFolderPath);

        /* Bring the structured log up here — BEFORE game-data discovery and the
           folder picker — so their diagnostics reach adeline.log and the terminal,
           not just stderr. The "can't find game data" path is exactly when a
           persisted log matters most, and it used to be invisible there because
           CreateLog/Log_Init ran later, inside InitAdeline. The log file lives in
           the user dir (known now, independent of where the game data is). Use
           GetLogPathInDir rather than GetLogPath, which needs InitDirectories — and
           that needs the game dir we haven't resolved yet; it derives the same path
           from the user dir alone. The console-buffer sink joins later in
           InitAdeline: there's no F12 overlay this early. */
        char logFilePath[ADELINE_MAX_PATH] = "";
        GetLogPathInDir(logFilePath, ADELINE_MAX_PATH, userFolderPath, LOG_NAME);
        CreateLog(logFilePath);
        Log_Init();
        /* Master log level for every sink: --log-level wins, else LBA2_LOG_LEVEL,
           else INFO (debug off by default). The floor gates the file, terminal,
           and F12 console alike, so this one value controls all logging. */
        {
            LogSeverity logLevel = LOG_INFO;
            const char *envLevel;
            if (Control_HasLogLevel())
                logLevel = (LogSeverity)Control_GetLogLevel();
            else if ((envLevel = getenv("LBA2_LOG_LEVEL")) != NULL && *envLevel)
                (void)Log_ParseLevel(envLevel, &logLevel); /* invalid -> stays INFO */
            Log_SetLevel(logLevel);
        }
        Log_AddSink(Log_MakeFileSink(logFilePath, LOG_DEBUG));
        Log_AddSink(Log_MakeTerminalSink(LOG_DEBUG));
        atexit(Log_Shutdown);

        if (!ResolveGameDataDir(resFolderPath, ADELINE_MAX_PATH, &argc, argv)) {
            /* A batch or headless run must not reach the folder picker: it is modal, and
             * --headless still brings SDL up on the dummy video driver, so the dialog does
             * open and then waits forever for an answer nobody can give. Exit with the fix
             * in the message instead. */
            if (Control_IsActive() || Control_IsHeadless()) {
                Log_Error("Game data not found. Pass --game-dir <folder containing "
                          "LBA2.HQR> (often Common/), or set LBA2_GAME_DIR.");
                exit(EXIT_FAILURE);
            }

            /* Interactive last fallback: prompt with a folder dialog and persist the pick
             * for next launch. */
            if (!PromptForResDir(resFolderPath, ADELINE_MAX_PATH)) {
                /* Do not SDL_Quit() here: SDL3 can double-free in
                 * SDL_QuitFilesystem when shutting down after early
                 * filesystem API use; exiting is enough. */
                exit(EXIT_FAILURE);
            }
            WritePersistedGameDir(resFolderPath);
        }

        InitDirectories(userFolderPath, resFolderPath, userFolderPath, "",
                        DistribVersion);

        /* Mount a disc image in the install dir, if one is present, so assets
           that ship only inside the image (FMV, voices, music) resolve through
           the OpenRead seam with no extraction step. A pure no-op when there is
           no image (today's filesystem-only behaviour). Done before InitAdeline
           so the banner can show the mount, and before AssetPreflight so it
           sees disc-backed assets. */
        DiscImage_Mount(resFolderPath, Directories_GetResMarker());
    }

    HQRGetErrorFunc = ErrorHQRGet;

    /* Resolve the cfg path early so Res_LoadBootDimensions can read
       ResolutionX/Y BEFORE we size MainBuffer. InitAdeline re-resolves
       the same path later (GetCfgPath is deterministic / idempotent
       given directoriesCfgDir) and re-inits the cfg buffer against
       ScreenAux for the main ReadConfigFile pass. */
    GetCfgPath(PathConfigFile, ADELINE_MAX_PATH, CFG_NAME);

    /* Configure the framebuffer-sized ListMem entries with the effective resolution
       BEFORE InitMainBuffer() does the giant single-shot allocation. Precedence:
       --resolution CLI > lba2.cfg ResolutionX/Y > Quest native / auto-detect >
       compile-time default. The cfg read uses a transient stack/BSS buffer (no
       heap, no MainBuffer dependency), so we get the right MainBuffer size in a
       single allocation cycle. */
    /* Bring up the video subsystem now (idempotent, since InitProgram's later
       SDL_INIT_VIDEO init is guarded by SDL_WasInit) so Res_LoadBootDimensions
       can query the primary display for widescreen auto-detect BEFORE we size
       the framebuffer here. Without this the early resolve would have no display
       (windowed/640) while InitAdeline's later resolve (video up) would pick the
       wide size, and the two MUST agree or MainBuffer and Log/Screen mismatch. */
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        SDL_InitSubSystem(SDL_INIT_VIDEO);

    U32 reqResX, reqResY;
    Res_LoadBootDimensions(&reqResX, &reqResY);
    Mem_ConfigureScreenBuffers(reqResX, reqResY);
    InitMainBuffer(); // Alloue gros buffer pour tous les pointeurs

    End_Error = PathConfigFile;

    InitAdeline(argc, argv);

    /* Split the boot time: engine subsystems (InitAdeline — SDL, audio device,
       video) vs everything after (data loads + the intro logo splashes). */
    const U32 subsystemsMs = SDL_GetTicks() - bootStartMs;

    /* Console hooks: keyboard toggle/input interception + final-frame overlay compositing. */
    SetEventFilter(Console_FeedEvent);
    SetPrePresentCallback(Console_PrePresent);
    SetPrePresentActiveCallback(Console_WantsPrePresent);

    /* Touch virtual gamepad (Android / mobile). Init before Perftrace so
       timing callbacks nest correctly. No-op on non-Android builds. */
    TouchOverlay_Init();
    if (TouchOverlay_IsActive())
        SetOverlayPrePresentCallback(TouchOverlay_Render);

    /* OpenXR is initialized earlier (before GL window creation) on Quest.
       No-op when LBA2_OPENXR is off or no headset is present. */

    /* Perftrace: ring-buffer per-frame timing capture; off by default, opt-in
       via the `perftrace` console verb. See docs/PERFTRACE.md. */
    Perftrace_Init();
    SetPresentTimingCallbacks(Perftrace_PresentBegin, Perftrace_PresentEnd);

    atexit(TheEndInfo);

    End_Error = "";

    Log_Info("main: calling InitProgram");
    InitProgram();
#if defined(LBA2_OPENXR) && defined(__ANDROID__)
    /* ReadConfigFile (inside InitProgram) creates the OpenXR session, which is
       the first moment the headset's real per-eye recommendation is known. The
       boot resolve had to use the Quest 3 / 3S constant, so reconcile now, while
       the framebuffer is the only state that depends on it and before InitMemory
       and the boot log. A CLI / cfg / menu choice is left alone. */
    Res_ApplyHeadsetNative();
#endif
    Log_Info("main: calling InitMemory");
    InitMemory(); // Alloue petits buffers
    Log_Info("main: InitMemory done");

    /* "Display" status: WxH render resolution, where it came from (CLI / cfg /
       auto-detect / default), and the real SDL window mode. The mode comes from
       Window_IsFullscreen (the actual SDL flag state, not the cached request
       flag). Logged here in main with the rest of the post-init summary. */
    {
        U32 dispW = 0, dispH = 0;
        Res_LoadBootDimensions(&dispW, &dispH);
        Log_Info("Display    %ux%u (%s) %s", dispW, dispH,
                 Res_BootDimensionsSource(),
                 Control_IsHeadless()    ? "headless (no window)"
                 : Window_IsFullscreen() ? "fullscreen"
                                         : "windowed");
        Log_Info("main: AssetPreflight");
    }
    Log_Info("main: after display log");

    /* Fail fast on missing required data: AssetPreflight has logged each missing
       file; bail here (before any heavy load and before "Ready") via TheEnd so it
       flows through the same fatal surface as every other error — non-zero exit
       plus the native dialog in TheEndInfo for terminal-less launches. */
    if (AssetPreflight() > 0) {
        TheEnd(ERROR_NOT_FOUND_FILE, "required game data is missing (see the log)");
    }
    Log_Info("main: after AssetPreflight");

#ifdef ONE_GAME_DIRECTORY
    CreateLbaDirectories();
#endif

    //-------------------   buffer samples

    if (Sample_Driver_Enabled) {
        // déclare ressource samples buffer
        GetResPath(tmpFilePath, ADELINE_MAX_PATH, SAMPLES_HQR_NAME);
        HQR_Samples = HQR_Init_Ressource(
            tmpFilePath,
            SampleMem,
            SampleMem / 5000);

        SamplesEnable = TRUE;

        if (!HQR_Samples) {
            TheEndCheckFile(tmpFilePath);
        }

        // pour Windows
        HQR_Lock_Ressource(HQR_Samples);
    }

    //-------------------

    InitLanguage(); // multilangue
    /* --language <name> override: bypass the cfg's Language key for this run.
       Used by the test harness to pin a deterministic language regardless of
       whatever the local cfg says, and useful for any dev wanting to spot-
       check a language without editing their cfg. */
    if (Control_HasLanguage()) {
        Language = Control_GetLanguage();
        if (Control_IsVerbose())
            fprintf(stderr, "[control] --language: %s (index %d)\n",
                    GetLanguageName(Language), (int)Language);
    }
    Log_Info("Language   %s", GetLanguageName(Language));

    /* "Ready" counts engine subsystems coming up (InitAdeline: SDL, audio,
       video, window). The intro banners/logos that follow are a timed slideshow,
       deliberately not included. */
    Log_Raw("");
    Log_Banner("======== Ready in %.2fs ========", (double)subsystemsMs / 1000.0);

    Log_Info("main: after Ready banner");
    BufText = (U8 *)SmartMalloc(BIG_FILE_DIA);
    if (!BufText)
        TheEnd(NOT_ENOUGH_MEM, "BufText");
    Log_Info("main: BufText allocated");

    BufOrder = (U16 *)SmartMalloc(BIG_FILE_ORD);
    if (!BufOrder)
        TheEnd(NOT_ENOUGH_MEM, "BufOrder");
    Log_Info("main: BufOrder allocated");

    InitDial(0);
    Log_Info("main: InitDial done");
    GetMultiText(49, PleaseWait);
    GetMultiText(71, NewGameTxt);

    // chargement palettes ici car le InitDial(2) ou le AskForCD()
    // peuvent en avoir besoin...
    InitPalette();
    ChoicePalette();

#ifdef CDROM
#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
    InitCD("");
#else
    SetBlackPal(); // pour etre sur ...

    {
        char volname[10];
        S32 cd = TRUE;

        switch (DistribVersion) {
        case UNKNOWN_VERSION:
        case EA_VERSION:
            strcpy(volname, "LBA2");
            strcpy(MessageNoCD, MESSAGE_NO_CD);
            break;

        default:
            strcpy(volname, "TWINSEN");
            strcpy(MessageNoCD, MESSAGE_NO_CD_US);
            break;
        }

        do {
            // The CD loading screen and its palette fade are pure presentation, and
            // FadeToPal spins on a real wall-clock timer (FADE_DELAY ms). They make no
            // sense for the headless control harness / save-load test: no save has loaded
            // yet and the scene render overwrites the framebuffer anyway. Skip them there;
            // default gameplay keeps the loading screen. InitCD still runs (cheap; it sets
            // up the music track table).
            if (!SaveLoadTestMode && !Control_IsActive()) {
                // pour Windows car ca prend du temps a scaner le CD
                GetResPath(tmpFilePath, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
                Image_LoadCentered(tmpFilePath, Log, PCR_CDROM);
                BoxStaticFullflip();

                // premiere fois
                if (cd)
                    FadeToPal(PtrPalNormal);
            }

            if (!InitCD(volname)) {
                // premiere fois
                if (cd)
                    PlayErrorSample();

                if (!AskForCD()) {
                    TheEnd(PROGRAM_OK, MessageNoCD);
                }

                cd = FALSE;
            } else
                cd = TRUE;
        } while (!cd);
    }
    if (!SaveLoadTestMode && !Control_IsActive())
        FadeToBlack(PtrPalNormal);

#endif

    //-------------------   divers malloc
    BufMemoSeek = (U32 *)SmartMalloc(2048L);
    if (!BufMemoSeek)
        TheEnd(NOT_ENOUGH_MEM, "BufMemoSeek");
#endif

    InitJingle();

    BufferAnim = (U8 *)SmartMalloc(SIZE_BUFFER_ANIM);
    if (!BufferAnim)
        TheEnd(NOT_ENOUGH_MEM, "Buffer Anim");
    //-------------------

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
    LbaFont = (U8 *)LoadMalloc_HQR(tmpFilePath, RESS_FONT_GPM);
    if (!LbaFont)
        TheEndCheckFile(tmpFilePath);

    SetFont(LbaFont, 2, 8);
    ColorFont(14);

    InitAcf();

#ifndef DEBUG_TOOLS
    //-------------------   logo adeline (affiche)

    if (!SaveLoadTestMode && !Control_IsActive()) {
        DistribLogo();

        Cls();
        BoxUpdate();

        if (!IsFirstGameLaunched()) {
            AdelineLogo();

            Cls();
            BoxUpdate();
        }

#ifdef DEMO
        DemoBumper();

        Cls();
        BoxUpdate();
#endif
    } else {
        Cls();
        BoxUpdate();
    }
#else
    Cls();
#endif

    InitDial(2); // on s'assure que FR_GAM.VOX soit sur le Disque Dur

    Init3DExt();

    // a Faire Imperativement avant LoadFicPerso() !!!!!!!!
    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
    if (!Load_HQR(tmpFilePath, BufferFile3D, RESS_FILE3D))
        TheEndCheckFile(tmpFilePath);

    LoadFicPerso();
    InitObjects(BufferAnim, SIZE_BUFFER_ANIM, GivePtrBody, GivePtrAnim);

    //-------------------   load ressources diverses

    // Textures inventaire, médaillon, etc... (256x256)
    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
    BufferTexture = (U8 *)LoadMalloc_HQR(tmpFilePath, RESS_TEXTURES);
    if (!BufferTexture)
        TheEndCheckFile(tmpFilePath);

    ScanTextureAnimation(BufferTexture, 256);
    InitTextureAnimation(); // pour initialiser l'affaire

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
    PtrZvExtra = (S16 *)LoadMalloc_HQR(tmpFilePath, RESS_GOODIES_GPC);
    if (!PtrZvExtra)
        TheEndCheckFile(tmpFilePath);

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
    PtrZvExtraRaw = (S16 *)LoadMalloc_HQR(tmpFilePath, RESS_GOODRAW_GPC);
    if (!PtrZvExtraRaw)
        TheEndCheckFile(tmpFilePath);

#ifndef DEMO
    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
    PtrZvAnim3DS = (S16 *)LoadMalloc_HQR(tmpFilePath, RESS_ANIM3DS_GPC);
    if (!PtrZvAnim3DS)
        TheEndCheckFile(tmpFilePath);
#endif

    //-------------------   buffer sprites

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, SPRITES_HQR_NAME);
    HQRPtrSprite = HQR_Init_Ressource(
        tmpFilePath,
        SpriteMem,
        SpriteMem / 1000);

    if (!HQRPtrSprite)
        TheEndCheckFile(tmpFilePath);

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, SPRIRAW_HQR_NAME);
    HQRPtrSpriteRaw = HQR_Init_Ressource(
        tmpFilePath,
        SpriteRawMem,
        SpriteRawMem / 100);

    if (!HQRPtrSpriteRaw)
        TheEndCheckFile(tmpFilePath);

//-------------------   Anims 3DS
#ifndef DEMO
    GetResPath(tmpFilePath, ADELINE_MAX_PATH, ANIM3DS_HQR_NAME);
    HQRPtrAnim3DS = HQR_Init_Ressource(
        tmpFilePath,
        Anim3DSMem,
        100);

    if (!HQRPtrAnim3DS)
        TheEndCheckFile(tmpFilePath);

    LoadListAnim3DS();
#endif
    //-------------------   Pof
    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
    Load_HQR(tmpFilePath, BufferPof, RESS_POF);

    //-------------------   buffer Impacts
    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
    Load_HQR(tmpFilePath, BufferImpact, RESS_IMPACT);

    //-------------------   buffer animations

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, ANIM_HQR_NAME);
    HQR_Anims = HQR_Init_Ressource(
        tmpFilePath,
        AnimMem,
        AnimMem / 800);

    if (!HQR_Anims)
        TheEndCheckFile(tmpFilePath);

    //-------------------   buffer bodys

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, BODY_HQR_NAME);
    HQR_Bodys = HQR_Init_Ressource(
        tmpFilePath,
        BodyMem,
        BodyMem / 1000);

    if (!HQR_Bodys)
        TheEndCheckFile(tmpFilePath);

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, OBJFIX_HQR_NAME);
    HQR_ObjFix = HQR_Init_Ressource(
        tmpFilePath,
        ObjFixMem,
        ObjFixMem / 1000);

    if (!HQR_ObjFix)
        TheEndCheckFile(tmpFilePath);

    /* (No per-type asset counts here: the ressource HQRs load lazily and only
       expose a cache-slot budget, not the file's real entry count. The asset
       preflight above reports presence, which is the verification that matters.
       Real entry counts would need an HQR-header reader — a future addition.) */

    //-------------------   buffer flows de particules

    InitPartFlow();
    InitDarts();
    InitRain();
    InitSysText();

    // doit etre la derniere allocation faite
    PtrPolySea = (S_ANI_POLY *)Malloc(SIZE_BUFFER_POLY_SEA);
    if (!PtrPolySea)
        MaxPolySea = 0; // Animation de l'eau non dispo

#ifdef DEBUG_TOOLS
    MemoMinDosMemory = DosAvailableMem(); // Dos memory after inits
    MemEnd = AvailableMem();              // Extend memory at the end
#endif

#ifndef DEBUG_TOOLS
    // Pour virer warning !!!!!!
    ArgC = ArgC;
#else
    // temporaire pour virer Warning
    ArgC = ArgC;

    Palette(PtrPalBlack);
#endif

    //-------------------   main game menu

    FadeToBlack(PalettePcx);

    // Pour un SlideShow sur un salon, il ne faut pas que les p'tits gars
    // puissent jouer avec le clavier !!!!!
    /*      if( ArgC==2 AND !strcasecmp(ArgV[1],"/NOKEY") )
        {
                KeyOff = TRUE ;
        }
*/

#ifdef DEBUG_TOOLS
    // Command-line cube selection: ./lba2 <cube_number>
    if (ArgC > 1) {
        char *p = ArgV[1];
        S32 isNumber = TRUE;
        while (*p) {
            if (!isdigit((unsigned char)*p++)) {
                isNumber = FALSE;
                break;
            }
        }

        if (isNumber) {
            S32 cubeNum = atoi(ArgV[1]);
            LogPrintf("\nStarting at cube %d\n", cubeNum);

            InitGame(1);
            NewCube = cubeNum; // Set AFTER InitGame since it resets NewCube to 0
            FlagFade = TRUE;
            MainLoop();
            TheEnd(PROGRAM_OK, "");
            return 0;
        }
    }
#endif

    // Load save game from command line argument
    if (ArgC > 1) {
#ifdef DEMO
#ifdef DEBUG_TOOLS
        load = atoi(ArgV[1]);
#endif
#else
        LogPrintf("\nTry to load %s\n", ArgV[1]);

        if (FileSize(ArgV[1])) {
            strcpy(GamePathname, ArgV[1]);
            load = TRUE;
        } else {
            GetSavePath(GamePathname, ADELINE_MAX_PATH, ArgV[1]);

            if (FileSize(GamePathname)) {
                load = TRUE;
            } else {
                char tmpFileName[ADELINE_MAX_PATH];
                snprintf(tmpFileName, ADELINE_MAX_PATH, "%s.lba", ArgV[1]);
                GetSavePath(GamePathname, ADELINE_MAX_PATH, tmpFileName);

                if (FileSize(GamePathname)) {
                    load = TRUE;
                } else
                    strcpy(GamePathname, "");
            }
        }

        if (load) {
            Palette(PtrPal);
        }
#endif
    }

#ifdef DEBUG_TOOLS
    HERCULE(HCls());
#endif

    if (SaveLoadTestMode) {
        /* Issue #62 harness: drive the menu's "load_game" sequence on a fixed path,
         * print parseable lines for the driver script, and exit.  Mirrors
         * GAMEMENU.CPP load_game label: InitGame(-1) → ChangeCube(). */
        if (!FileSize(SaveLoadTestPath)) {
            LogPrintf("SAVE_LOAD_TEST: stage=open path=%s status=missing\n", SaveLoadTestPath);
            fprintf(stdout, "SAVE_LOAD_TEST: stage=open path=%s status=missing\n", SaveLoadTestPath);
            fflush(stdout);
            TheEnd(PROGRAM_OK, "");
            return 2;
        }
        strcpy(GamePathname, SaveLoadTestPath);
        fprintf(stdout, "SAVE_LOAD_TEST: stage=start path=%s\n", SaveLoadTestPath);
        fflush(stdout);
        InitGame(-1);
        fprintf(stdout, "SAVE_LOAD_TEST: stage=after_initgame newcube=%d\n", (int)NewCube);
        fflush(stdout);
        ChangeCube();
        /* ChangeCube prints SAVE_LOAD_TEST: stage=after_loadgame flagload=N when in test mode. */
        {
            extern S32 NbPatches;
            fprintf(stdout, "SAVE_LOAD_TEST: stage=done numcube=%d nb_objets=%d nb_patches=%d\n",
                    (int)NumCube, (int)NbObjets, (int)NbPatches);
        }
        fflush(stdout);
        TheEnd(PROGRAM_OK, "");
        return 0;
    }

    /* CLI control harness: when driven by --load/--exec/--tick/etc, bypass the menu
     * and run the automated boot path (load -> exec -> tick -> dump/screenshot ->
     * exit). The single Control_TickHook() in MainLoop does the per-tick work. */
    if (Control_IsActive()) {
        if (!Control_Begin()) {
            /* Setup failed (a --load that resolved to nothing, say). Exit non-zero and
               WITHOUT Control_End: it would write --dump-state / --screenshot from a game
               that never loaded, i.e. plausible artifacts of the wrong scene. */
            TheEnd(PROGRAM_FAIL, "control: setup failed");
            return 1;
        }
        MainLoop();
        Control_End();
        if (Control_UnfiredExecAt() > 0) {
            TheEnd(PROGRAM_FAIL, "control: --exec-at never ran");
            return 1;
        }
        TheEnd(PROGRAM_OK, "");
        return 0;
    }

    Log_Info("main: calling MainGameMenu");
    MainGameMenu(load);
    Log_Info("main: MainGameMenu returned");

#ifdef DEMO
    if (!FlagShowEndDemo) {
        SlideShow();
    }

    DemoBumper();
#endif

    TheEnd(PROGRAM_OK, "");

    return 0;
}

/*══════════════════════════════════════════════════════════════════════════*/
