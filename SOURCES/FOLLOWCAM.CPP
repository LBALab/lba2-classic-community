#include "C_EXTERN.H"

#include "CONSOLE/CONSOLE.H" /* Console_RegisterCvar, for this module's own cvars */
#include "FOLLOWCAM.H"
#include "FOLLOWCAM_CFG.H"
#include "FOLLOWCAM_MATH.H"
#include "SETTINGS.H"
#include "INPUT.H"
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOG.H>

/* FollowCamera refreshes terrain with AFF_ALL_FLIP every movement frame.
 * AFF_ALL_FLIP runs the full AffScene preamble (MemoClipWindow / UnsetClipWindow
 * / ClearImpactRain) before the render, preventing black edges from active clip
 * regions. Wall-clock time during the render+vsync wait is correctly credited
 * to TimerRefHR by ManageTime() after the LIB386/SYSTEM/TIMER.CPP generalised
 * fix; see docs/TIMING.md. Vsync naturally caps terrain refreshes at the
 * display rate (~60 Hz). When the hero is idle, all work is skipped entirely. */
/* Settings, read from the cfg and mostly exposed as console cvars. Defaults in
 * FOLLOWCAM_CFG.H. Declared in FOLLOWCAM.H. */
S32 FollowCamera = FALSE;
S32 FollowCamReengageFrames = FOLLOW_CAM_REENGAGE_FRAMES_DEFAULT;
S32 FollowCamDriftDivisor = FOLLOW_CAM_DRIFT_DIVISOR_DEFAULT;
S32 FollowCamHDRecompose = FOLLOW_CAM_HD_RECOMPOSE_DEFAULT;
S32 FollowCamHDExcessCap = FOLLOW_CAM_HD_EXCESS_CAP_DEFAULT;
S32 FollowCamHDPitchGain = FOLLOW_CAM_HD_PITCH_GAIN_DEFAULT;
S32 FollowCamHDDistGain = FOLLOW_CAM_HD_DIST_GAIN_DEFAULT;
S32 FollowCamHDLeanGain = FOLLOW_CAM_HD_LEAN_GAIN_DEFAULT;
S32 FollowCamGround = FOLLOW_CAM_GROUND_DEFAULT;
S32 FollowCamGroundClearance = FOLLOW_CAM_GROUND_CLEARANCE_DEFAULT;
S32 FollowCamManualHoldFrames = FOLLOW_CAM_MANUAL_HOLD_DEFAULT;
S32 FollowCamHoldAngle = FOLLOW_CAM_HOLD_ANGLE_DEFAULT;

/* Console `camtrace`. */
S32 DbgCamTrace = 0;

/* Live state. The public half is declared in FOLLOWCAM.H; everything below the
 * blank line is private to this file and reached only through the four entry
 * points. */
S32 FollowCamBaseDist = 0; /* player's chosen zoom distance (0 = uninitialised) */

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

/* This file's live settings, applied to the rule in FOLLOWCAM_MATH.H (which the
 * host test covers). Callers pass 0 instead inside a camera zone, so an authored
 * shot is never recomposed. Tuning in FOLLOWCAM_CFG.H. */
static S32 FollowCamHDExcess(void) {
    return FollowCamHDExcessFor(FollowCamHDRecompose, (S32)ModeDesiredY, FollowCamHDExcessCap);
}

/* camtrace: one line per frame describing where the camera is and who is deciding
 * that.
 *
 * Emitted from the main loop rather than from the Auto camera's own update, which
 * runs only while the Auto camera holds the view: a trace written from in there
 * can never report a camera zone, a cutscene, an interior or the classic camera,
 * because it does not run in any of them. Those are most of the camera system and
 * all of the moments it changes hands.
 *
 * `step` is BetaCam's movement since the previous frame, so it counts whoever
 * moved it -- the Auto camera's lerp, a zone's authored angle, a scripted order,
 * or the classic camera's quarter turns -- rather than only the one writer the
 * old placement could see. `zone` and `forced` separate the frame a camera zone
 * re-aims the view from the state of being under its shot. */
void FollowCamTrace(void) {
    static S32 prevBeta = 0;
    static S32 prevX = 0;
    static S32 prevZ = 0;
    static S32 primed = FALSE;

    T_OBJ_3D *ptr3d = &ListObjet[NumObjFollow].Obj;
    S32 step;
    S32 moving;

    if (!DbgCamTrace) {
        /* Keep the references current while the trace is off, so switching it on
         * mid-run reports a real step rather than the distance back to whenever
         * it was last on. */
        prevBeta = BetaCam;
        prevX = ptr3d->X;
        prevZ = ptr3d->Z;
        primed = TRUE;
        return;
    }

    /* The first traced frame has nothing to measure against, and reporting the
     * distance from an unset reference would put a step of a couple of thousand
     * units at the top of the run -- which is the shape of the very defects these
     * traces are read for, and would be picked up by anything scanning for the
     * largest movement. */
    step = 0;
    moving = 0;
    if (primed) {
        step = BetaCam - prevBeta;
        if (step > 2048)
            step -= 4096;
        if (step < -2048)
            step += 4096;

        moving = (ptr3d->X != prevX OR ptr3d->Z != prevZ);
    }
    primed = TRUE;

    Log_Info("[cam] heroBeta %4d add %4d beta %4d target %4d step %5d delay %3d orbit %d moving %d "
             "zone %d forced %d cine %d follow %d ext %d vue %d alpha %4d dist %6d",
             (int)ptr3d->Beta, (int)AddBetaCam, (int)BetaCam, (int)FollowCamTargetBeta,
             (int)step, (int)FollowCamReengageDelay, (int)ManualCameraOrbitActive, (int)moving,
             (int)CameraZone, (int)FlagCameraForcee, (int)CinemaMode, (int)FollowCamera,
             (int)(CubeMode == CUBE_EXTERIEUR), (int)VueCamera, (int)AlphaCam, (int)VueDistance);

    prevBeta = BetaCam;
    prevX = ptr3d->X;
    prevZ = ptr3d->Z;
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
void UpdateFollowCameraExt(void) {
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
    S32 catchupDiff = FollowCamAngleDiff(FollowCamTargetBeta, BetaCam);

    if (FollowCamNeedsUpdate(ptr3d, catchupDiff)) {
        /* HD recompose strength (kExcess): 0 at 480 or when disabled. Shared by
                         * the forward-lean cut here and the boom pull-in + pitch steepen at
                         * the apply below, so the whole recompose is a no-op at 480.
                         *
                         * Not gated on CameraZone. This function has one caller and it sits
                         * inside `if (!CameraZone)`, so a camera zone never reaches here to
                         * be tested for: it keeps the follow camera from running at all,
                         * rather than running it with the correction switched off. */
        S32 hdExcess = FollowCamHDExcess();

        /* Manual-override fade: a manual nudge set FollowCamManualHold
                         * (EXTFUNC), driving the auto-assist strength to 0 so the
                         * player's tilt/orbit wins; it eases back to full as the hold
                         * counts down. Scales the recompose pitch steepen and the
                         * ground-lift re-aim below (the two that fight a manual tilt);
                         * 100 (full assist) when no hold. */
        if (FollowCamManualHold > 0)
            FollowCamManualHold--;
        /* The console sets cvars without going through the config's range check,
         * so clamp where it is used, as the orbit glide does in EXTFUNC. Both
         * bounds are load-bearing here rather than tidiness: the hold counter is
         * seeded from the frame count, so an unbounded cam_manual_hold makes
         * `hold * 100` leave S32, and the negative product that follows drives
         * the fade to full assist exactly when the player has asked for none. */
        S32 holdFrames = FollowCamManualHoldFrames;
        if (holdFrames > FOLLOW_CAM_MANUAL_HOLD_MAX)
            holdFrames = FOLLOW_CAM_MANUAL_HOLD_MAX;
        S32 hold = FollowCamManualHold;
        if (hold > holdFrames)
            hold = holdFrames;
        S32 autoFactor = 100;
        if (holdFrames > 0)
            autoFactor = 100 - hold * 100 / holdFrames;
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
        FollowCamTargetBeta = FollowCamTargetBetaFor(AddBetaCam, ptr3d->Beta);

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
                /* The minimum step is what stops integer division truncating to zero
                 * a few units short of the target, where the camera would neither
                 * step nor snap: it would sit off target for ever with the dirty
                 * check firing and the exterior re-rendering every idle frame. */
                S32 step = FollowCamRotStep(diff, dynamicDiv, FOLLOW_CAM_ROT_MIN_STEP);
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

/* The two entry points below are the assignments the main loop used to make
 * into this file's statics, named for the moments that need them. */

void FollowCamResetToView(void) {
    FollowCamBaseDist = FOLLOW_CAM_INITIAL_DIST;
    FollowCamEffectiveDist = FollowCamBaseDist;
    FollowCamSyncTarget();
    FollowCamReengageDelay = 0;
    FollowCamPrevBaseDist = FollowCamBaseDist;
    FollowCamPrevAlphaCam = AlphaCam;
    /* Drop the smoothed ground lift so the recenter starts from the clean reset
     * position instead of gliding down from a stale terrain-occluder lift. */
    FollowCamEyeLift = 0;
    FollowCamGroundSettling = 0;
}

void FollowCamSyncTarget(void) {
    FollowCamTargetBeta = BetaCam;
}

/* The cfg keys this module owns. Declared in FOLLOWCAM.H, called by
 * CONFIG_FILE.CPP. Every clamp here sits in the same file as the arithmetic
 * that needs it: FollowCamGroundClearance's ceiling, for one, exists to keep
 * (g + clearance - hy) * 100 inside S32 in the occlusion march below. */
/* Every tunable this module owns, declared once. The cfg reader, the cfg writer
 * and the console all read this rather than naming a setting, so adding one is a
 * single row and its range travels with it to whichever surface writes it.
 *
 * Row order is the cfg's key order, which is user-visible: lba2.cfg is rewritten
 * in this order, and reordering rows would reorder every player's file.
 *
 * cam_follow has no key here. FollowCamera is persisted by CONFIG_FILE.CPP at its
 * own position in the file, apart from this block. */
static const T_SETTING FollowCamSettings[] = {
    {"FollowCamReengageFrames", NULL, "cam_recenter_delay", &FollowCamReengageFrames, SETTING_CLAMP,
     FOLLOW_CAM_REENGAGE_FRAMES_DEFAULT, 0, 600, NULL, NULL,
     "Frames before auto-cam re-centers after a manual pan"},
    {"FollowCamDriftDivisor", NULL, "cam_recenter_drift", &FollowCamDriftDivisor, SETTING_CLAMP,
     FOLLOW_CAM_DRIFT_DIVISOR_DEFAULT, 1, 200, NULL, NULL,
     "Auto-cam re-center drift divisor (higher = gentler)"},
    {"FollowCamHDRecompose", NULL, "cam_hd", &FollowCamHDRecompose, SETTING_TRUTHY,
     FOLLOW_CAM_HD_RECOMPOSE_DEFAULT, 0, 1, NULL, NULL,
     "Auto cam HD recompose: pull in + steepen at tall render heights"},
    {"FollowCamHDExcessCap", NULL, "cam_hd_cap", &FollowCamHDExcessCap, SETTING_CLAMP,
     FOLLOW_CAM_HD_EXCESS_CAP_DEFAULT, 0, SETTING_MAX_NONE, NULL, NULL,
     "Cap on recompose strength so tall res converges (0 = pure linear; 70 ~= 816p)"},
    {"FollowCamHDPitchGain", NULL, "cam_hd_pitch", &FollowCamHDPitchGain, SETTING_CLAMP,
     FOLLOW_CAM_HD_PITCH_GAIN_DEFAULT, SETTING_MIN_NONE, SETTING_MAX_NONE, NULL, NULL,
     "HD recompose pitch gain (AlphaCam units per 1.0 of k-1)"},
    {"FollowCamHDDistGain", NULL, "cam_hd_dist", &FollowCamHDDistGain, SETTING_CLAMP,
     FOLLOW_CAM_HD_DIST_GAIN_DEFAULT, SETTING_MIN_NONE, SETTING_MAX_NONE, NULL, NULL,
     "HD recompose distance pull (% of boom per 1.0 of k-1)"},
    {"FollowCamHDLeanGain", NULL, "cam_hd_lean", &FollowCamHDLeanGain, SETTING_CLAMP,
     FOLLOW_CAM_HD_LEAN_GAIN_DEFAULT, SETTING_MIN_NONE, SETTING_MAX_NONE, NULL, NULL,
     "HD recompose forward-lean cut (% per 1.0 of k-1)"},
    {"FollowCamGround", NULL, "cam_ground", &FollowCamGround, SETTING_TRUTHY,
     FOLLOW_CAM_GROUND_DEFAULT, 0, 1, NULL, NULL,
     "Auto cam: smooth terrain ground-clearance lift"},
    /* The ceiling keeps (g + clearance - hy) * 100 inside S32 in the occlusion
     * march below, which is why it is stated with the setting rather than at the
     * one surface that used to check it. */
    {"FollowCamGroundClearance", NULL, "cam_ground_clear", &FollowCamGroundClearance, SETTING_CLAMP,
     FOLLOW_CAM_GROUND_CLEARANCE_DEFAULT, 0, 20000, NULL, NULL,
     "Ground clearance: keep the eye this far above the ground under it"},
    {"FollowCamManualHoldFrames", NULL, "cam_manual_hold", &FollowCamManualHoldFrames, SETTING_CLAMP,
     FOLLOW_CAM_MANUAL_HOLD_DEFAULT, 0, FOLLOW_CAM_MANUAL_HOLD_MAX, NULL, NULL,
     "Frames the pitch/distance auto-assist yields after a manual camera nudge"},
    {"FollowCamHoldAngle", NULL, "cam_hold_angle", &FollowCamHoldAngle, SETTING_TRUTHY,
     FOLLOW_CAM_HOLD_ANGLE_DEFAULT, 0, 1, NULL, NULL,
     "Hold the manual camera rotation (free-camera); off = classic auto-recenter drift"},
    /* cvar only, and it must stay that way. FollowCamera's cfg row lives in
       CONFIG_FILE.CPP's table, which reads it with the legacy AutoCameraCenter
       fallback and its own rule; giving this row a key would read the same
       global twice under two rules, and this table runs last. */
    {NULL, NULL, "cam_follow", &FollowCamera, SETTING_TRUTHY, FALSE, 0, 1, NULL, NULL,
     "Auto camera (third-person follow) enable"},
};
#define FOLLOW_CAM_SETTING_COUNT ((S32)(sizeof(FollowCamSettings) / sizeof(FollowCamSettings[0])))

void FollowCam_ReadConfig(void) {
    Settings_ReadConfig(FollowCamSettings, FOLLOW_CAM_SETTING_COUNT);
}

void FollowCam_WriteConfig(void) {
    Settings_WriteConfig(FollowCamSettings, FOLLOW_CAM_SETTING_COUNT);
}

void FollowCam_RegisterCvars(void) {
    Settings_RegisterCvars(FollowCamSettings, FOLLOW_CAM_SETTING_COUNT);
}

/* The console commands this module owns, registered from Console_RegisterAll
 * beside its cvars.
 *
 * camnudge is not here. It writes DbgCamNudge*, which EXTFUNC.CPP's manual
 * camera path consumes, so it belongs to that path and stays with the other
 * bus state until that path has a module of its own. */
/* camtrace <0|1>: log the Auto-camera angle state on every follow-cam update, so a headless run can
   show frame by frame what the camera did while the hero turned and the player orbited. */
static void cmd_camtrace(int argc, const char *argv[]) {
    if (argc >= 2)
        DbgCamTrace = atoi(argv[1]) ? 1 : 0;
    Console_Print("camtrace: %s", DbgCamTrace ? "ON" : "OFF");
}

void FollowCam_RegisterCommands(void) {
    Console_RegisterCommandEx("camtrace", cmd_camtrace, "Log the Auto-camera angle state each update",
                              "camtrace <0|1>",
                              "Logs hero Beta, AddBetaCam, BetaCam and its target, the re-engage "
                              "countdown and the manual-orbit flag on every follow-cam update.");
}
