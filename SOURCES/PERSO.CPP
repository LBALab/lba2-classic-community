#include "C_EXTERN.H"
#include "IMAGE_LOAD.H"

#include "BUILD_INFO.h" /* LBA2_VERSION_STRING (for --version flag) */
#include "INITADEL.H"
#include "DIRECTORIES.H"
#include "RES_DISCOVERY.H"
#include "RES_PICKER.H"
#include "INPUT.H"
#include "CONTROL.H"
#include "RES_SWITCH.H"
#include "CONSOLE/CONSOLE.H"
#include "EXIT_SCREEN.H"
#include "PERFTRACE.H"
#include "TOUCH_INPUT.H"
#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/LOGPRINT.H> /* CreateLog (boot log brought up in main) */
#include "ASSET_PREFLIGHT.H"
#include <SYSTEM/EVENTS.H>
#include <SVGA/VIDEO.H>
#include <FILEIO/SAVEPNG.H>

#include <SVGA/SCREEN.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include "FOLLOWCAM_CFG.H"
#include <SYSTEM/STRING.H>
#include <SYSTEM/WINDOW.H>

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
/* Default resting distance for the spring arm: midpoint of the two VueCamera
 * presets (DefVueDistance[0]=10500, DefVueDistance[1]=17000). */
#define FOLLOW_CAM_INITIAL_DIST ((DefVueDistance[0] + DefVueDistance[1]) / 2)
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
    S32 k100 = (S32)ModeDesiredY * 100 / 480;
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
    if (FollowCamBaseDist == 0)
        FollowCamBaseDist = FOLLOW_CAM_INITIAL_DIST;
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

/*──────────────────────────────────────────────────────────────────────────*/

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

        MyGetInput();

        // Console `input`: OR injected input bits into this frame's Input, so the harness can drive
        // the hero (walk/turn/act) without a keyboard. Held for DbgInjectInputFrames frames.
        if (DbgInjectInputFrames > 0) {
            Input |= (U32)DbgInjectInput;
            if (--DbgInjectInputFrames <= 0)
                DbgInjectInput = 0;
        }

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
                SavePNG(shotPath, Log, ModeDesiredX, ModeDesiredY, PtrPal);
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
                GameOptionMenu[5] = 15; // retour au jeu

                StopSpeak();
                InitDial(0); //      SYS

                InitPlasmaMenu();

                PauseMusic(FALSE);

                OptionsMenu(FALSE);

                GameOptionMenu[5] = 26; // retour au menu precedent

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

    sim_step:
        if (FixedTimestep > 0) {
            TimerRefHR = simBaseRef + (U32)(simStep + 1) * (U32)FixedTimestep;
            if (simStep > 0)
                Input = simSavedInput & I_JOY; // movement only after the first sub-step
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

        if (!CameraZone) {
            if (!CinemaMode) {
                // AlphaCam (touches '+' et '-' *)
                GereExtKeys(MyKey);

                if (Input & I_CAMERA) {
                    SaveTimer();
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

                if (FollowCamera AND CubeMode == CUBE_EXTERIEUR)
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
                        AND !(FollowCamera AND CubeMode == CUBE_EXTERIEUR)) {
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
            // input mask. LastSimRefHR was already advanced by Timer_PlanSimSteps (the carry).
            TimerRefHR = simFinalRef;
            Input = simSavedInput;
        }

        /*-------------------------------------------------------------------------*/
        /* affiche tout */

    skip_simulation:
        Perftrace_PhaseBegin(PERFTRACE_PHASE_SCENE);
        AffScene(FirstTime);
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

    ReverseStereo = DefFileBufferReadValueDefault("ReverseStereo", FALSE);

    InverseStereoSample(ReverseStereo);

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

    DisplayVSync = DefFileBufferReadValueDefault("VSync", TRUE);

    if (DisplayVSync < 0 OR DisplayVSync > 1)
        DisplayVSync = TRUE;

    /* Renderer exists by now (SetWindowFullscreen above operates on the same
       window/renderer pair), so this applies the saved choice immediately.
       Unlike fullscreen there's no visible boot-flash to avoid, so no early
       Res_LoadBoot* pre-read is needed. */
    Window_SetVSync(DisplayVSync);

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
    DefFileBufferWriteValue("ReverseStereo", ReverseStereo);
    DefFileBufferWriteValue("DetailLevel", DetailLevel);
    DefFileBufferWriteValue("FullScreen", VideoFullScreen);
    DefFileBufferWriteValue("DisplayFullScreen", DisplayFullScreen);
    DefFileBufferWriteValue("VSync", DisplayVSync);
    DefFileBufferWriteValue("FixedTimestep", FixedTimestep);
    DefFileBufferWriteValue("ConsoleToggleKey", Console_GetToggleScancode());
    /* Render resolution. Read at boot by Res_LoadBootDimensions (CLI > cfg >
       auto-detect > compile-time default). Persist it only when the player
       chose it explicitly (CLI/cfg at boot, or a runtime Res_Switch); an
       auto-detected or defaulted resolution is left out so it re-derives from
       the display each launch instead of freezing into the cfg on first exit. */
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
            /* Auto-discovery exhausted. Last fallback before exit: in a
             * windowed environment, prompt the user with a folder dialog
             * and persist their pick for next launch. Headless / no-display
             * environments fall through to exit (PromptForResDir returns
             * false when SDL_INIT_VIDEO can't be brought up). */
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
       --resolution CLI > lba2.cfg ResolutionX/Y > compile-time default. The cfg
       read uses a transient stack/BSS buffer (no heap, no MainBuffer dependency),
       so we get the right MainBuffer size in a single allocation cycle. */
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

    /* Touch virtual gamepad (Android / mobile). Init before Perftrace so
       timing callbacks nest correctly. No-op on non-Android builds. */
    TouchOverlay_Init();
    if (TouchOverlay_IsActive())
        SetOverlayPrePresentCallback(TouchOverlay_Render);

    /* Perftrace: ring-buffer per-frame timing capture; off by default, opt-in
       via the `perftrace` console verb. See docs/PERFTRACE.md. */
    Perftrace_Init();
    SetPresentTimingCallbacks(Perftrace_PresentBegin, Perftrace_PresentEnd);

    atexit(TheEndInfo);

    End_Error = "";

    InitProgram();
    InitMemory(); // Alloue petits buffers

    /* "Display" status: WxH render resolution, where it came from (CLI / cfg /
       auto-detect / default), and the real SDL window mode. The mode comes from
       Window_IsFullscreen (the actual SDL flag state, not the cached request
       flag). Logged here in main with the rest of the post-init summary. */
    {
        U32 dispW = 0, dispH = 0;
        Res_LoadBootDimensions(&dispW, &dispH);
        Log_Info("Display    %ux%u (%s) %s", dispW, dispH,
                 Res_BootDimensionsSource(),
                 Window_IsFullscreen() ? "fullscreen" : "windowed");
    }

    /* Fail fast on missing required data: AssetPreflight has logged each missing
       file; bail here (before any heavy load and before "Ready") via TheEnd so it
       flows through the same fatal surface as every other error — non-zero exit
       plus the native dialog in TheEndInfo for terminal-less launches. */
    if (AssetPreflight() > 0) {
        TheEnd(ERROR_NOT_FOUND_FILE, "required game data is missing (see the log)");
    }

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

    BufText = (U8 *)SmartMalloc(BIG_FILE_DIA);
    if (!BufText)
        TheEnd(NOT_ENOUGH_MEM, "BufText");

    BufOrder = (U16 *)SmartMalloc(BIG_FILE_ORD);
    if (!BufOrder)
        TheEnd(NOT_ENOUGH_MEM, "BufOrder");

    InitDial(0);
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
        if (Control_Begin())
            MainLoop();
        Control_End();
        TheEnd(PROGRAM_OK, "");
        return 0;
    }

    MainGameMenu(load);

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
