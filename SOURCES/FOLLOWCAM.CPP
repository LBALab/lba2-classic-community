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
S32 FollowCamSceneCarry = FOLLOW_CAM_SCENE_CARRY_DEFAULT;
S32 FollowCamZoneSkip = FOLLOW_CAM_ZONE_SKIP_DEFAULT;
S32 FollowCamObstruct = FOLLOW_CAM_OBSTRUCT_DEFAULT;
S32 FollowCamObstructHurryDivisor = FOLLOW_CAM_OBSTRUCT_HURRY_DEFAULT;
S32 FollowCamHDRecompose = FOLLOW_CAM_HD_RECOMPOSE_DEFAULT;
S32 FollowCamHDExcessCap = FOLLOW_CAM_HD_EXCESS_CAP_DEFAULT;
S32 FollowCamHDPitchGain = FOLLOW_CAM_HD_PITCH_GAIN_DEFAULT;
S32 FollowCamHDDistGain = FOLLOW_CAM_HD_DIST_GAIN_DEFAULT;
S32 FollowCamHDLeanGain = FOLLOW_CAM_HD_LEAN_GAIN_DEFAULT;
S32 FollowCamGround = FOLLOW_CAM_GROUND_DEFAULT;
S32 FollowCamGroundClearance = FOLLOW_CAM_GROUND_CLEARANCE_DEFAULT;
S32 FollowCamManualHoldFrames = FOLLOW_CAM_MANUAL_HOLD_DEFAULT;
S32 FollowCamHoldAngle = FOLLOW_CAM_HOLD_ANGLE_DEFAULT;
S32 FollowCamZoneReturn = FOLLOW_CAM_ZONE_RETURN_DEFAULT;
S32 FollowCamZoneAnchor = FOLLOW_CAM_ZONE_ANCHOR_DEFAULT;
S32 FollowCamWorldAnchor = FOLLOW_CAM_WORLD_ANCHOR_DEFAULT;
S32 FollowCamWorldReturn = FOLLOW_CAM_WORLD_RETURN_DEFAULT;
S32 FollowCamEaseMs = FOLLOW_CAM_EASE_MS_DEFAULT;
S32 FollowCamTargetMs = FOLLOW_CAM_TARGET_MS_DEFAULT;

/* Console `camtrace`. */
S32 DbgCamTrace = 0;

/* Live state. The public half is declared in FOLLOWCAM.H; everything below the
 * blank line is private to this file and reached only through the entry
 * points that header declares. */
S32 FollowCamBaseDist = 0; /* player's chosen zoom distance (0 = uninitialised) */

static S32 FollowCamPrevHeroX = 0;
static S32 FollowCamPrevHeroY = 0;
static S32 FollowCamPrevHeroZ = 0;
static S32 FollowCamPrevHeroBeta = 0;
static S32 FollowCamPrevAddBeta = 0;      /* dirty check: last seen AddBetaCam */
static S32 FollowCamEffectiveDist = 0;    /* spring arm: smoothed distance (0 = uninitialised) */
static S32 FollowCamTargetBeta = 0;       /* rotation lag: desired BetaCam */
S32 FollowCamReengageDelay = 0;           /* frames before auto-center resumes after manual pan */
S32 FollowCamManualHold = 0;              /* frames the pitch/distance auto-assist yields after a manual nudge */
static S32 FollowCamPrevBaseDist = -1;    /* dirty: zoom from GereExtKeys (numpad * /) */
static S32 FollowCamPrevAlphaCam = -1;    /* dirty: numpad +/- elevation while follow */
static S32 FollowCamEyeLift = 0;          /* ground clearance: eased eye lift above terrain */
static S32 FollowCamGroundSettling = 0;   /* ground clearance: lift still easing (keeps the dirty check live while the hero stands still) */
static S32 FollowCamObstructExtra = 0;    /* obstruction clearance: eased pitch added for the render */
static S32 FollowCamObstructTarget = 0;   /* obstruction clearance: what the last probe asked for */
static S32 FollowCamObstructSnap = 0;     /* obstruction clearance: the next update starts settled */
static S32 FollowCamObstructHurrying = 0; /* obstruction clearance: the hero was drawn hidden (1 while rising, 2 with the search silent) */
static S32 FollowCamObstructFloor = 0;    /* obstruction clearance: the least extra a hidden hero has shown this spot to need */
static S32 FollowCamObstructFloorX = 0;   /* obstruction clearance: where he stood when the floor was last raised */
static S32 FollowCamObstructFloorZ = 0;
static S32 FollowCamHiddenSeen = 0; /* camtrace: what the last render made of a hidden hero (FollowCamNoteHidden) */

/* The eases paced by the clock (cam_ease_ms, cam_target_ms). They are paced by
 * the game clock's time since the previous update, which is a frame's length
 * only when that update ran on the previous frame. */
static U32 FollowCamFrame = 0;             /* simulation steps, counted by FollowCamNoteFrame */
static U32 FollowCamEaseFrame = 0;         /* the frame the last update ran on */
static U32 FollowCamEaseClock = 0;         /* TimerRefHR at the last update */
static S32 FollowCamEaseClockRuns = FALSE; /* those two describe an update the next one can be timed from */
static S32 FollowCamEaseDt = 0;            /* camtrace: the milliseconds this frame's update was paced by */
static S32 FollowCamYawCarry = 0;          /* thousandths of a unit the yaw ease has earned and not yet moved */
static S32 FollowCamTargetLagX = 0;        /* look-at point: how far it trails the spot ahead of the hero */
static S32 FollowCamTargetLagY = 0;
static S32 FollowCamTargetLagZ = 0;
static S32 FollowCamTargetCarryX = 0; /* look-at point: thousandths earned and not yet moved */
static S32 FollowCamTargetCarryY = 0;
static S32 FollowCamTargetCarryZ = 0;
static S32 FollowCamTargetSpotX = 0; /* look-at point: where that spot was at the last update */
static S32 FollowCamTargetSpotY = 0;
static S32 FollowCamTargetSpotZ = 0;
static S32 FollowCamTargetKnown = FALSE; /* look-at point: the three above are set */

/* A scripted shot's loan (cam_zone_return / cam_zone_anchor). The bits say what
 * is still owed: an axis drops out when it has been given back, or when the
 * player sets it by hand and so makes it theirs. RETURNING is set from the frame
 * the hero leaves the shot's box until both axes are settled. */
#define FOLLOW_CAM_LOAN_YAW 1
#define FOLLOW_CAM_LOAN_PITCH 2
#define FOLLOW_CAM_LOAN_RETURNING 4
S32 FollowCamInShotBox = FALSE;         /* this frame: the followed object is in the box of a camera zone that has fired */
static S32 FollowCamLoan = 0;           /* FOLLOW_CAM_LOAN_* */
static S32 FollowCamLentPan = 0;        /* AddBetaCam as it was before the shot */
static S32 FollowCamLentHeroBeta = 0;   /* the hero's facing that pan is measured from (cam_world_anchor) */
static S32 FollowCamLentAlpha = 0;      /* AlphaCam as it was before the shot */
static S32 FollowCamShotBeta = 0;       /* the shot's authored world yaw */
static S32 FollowCamWasIndoors = FALSE; /* the last simulated frame was in an interior */
static S32 FollowCamWasCinema = FALSE;  /* the last simulated frame was in a cutscene */

/* The pitch the Auto camera rests at when nobody else has it: the player's own
 * tilt if they set one, the preset a recentre put there if not. -1 until one of
 * those has happened. AlphaCam cannot stand in for it, because two writers put
 * pitches there that are not the camera's own and nothing takes them away
 * again: a camera zone, and the obstruction search CameraCenter runs, whose
 * position is discarded by the next follow update while the pitch it wrote
 * beside it stays. */
static S32 FollowCamOwnAlpha = -1;

/* cam_world_anchor: the hero's facing AddBetaCam was last measured from. */
static S32 FollowCamAnchorHeroBeta = 0;
static S32 FollowCamWorldReturnCarry = 0; /* thousandths of a unit the return has earned and not yet moved */

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
 * re-aims the view from the state of being under its shot. `inshot` is the hero
 * standing in the box of a zone that has fired, which for a zone that fires once
 * is the only sign he is still there, and `loan` is what the Auto camera still
 * owes back from a shot (1 yaw, 2 pitch, 4 on its way back; always 0 unless
 * cam_zone_return or cam_zone_anchor is on). `obs` is the pitch the obstruction
 * clearance is adding to `alpha` for the render (always 0 unless cam_obstruct
 * is on). `hid` is what the render since the last line made of the followed
 * object: 0 seen, 1 drawn fully hidden and left so, 2 hidden with the clearance
 * still on its way up, 3 hidden and recentred. `dt` is the milliseconds the
 * Auto camera's update was paced by on this frame: 0 on a frame it did not run,
 * and on the first one after a frame without it. `tlag` is how far the look-at
 * point trails the spot ahead of the hero, in world units along the ground
 * (always 0 with cam_target_ms 0, and on a frame the update did not run). */
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
             "zone %d forced %d cine %d follow %d ext %d vue %d alpha %4d dist %6d inshot %d loan %d obs %d hid %d "
             "dt %3d tlag %4d",
             (int)ptr3d->Beta, (int)AddBetaCam, (int)BetaCam, (int)FollowCamTargetBeta,
             (int)step, (int)FollowCamReengageDelay, (int)ManualCameraOrbitActive, (int)moving,
             (int)CameraZone, (int)FlagCameraForcee, (int)CinemaMode, (int)FollowCamera,
             (int)(CubeMode == CUBE_EXTERIEUR), (int)VueCamera, (int)AlphaCam, (int)VueDistance,
             (int)FollowCamInShotBox, (int)FollowCamLoan, (int)FollowCamObstructExtra, (int)FollowCamHiddenSeen,
             (int)FollowCamEaseDt,
             (int)((FollowCamEaseFrame == FollowCamFrame) ? Distance2D(0, 0, FollowCamTargetLagX, FollowCamTargetLagZ) : 0));
    FollowCamHiddenSeen = 0;

    prevBeta = BetaCam;
    prevX = ptr3d->X;
    prevZ = ptr3d->Z;
}

/* Whether the yaw still has ground to make up on its target.
 *
 * The per-frame lerp stops inside its snap threshold: within it the camera is
 * left where it is until something else redraws the frame, up to a degree off.
 * The timed ease lands on the target, so what is left inside the threshold is
 * still owed. But only while the lerp is free to pay it: during the grace after
 * a manual pan the camera is held off its target on purpose, and calling that
 * owed would redraw the scene on every frame the hero stood still. */
static S32 FollowCamCatchupOwed(S32 catchupDiff) {
    if (catchupDiff > FOLLOW_CAM_ROT_THRESHOLD OR catchupDiff < -FOLLOW_CAM_ROT_THRESHOLD)
        return TRUE;

    return FollowCamEaseMs > 0 AND catchupDiff != 0 AND !ManualCameraOrbitActive AND FollowCamReengageDelay == 0;
}

/* The follow camera re-renders when the hero moved or turned, the player panned,
 * BetaCam still has catch-up pending, the zoom/elevation changed, or the
 * ground-lift / manual-hold / look-at point are still settling (the last three
 * keep this true so they finish easing while the hero stands still). */
static S32 FollowCamNeedsUpdate(T_OBJ_3D *ptr3d, S32 catchupDiff) {
    return ptr3d->X != FollowCamPrevHeroX OR ptr3d->Y != FollowCamPrevHeroY OR ptr3d->Z != FollowCamPrevHeroZ OR ptr3d->Beta != FollowCamPrevHeroBeta OR AddBetaCam != FollowCamPrevAddBeta OR FollowCamCatchupOwed(catchupDiff) OR FollowCamBaseDist != FollowCamPrevBaseDist OR AlphaCam != FollowCamPrevAlphaCam OR FollowCamGroundSettling OR(FollowCamObstruct AND(FollowCamObstructExtra != FollowCamObstructTarget OR FollowCamObstructSnap)) OR FollowCamManualHold OR FollowCamTargetLagX OR FollowCamTargetLagY OR FollowCamTargetLagZ;
}

/* The eases paced by the clock start over: the next update is not timed from
 * the last one, and the look-at point is back on its spot. For a frame the
 * update did not run on, and for every moment the camera is put somewhere with
 * a cut. */
static void FollowCamEaseRestart(void) {
    FollowCamEaseClockRuns = FALSE;
    FollowCamYawCarry = 0;
    FollowCamTargetLagX = FollowCamTargetLagY = FollowCamTargetLagZ = 0;
    FollowCamTargetCarryX = FollowCamTargetCarryY = FollowCamTargetCarryZ = 0;
    FollowCamTargetKnown = FALSE;
}

/* How long this update is paced by, in milliseconds of the game clock, and the
 * clock read for the next one. The game clock and not the host's: it stands
 * still in a menu, and it is the one a session replay reproduces.
 *
 * 0 when the previous update was not on the previous frame. A camera zone, a
 * cutscene or a building may have had the view for any length of time, and
 * spent in one step that time would be a jump on the first frame back. A frame
 * longer than FOLLOW_CAM_EASE_DT_MAX is paced as one of that length, so a stall
 * is not a jump either. That also covers a clock a loaded save has set back,
 * which reads as a very long frame. */
static S32 FollowCamEaseTick(void) {
    U32 now = TimerRefHR;
    S32 dtMs = 0;

    if (FollowCamEaseClockRuns AND FollowCamFrame - FollowCamEaseFrame == 1) {
        U32 gone = now - FollowCamEaseClock;

        dtMs = (gone > FOLLOW_CAM_EASE_DT_MAX) ? FOLLOW_CAM_EASE_DT_MAX : (S32)gone;
    } else {
        FollowCamEaseRestart();
    }

    FollowCamEaseClock = now;
    FollowCamEaseFrame = FollowCamFrame;
    FollowCamEaseClockRuns = TRUE;
    FollowCamEaseDt = dtMs;

    return dtMs;
}

/* cam_target_ms. The camera looks at a spot ahead of the hero, which the caller
 * has just put in VueOffsetX/Y/Z. Pinned there, the view starts and stops on
 * the frame he does. With the setting on, what the camera looks at trails that
 * spot and closes on it with the setting's time constant.
 *
 * What is kept is how far it trails, not where it is. The seam between two
 * outdoor scenes renumbers the world under the hero without moving him on
 * screen, and a place remembered in the old scene's coordinates would be a
 * scene's width away in the new one's. So each update the trail grows by as far
 * as the spot moved, as though the look-at point had stayed put in the world,
 * and then closes. A spot that moved further than anything walks in one update
 * was not walked there, and the trail is left as it was. */
static void FollowCamEaseLookAt(S32 dtMs) {
    S32 spotX = VueOffsetX, spotY = VueOffsetY, spotZ = VueOffsetZ;

    if (FollowCamTargetMs <= 0) {
        FollowCamTargetLagX = FollowCamTargetLagY = FollowCamTargetLagZ = 0;
        FollowCamTargetKnown = FALSE;
        return;
    }

    if (FollowCamTargetKnown) {
        S32 dx = spotX - FollowCamTargetSpotX;
        S32 dy = spotY - FollowCamTargetSpotY;
        S32 dz = spotZ - FollowCamTargetSpotZ;

        if (dx <= FOLLOW_CAM_TARGET_JUMP AND dx >= -FOLLOW_CAM_TARGET_JUMP AND dy <= FOLLOW_CAM_TARGET_JUMP AND dy >= -FOLLOW_CAM_TARGET_JUMP AND dz <= FOLLOW_CAM_TARGET_JUMP AND dz >= -FOLLOW_CAM_TARGET_JUMP) {
            FollowCamTargetLagX -= dx;
            FollowCamTargetLagY -= dy;
            FollowCamTargetLagZ -= dz;

            FollowCamTargetLagX += FollowCamEaseStepMs(-FollowCamTargetLagX, dtMs, FollowCamTargetMs,
                                                       FOLLOW_CAM_TARGET_MIN_RATE, &FollowCamTargetCarryX);
            FollowCamTargetLagY += FollowCamEaseStepMs(-FollowCamTargetLagY, dtMs, FollowCamTargetMs,
                                                       FOLLOW_CAM_TARGET_MIN_RATE, &FollowCamTargetCarryY);
            FollowCamTargetLagZ += FollowCamEaseStepMs(-FollowCamTargetLagZ, dtMs, FollowCamTargetMs,
                                                       FOLLOW_CAM_TARGET_MIN_RATE, &FollowCamTargetCarryZ);
        }
        /* else the trail is held for this update, neither grown nor closed:
         * how far the hero walked across the jump is not known, and closing
         * without growing would pull the view toward him by a step. */
    }

    FollowCamTargetSpotX = spotX;
    FollowCamTargetSpotY = spotY;
    FollowCamTargetSpotZ = spotZ;
    FollowCamTargetKnown = TRUE;

    VueOffsetX = spotX + FollowCamTargetLagX;
    VueOffsetY = spotY + FollowCamTargetLagY;
    VueOffsetZ = spotZ + FollowCamTargetLagZ;
}

/* The pitch the classic obstruction search wants for the camera as it stands:
 * its march out along the boom over terrain, scenery boxes and scripted
 * objects, asked as a question. SearchCameraPos answers by writing AlphaCam,
 * the eye and the target, so each is put back. It reads AlphaCam, BetaCam and
 * VueDistance, which the caller has set to the view about to be rendered. */
static S32 FollowCamNeededPitch(T_OBJ_3D *ptr3d) {
    S32 saveAlpha = AlphaCam;
    S32 saveOffX = VueOffsetX, saveOffY = VueOffsetY, saveOffZ = VueOffsetZ;
    S32 savePosX = CamPosX, savePosY = CamPosY, savePosZ = CamPosZ;
    S32 needed;

    SearchCameraPos(ptr3d->X, ptr3d->Y, ptr3d->Z, ptr3d->Beta, 1);
    needed = AlphaCam;

    AlphaCam = saveAlpha;
    VueOffsetX = saveOffX;
    VueOffsetY = saveOffY;
    VueOffsetZ = saveOffZ;
    CamPosX = savePosX;
    CamPosY = savePosY;
    CamPosZ = savePosZ;

    return needed;
}

/* cam_world_anchor. The update aims behind the hero, offset by AddBetaCam, so a
 * turn of his moves the target by as much. With the setting on, the turn he has
 * made since the last update is taken back out of the offset, which leaves the
 * target where it was in the world. Whatever else wrote the offset in between
 * (an orbit, the pan keys, the quarter-turn key, a script) is kept, so the
 * player still turns the camera and only the hero no longer does.
 *
 * Only across consecutive updates. After a frame without one (a camera zone, a
 * cutscene, a building) or a cut, the offset is taken as it stands: whoever had
 * the view has set it for the facing the hero has now.
 *
 * What a scripted shot owes back for the yaw is a world direction too, for as
 * long as the anchor is on: the pan it was lent is re-solved against his facing
 * now, from the facing it was measured at. That one is kept across gaps as
 * well, since a shot that holds the view keeps this update from running for as
 * long as he stays in its box. */
static void FollowCamWorldAnchorStep(T_OBJ_3D *ptr3d, S32 consecutive) {
    S32 heroBeta = ptr3d->Beta & 4095;

    if (FollowCamWorldAnchor) {
        if (consecutive)
            AddBetaCam = (AddBetaCam - (heroBeta - FollowCamAnchorHeroBeta)) & 4095;

        if (FollowCamLoan & FOLLOW_CAM_LOAN_YAW) {
            FollowCamLentPan = FollowCamPanForAngle(FollowCamTargetBetaFor(FollowCamLentPan, FollowCamLentHeroBeta), heroBeta);
            FollowCamLentHeroBeta = heroBeta;
        }
    }

    FollowCamAnchorHeroBeta = heroBeta;
}

/* cam_world_anchor with cam_hold_angle 0: the camera comes back behind the hero
 * as he walks. What pulls is the ground he covered since the last update, so
 * turning on the spot or standing still does not, nor does walking backwards. The pull is the clock ease's exponential with that
 * distance standing for the time, so cam_world_return reads as a time constant
 * does and the camera lands behind him.
 *
 * Not during the grace after the player moved the camera, nor while a shot's
 * yaw is owed (the loan decides that one), nor across a jump: a teleport, or the
 * seam between two outdoor scenes, which renumbers the world under him. */
static void FollowCamWorldReturnStep(T_OBJ_3D *ptr3d, S32 consecutive) {
    S32 dx = ptr3d->X - FollowCamPrevHeroX;
    S32 dz = ptr3d->Z - FollowCamPrevHeroZ;
    S32 walked;
    S32 pan;

    if (!FollowCamWorldAnchor OR FollowCamHoldAngle OR FollowCamWorldReturn <= 0 OR !consecutive OR FollowCamReengageDelay > 0 OR(FollowCamLoan & FOLLOW_CAM_LOAN_YAW) OR AddBetaCam == 0)
        return;
    if ((dx < 0 ? -dx : dx) > FOLLOW_CAM_TARGET_JUMP OR(dz < 0 ? -dz : dz) > FOLLOW_CAM_TARGET_JUMP)
        return;

    /* Backing away from his facing does not pull. Anything else does by the
     * ground covered, sliding along a wall included: the player is pushing him
     * forward, and the classic drift counts that too. */
    Rotate(0, 1024, ptr3d->Beta & 4095); /* his facing, 1024 long, in X0/Z0 */
    if (dx * X0 + dz * Z0 < 0)
        return;
    walked = Distance2D(0, 0, dx, dz);
    if (walked <= 0)
        return;

    pan = AddBetaCam & 4095;
    if (pan > 2048)
        pan -= 4096;
    AddBetaCam = (AddBetaCam + FollowCamEaseStepMs(-pan, walked, FollowCamWorldReturn,
                                                   FOLLOW_CAM_WORLD_RETURN_MIN_RATE, &FollowCamWorldReturnCarry)) &
                 4095;
}

/* Per-frame Auto-camera update (exterior). Extracted from the main loop:
 * spring-arm zoom, lazy orbit + pan drift, HD recompose, terrain occlusion
 * clearance, and the manual-override fade. Reads/writes the FollowCam* globals
 * and ListObjet[NumObjFollow]. */
void UpdateFollowCameraExt(void) {
    T_OBJ_3D *ptr3d = &ListObjet[NumObjFollow].Obj;
    S32 manualPan = CheckKey(K_LBRACKET) OR CheckKey(K_RBRACKET);
    S32 heroMoving = (ptr3d->X != FollowCamPrevHeroX OR ptr3d->Z != FollowCamPrevHeroZ);
    /* Read before FollowCamEaseTick moves the clock on: whether the last update
     * was on the previous step with no cut since. */
    S32 consecutive = FollowCamEaseClockRuns AND FollowCamFrame - FollowCamEaseFrame == 1;
    S32 dtMs = FollowCamEaseTick();

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

    /* --- A scripted shot's loan ---
     * Inert unless cam_zone_return or cam_zone_anchor is on: FollowCamLoan is
     * only ever set by FollowCamLendToShot, which does nothing with both off. */
    if (FollowCamLoan AND !FollowCamZoneReturn AND !FollowCamZoneAnchor)
        FollowCamLoan = 0; /* switched off with a shot still out: keep what it left, as before */

    FollowCamWorldAnchorStep(ptr3d, consecutive);

    if (FollowCamLoan AND !(FollowCamLoan & FOLLOW_CAM_LOAN_RETURNING) AND !FollowCamInShotBox) {
        /* The hero has left the shot's box. A zone that holds the view every
         * frame keeps this update from running at all while he is inside, so for
         * that kind this is the first frame back. */
        if (FollowCamZoneReturn) {
            FollowCamLoan |= FOLLOW_CAM_LOAN_RETURNING;
            /* Start from the angle on screen. The offset adopted when the shot
             * fired was measured against the heading the hero had on that frame;
             * he may have turned any amount under a held shot since, and without
             * this the camera would first swing by that turn and only then begin
             * to come back. */
            if (FollowCamLoan & FOLLOW_CAM_LOAN_YAW) {
                FollowCamAdoptAngle();
                /* And without waiting out a grace left over from a pan made
                 * before the shot. That countdown only runs while the hero
                 * walks, and it stops the camera following its target, so a
                 * player who had orbited a moment before the shot and stopped
                 * outside its box would watch the offset come home on paper
                 * while the camera stayed where the shot left it. */
                FollowCamReengageDelay = 0;
            }
        } else {
            FollowCamLoan = 0; /* anchor only: the shot ends here and nothing is given back */
        }
    }

    if (FollowCamLoan & FOLLOW_CAM_LOAN_RETURNING) {
        /* Runs whether or not the hero is walking, unlike the pan drift below: a
         * hand-back that paused whenever he stopped would leave the camera
         * parked at an angle that is neither the shot nor his own. Both steps
         * move a value the dirty check watches, so the frame is redrawn. */
        S32 div = FollowCamDriftDivisor;
        if (div < 1)
            div = 1; /* console can set 0; never divide by zero */

        if (FollowCamLoan & FOLLOW_CAM_LOAN_YAW) {
            AddBetaCam = FollowCamEasePan(AddBetaCam, FollowCamLentPan, div, FOLLOW_CAM_RETURN_YAW_MIN_STEP);
            if (((AddBetaCam - FollowCamLentPan) & 4095) == 0)
                FollowCamLoan &= ~FOLLOW_CAM_LOAN_YAW;
        }
        if (FollowCamLoan & FOLLOW_CAM_LOAN_PITCH) {
            AlphaCam = FollowCamEaseValue(AlphaCam, FollowCamLentAlpha, div, FOLLOW_CAM_RETURN_PITCH_MIN_STEP);
            if (AlphaCam == FollowCamLentAlpha)
                FollowCamLoan &= ~FOLLOW_CAM_LOAN_PITCH;
        }
        if (!(FollowCamLoan & (FOLLOW_CAM_LOAN_YAW | FOLLOW_CAM_LOAN_PITCH)))
            FollowCamLoan = 0;
    } else if (FollowCamZoneAnchor AND(FollowCamLoan & FOLLOW_CAM_LOAN_YAW)) {
        /* Still in the box of a shot that fired once: keep its world yaw by
         * re-solving the offset against the hero's facing every frame, so the
         * target the update computes below stays where the shot put it however
         * he turns. */
        AddBetaCam = FollowCamPanForAngle(FollowCamShotBeta, ptr3d->Beta);
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
        } else if (!FollowCamHoldAngle AND !FollowCamWorldAnchor AND AddBetaCam != 0 AND !(FollowCamLoan & FOLLOW_CAM_LOAN_YAW)) {
            /* Not while a shot's yaw is still owed: the offset then is the
             * shot's rather than a pan the player made, and what happens to it
             * is the loan's to decide above. Nor with the world anchor on:
             * this drift is paced by the frame, and the anchor's way back,
             * below, by the distance walked. */
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
    if (!manualPan)
        FollowCamWorldReturnStep(ptr3d, consecutive);

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
        /* The settings row bounds every writer, the console included; this clamp
         * stays beside the arithmetic because both bounds are load-bearing here
         * rather than tidiness: the hold counter is seeded from the frame count,
         * so an unbounded cam_manual_hold makes
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
            leanDist = FollowCamHDShrink(DISTANCE_VISEE, hdExcess,
                                         FollowCamHDLeanGain, FOLLOW_CAM_HD_LEAN_MAX_CUT);
        }
        Rotate(0, leanDist, leanBeta);

        VueOffsetX = ptr3d->X + X0;
        VueOffsetY = ptr3d->Y;
        VueOffsetZ = ptr3d->Z + Z0;

        FollowCamEaseLookAt(dtMs);

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
            S32 diff = FollowCamAngleDiff(FollowCamTargetBeta, BetaCam);

            if (FollowCamEaseMs > 0 AND !ManualCameraOrbitActive) {
                /* cam_ease_ms: the same inertia, as a time constant and paced
                 * by the clock, so the camera comes round in the same time at
                 * any frame rate. It lands on the target by itself, so there is
                 * no snap to finish it. An orbit the player is driving keeps
                 * the tight per-frame lerp below: that one smooths a device,
                 * frame by frame, and is tuned as such. */
                S32 tau = (S32)((S64)FollowCamEaseMs * FollowCamEffectiveDist / FollowCamBaseDist);

                if (tau < FollowCamEaseMs / 3)
                    tau = FollowCamEaseMs / 3;
                BetaCam = (BetaCam + FollowCamEaseStepMs(diff, dtMs, tau, FOLLOW_CAM_ROT_MIN_RATE, &FollowCamYawCarry)) & 4095;
            } else if (diff > FOLLOW_CAM_ROT_THRESHOLD OR diff < -FOLLOW_CAM_ROT_THRESHOLD) {
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
                renderDist = FollowCamHDShrink(FollowCamEffectiveDist, hdExcess,
                                               FollowCamHDDistGain, FOLLOW_CAM_HD_DIST_MAX_PULL);

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

            /* Obstruction clearance (cam_obstruct). The search says how steep
             * the view would have to be to clear what stands between the eye
             * and the hero. What that is above the player's own pitch is
             * carried as an extra that eases toward it each update, up and
             * down, so there is never a pitch to give back.
             *
             * The extra is not written into AlphaCam here. The scene render
             * sets the camera again from AlphaCam before it draws, so a pitch
             * changed for this update's CameraCenter and put back after it
             * never reaches the frame. CameraCenter and the render add
             * FollowCamRenderExtraPitch() themselves. For the same reason it
             * is measured from the player's own pitch and not from the one the
             * recompose has just steepened.
             *
             * In the box of a shot that has fired the place is the author's to
             * judge, as it is for the classic camera, which never searches
             * inside a zone, and the shot is shown as written. */
            if (FollowCamObstruct) {
                S32 composedAlpha = AlphaCam;

                AlphaCam = saveAlphaCam;

                /* A floor learnt from a hidden hero is about the spot he was
                 * hidden at. Once he has walked on, or the view has been cut
                 * to somewhere else, the search has the say again. */
                if (FollowCamObstructFloor) {
                    S32 dx = ptr3d->X - FollowCamObstructFloorX;
                    S32 dz = ptr3d->Z - FollowCamObstructFloorZ;

                    if (FollowCamObstructSnap OR dx > FOLLOW_CAM_OBSTRUCT_FLOOR_REACH OR dx<-FOLLOW_CAM_OBSTRUCT_FLOOR_REACH OR dz> FOLLOW_CAM_OBSTRUCT_FLOOR_REACH OR dz < -FOLLOW_CAM_OBSTRUCT_FLOOR_REACH)
                        FollowCamObstructFloor = 0;
                }

                {
                    /* In the box of a shot that has fired the search is not
                     * asked: the place is the author's to judge there, as it is
                     * for the classic camera, which never searches inside a
                     * zone. A hero drawn hidden is still answered, through the
                     * floor, or he would stay hidden with the recentre stood
                     * down and nothing in its place. */
                    FollowCamObstructTarget = FollowCamInShotBox ? 0 : FollowCamNeededPitch(ptr3d) - AlphaCam;
                    if (FollowCamObstructTarget < FollowCamObstructFloor)
                        FollowCamObstructTarget = FollowCamObstructFloor;
                    if (AlphaCam + FollowCamObstructTarget > FOLLOW_CAM_OBSTRUCT_PITCH_MAX)
                        FollowCamObstructTarget = FOLLOW_CAM_OBSTRUCT_PITCH_MAX - AlphaCam;
                    if (FollowCamObstructTarget < 0)
                        FollowCamObstructTarget = 0;

                    if (FollowCamObstructSnap) {
                        FollowCamObstructExtra = FollowCamObstructTarget;
                    } else if (FollowCamObstructHurrying == 2) {
                        /* Hidden with the search silent: the floor was raised a
                         * step and the extra goes up by that step, at once. Eased
                         * like the rest it would creep at a tenth of the step and
                         * leave him hidden for a second. */
                        FollowCamObstructExtra += FOLLOW_CAM_OBSTRUCT_HIDDEN_STEP;
                        if (FollowCamObstructExtra > FollowCamObstructTarget)
                            FollowCamObstructExtra = FollowCamObstructTarget;
                    } else {
                        S32 div = FOLLOW_CAM_OBSTRUCT_EASE;

                        if (FollowCamObstructHurrying) {
                            /* The console sets a cvar without the config's range
                             * check. Below 1 there is nothing to divide by, and
                             * past the ordinary ease a hidden hero would slow the
                             * camera down. */
                            div = FollowCamObstructHurryDivisor;
                            if (div < 1)
                                div = 1;
                            if (div > FOLLOW_CAM_OBSTRUCT_EASE)
                                div = FOLLOW_CAM_OBSTRUCT_EASE;
                        }
                        FollowCamObstructExtra = FollowCamEaseValue(FollowCamObstructExtra, FollowCamObstructTarget,
                                                                    div, FOLLOW_CAM_OBSTRUCT_MIN_STEP);
                    }
                }
                FollowCamObstructSnap = FALSE;
                FollowCamObstructHurrying = FALSE;
                AlphaCam = composedAlpha;
            } else {
                FollowCamObstructExtra = 0;
                FollowCamObstructTarget = 0;
                FollowCamObstructFloor = 0;
                FollowCamObstructSnap = FALSE;
                FollowCamObstructHurrying = FALSE;
            }

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

/* Arriving in an outdoor scene, for cam_scene_carry. ChangeCube calls this in
 * place of its own CameraCenter, once the hero stands where the new scene puts
 * him.
 *
 * What it replaces does one of two things to the Auto camera. An ordered change
 * (a script's, the console's) takes CameraCenter(1), which zeroes the pan and
 * puts the pitch back to the preset: the angle and the tilt the player had set
 * are gone. A door out of an interior takes CameraCenter(0), which leaves
 * BetaCam where the last outdoor scene left it and runs the classic obstruction
 * search once. The hero walked in facing the door and walks out facing away
 * from it, so the update finds the camera half a turn from its target and
 * spends two seconds swinging round him. And the search, finding the building he
 * has just left behind the camera, rewrites AlphaCam to look down over it. The
 * classic camera gives that pitch back at its next off-screen recentre; the Auto
 * camera has no such recentre and keeps it as though the player had chosen it.
 *
 * Here the pitch is left alone, since it is the player's. The pan is his too
 * for as long as he stays outdoors, so it holds from one outdoor scene to the
 * next. Coming out of an interior it goes home: it was set for a view he left
 * at the door on his way in, and held through a building it turns the camera
 * to his side or his face as he steps out. The camera is then placed behind
 * the hero at whatever pan that leaves, with the lerp's target on it, so
 * nothing is owed. There is no obstruction search: the update's eased clearance
 * takes over on this same frame.
 *
 * That clearance knows terrain and not decor. Where the place behind the hero
 * is inside the building he has just left, he is drawn fully hidden on the
 * arrival frame and the hidden-hero recentre in AffOneObject fires, as it would
 * anywhere else, leaving the same lasting pitch the door used to. That is the
 * Auto camera's missing decor clearance, not the scene change.
 *
 * Walking across the seam between two outdoor scenes does not come through
 * here. Nothing about the camera changes there and nothing should. */
S32 FollowCamCarryIntoScene(void) {
    T_OBJ_3D *ptr3d = &ListObjet[NumObjFollow].Obj;

    /* Nor in a cutscene: a scene change in the middle of one is the script's,
     * and gets the centring it was written against. */
    if (!FollowCamSceneCarry OR !FollowCamera OR CubeMode != CUBE_EXTERIEUR OR FlagLoadGame OR CinemaMode)
        return FALSE;

    /* Latched state describes the scene just left. CameraCenter(1) and (2) drop
     * an orbit in flight for the same reason, and a re-engage countdown left
     * running would keep the camera from following the hero's first turn here. */
    FollowCamForgetManualGesture();
    FollowCamReengageDelay = 0;

    /* LastCubeMode is still the scene just left: ChangeCube sets it on the way
     * in and nothing touches it until the next change. */
    if (LastCubeMode != CUBE_EXTERIEUR)
        AddBetaCam = 0;

    /* The pitch that goes into the new scene is the camera's own. What AlphaCam
     * holds may be a shot's, or the obstruction search's answer for a place the
     * hero has now left, and neither was the player's to carry. */
    if (FollowCamOwnPitch() >= 0)
        AlphaCam = FollowCamOwnPitch();

    Rotate(0, DISTANCE_VISEE, (AddBetaCam + ptr3d->Beta) & 4095);

    VueOffsetX = ptr3d->X + X0;
    VueOffsetY = ptr3d->Y;
    VueOffsetZ = ptr3d->Z + Z0;

    StartXCube = VueOffsetX / SIZE_BRICK_XZ;
    StartYCube = VueOffsetY / SIZE_BRICK_Y;
    StartZCube = VueOffsetZ / SIZE_BRICK_XZ;

    BetaCam = FollowCamTargetBetaFor(AddBetaCam, ptr3d->Beta);
    FollowCamSyncTarget();

    CameraCenter(3);

    /* An arrival is a cut, so the obstruction clearance starts at what this
     * place needs. */
    FollowCamObstructSettle();

    /* Have the update run on this frame whatever the hero's new position
     * happens to be, so the arm length and the clearance are its own. */
    FollowCamPrevAlphaCam = -1;

    return TRUE;
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
    FollowCamObstructExtra = 0;
    FollowCamObstructTarget = 0;
    FollowCamObstructFloor = 0;
    FollowCamObstructSettle();
}

void FollowCamNoteHidden(S32 what) {
    if (what > FollowCamHiddenSeen)
        FollowCamHiddenSeen = what;
}

S32 FollowCamObstructOwnsSearch(void) {
    /* Not in a cutscene: its re-aims and scene changes are the script's, and
     * they get the search they were written against. */
    return FollowCamObstruct AND FollowCamera AND CubeMode == CUBE_EXTERIEUR AND !CinemaMode;
}

void FollowCamObstructShotFired(void) {
    /* A shot starts as written. Whatever tilt the last view needed, and any
     * floor a hidden hero taught it, belonged to that view. */
    FollowCamObstructExtra = 0;
    FollowCamObstructTarget = 0;
    FollowCamObstructFloor = 0;
}

S32 FollowCamRenderExtraPitch(void) {
    /* Not on a frame a zone holds the view, nor in a cutscene: both set the
     * pitch they mean, and the follow update that keeps the extra current does
     * not run in either. */
    if (!FollowCamObstructOwnsSearch() OR CameraZone OR CinemaMode)
        return 0;

    return FollowCamObstructExtra;
}

void FollowCamObstructSettle(void) {
    /* Nothing to settle with the setting off, and a flag raised then would keep
     * the dirty check true until an update came along to clear it. */
    if (FollowCamObstruct)
        FollowCamObstructSnap = TRUE;
}

S32 FollowCamObstructHidden(void) {
    T_OBJ_3D *ptr3d = &ListObjet[NumObjFollow].Obj;

    if (!FollowCamObstructOwnsSearch())
        return FALSE;

    if (FollowCamObstructExtra >= FollowCamObstructTarget) {
        /* The clearance has given all that was asked of it and he is hidden
         * all the same: the search does not see what is in the way here. The
         * render does, so its word is taken and the view is raised a step,
         * and held there for as long as he stays near. Only when there is no
         * pitch left to give is the classic recentre the answer. */
        S32 floor = FollowCamObstructExtra + FOLLOW_CAM_OBSTRUCT_HIDDEN_STEP;

        if (AlphaCam + floor > FOLLOW_CAM_OBSTRUCT_PITCH_MAX)
            return FALSE;

        FollowCamObstructFloor = floor;
        FollowCamObstructFloorX = ptr3d->X;
        FollowCamObstructFloorZ = ptr3d->Z;
        FollowCamObstructTarget = floor;
        FollowCamObstructHurrying = 2;
        FollowCamNoteHidden(2);
        return TRUE;
    }

    FollowCamObstructHurrying = 1;
    FollowCamNoteHidden(2);
    return TRUE;
}

void FollowCamSyncTarget(void) {
    FollowCamTargetBeta = BetaCam;
    /* The caller has just put the camera somewhere with a cut. */
    FollowCamEaseRestart();
}

void FollowCamWorldAnchorRebase(void) {
    FollowCamAnchorHeroBeta = ListObjet[NumObjFollow].Obj.Beta & 4095;
}

void FollowCamLendToShot(S32 shotBeta) {
    if (!FollowCamZoneReturn AND !FollowCamZoneAnchor)
        return;

    /* Per axis, and only if it is not already out. A second shot firing while
     * the first still has the camera, or while it is on its way back, changes
     * what is on screen but not what is owed: that is still whatever the Auto
     * camera had before the first one. An axis the player has since set by hand
     * is theirs, so it is lent afresh from where they left it. */
    if (!(FollowCamLoan & FOLLOW_CAM_LOAN_YAW)) {
        /* Straight out of a cutscene the pan is whatever its last shot left,
         * which nobody chose: what is owed then is behind the hero. */
        FollowCamLentPan = FollowCamWasCinema ? 0 : (AddBetaCam & 4095);
        /* The facing the pan was last measured from: the hero may have turned
         * since in this step's object loop, which the update has not seen. */
        FollowCamLentHeroBeta = FollowCamWasCinema ? (ListObjet[NumObjFollow].Obj.Beta & 4095) : FollowCamAnchorHeroBeta;
        FollowCamLoan |= FOLLOW_CAM_LOAN_YAW;
    }
    if (!(FollowCamLoan & FOLLOW_CAM_LOAN_PITCH)) {
        /* The camera's own pitch, and not whatever AlphaCam holds as the shot
         * fires. A door's recentre runs the obstruction search on the arrival
         * frame, just ahead of the zone over that door, and the search leaves a
         * pitch of its own (767 at the Tavern, above anything the player can
         * tilt to). Lent as found, that is what the return would hand back. */
        if (FollowCamOwnAlpha < 0)
            FollowCamOwnAlpha = AlphaCam; /* nothing better is known yet */
        FollowCamLentAlpha = FollowCamOwnAlpha;
        FollowCamLoan |= FOLLOW_CAM_LOAN_PITCH;
    }
    FollowCamLoan &= ~FOLLOW_CAM_LOAN_RETURNING;
    FollowCamShotBeta = shotBeta & 4095;
}

void FollowCamNoteOwnPitch(S32 alpha) {
    FollowCamOwnAlpha = alpha;
}

S32 FollowCamOwnPitch(void) {
    return FollowCamOwnAlpha;
}

void FollowCamPlayerTook(S32 yaw, S32 pitch) {
    if (yaw)
        FollowCamLoan &= ~FOLLOW_CAM_LOAN_YAW;
    if (pitch) {
        FollowCamLoan &= ~FOLLOW_CAM_LOAN_PITCH;
        FollowCamOwnAlpha = AlphaCam; /* callers tilt and clamp first, so this is where they left it */
    }
    if (!(FollowCamLoan & (FOLLOW_CAM_LOAN_YAW | FOLLOW_CAM_LOAN_PITCH)))
        FollowCamLoan = 0;
}

void FollowCamForgetLoan(void) {
    FollowCamLoan = 0;
    /* The same moments are cuts for the eases paced by the clock, and a load
     * among them has just set that clock from the save. */
    FollowCamEaseRestart();
}

void FollowCamNoteFrame(void) {
    FollowCamFrame++;
    FollowCamEaseDt = 0;
}

void FollowCamNoteCinema(S32 cinema) {
    /* The first frame after a cutscene. A shot that fired on it, over the spot
     * the script left the hero at, was lent the cutscene's leftover pan; what
     * it owes back is behind him, as it is after a building. */
    if (FollowCamWasCinema AND !cinema AND(FollowCamLoan & FOLLOW_CAM_LOAN_YAW)) {
        FollowCamLentPan = 0;
        FollowCamLentHeroBeta = ListObjet[NumObjFollow].Obj.Beta & 4095;
    }

    FollowCamWasCinema = cinema;
}

void FollowCamNoteScene(S32 exterior) {
    S32 cameOut = (exterior AND FollowCamWasIndoors);

    FollowCamWasIndoors = !exterior;

    /* Coming out of a building, what a shot owes back for the yaw is "behind the
     * hero", whatever pan it was lent at.
     *
     * The pan is an offset from his facing, and he comes out facing the other
     * way, so one that put the camera beside him going in puts it in front of
     * him coming out. That matters most for a pan the player set while the shot
     * over a door had the camera: it was made to look round that scene, the
     * zone lends it afresh as he steps back out, and returned as it stood it
     * left the camera in front of him for as long as he kept walking. Played by
     * the maintainer at the Tavern door, 159 degrees off for thirty seconds.
     *
     * Only what is owed changes. The angle on screen is the shot's until he
     * leaves its box, and a pan with no shot involved is not this function's
     * business: what a door does to that belongs to the scene change. */
    if (cameOut AND(FollowCamLoan & FOLLOW_CAM_LOAN_YAW)) {
        FollowCamLentPan = 0;
        FollowCamLentHeroBeta = ListObjet[NumObjFollow].Obj.Beta & 4095;
    }
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
     FOLLOW_CAM_REENGAGE_FRAMES_DEFAULT, 0, 600, NULL, NULL, NULL, NULL,
     "Frames before auto-cam re-centers after a manual pan"},
    {"FollowCamDriftDivisor", NULL, "cam_recenter_drift", &FollowCamDriftDivisor, SETTING_CLAMP,
     FOLLOW_CAM_DRIFT_DIVISOR_DEFAULT, 1, 200, NULL, NULL, NULL, NULL,
     "Auto-cam re-center drift divisor (higher = gentler)"},
    {"FollowCamSceneCarry", NULL, "cam_scene_carry", &FollowCamSceneCarry, SETTING_TRUTHY,
     FOLLOW_CAM_SCENE_CARRY_DEFAULT, 0, 1, NULL, NULL, NULL, NULL,
     "Auto cam: start behind the hero in a new outdoor scene, pitch kept"},
    {"FollowCamZoneSkip", NULL, "cam_zone_skip", &FollowCamZoneSkip, SETTING_TRUTHY,
     FOLLOW_CAM_ZONE_SKIP_DEFAULT, 0, 1, NULL, NULL, NULL, NULL,
     "Auto cam: leave scripted shots that only latch unfired while steering"},
    {"FollowCamObstruct", NULL, "cam_obstruct", &FollowCamObstruct, SETTING_TRUTHY,
     FOLLOW_CAM_OBSTRUCT_DEFAULT, 0, 1, NULL, NULL, NULL, NULL,
     "Auto cam: eased obstruction clearance in place of the classic search"},
    {"FollowCamObstructHurryDivisor", NULL, "cam_obstruct_hurry", &FollowCamObstructHurryDivisor, SETTING_CLAMP,
     FOLLOW_CAM_OBSTRUCT_HURRY_DEFAULT, 1, FOLLOW_CAM_OBSTRUCT_EASE, NULL, NULL, NULL, NULL,
     "cam_obstruct: ease divisor while the hero is hidden (1 = cut, 10 = no hurry)"},
    {"FollowCamHDRecompose", NULL, "cam_hd", &FollowCamHDRecompose, SETTING_TRUTHY,
     FOLLOW_CAM_HD_RECOMPOSE_DEFAULT, 0, 1, NULL, NULL, NULL, NULL,
     "Auto cam HD recompose: pull in + steepen at tall render heights"},
    {"FollowCamHDExcessCap", NULL, "cam_hd_cap", &FollowCamHDExcessCap, SETTING_CLAMP,
     FOLLOW_CAM_HD_EXCESS_CAP_DEFAULT, 0, SETTING_MAX_NONE, NULL, NULL, NULL, NULL,
     "Cap on recompose strength so tall res converges (0 = pure linear; 70 ~= 816p)"},
    {"FollowCamHDPitchGain", NULL, "cam_hd_pitch", &FollowCamHDPitchGain, SETTING_CLAMP,
     FOLLOW_CAM_HD_PITCH_GAIN_DEFAULT, SETTING_MIN_NONE, SETTING_MAX_NONE, NULL, NULL, NULL, NULL,
     "HD recompose pitch gain (AlphaCam units per 1.0 of k-1)"},
    {"FollowCamHDDistGain", NULL, "cam_hd_dist", &FollowCamHDDistGain, SETTING_CLAMP,
     FOLLOW_CAM_HD_DIST_GAIN_DEFAULT, SETTING_MIN_NONE, SETTING_MAX_NONE, NULL, NULL, NULL, NULL,
     "HD recompose distance pull (% of boom per 1.0 of k-1)"},
    {"FollowCamHDLeanGain", NULL, "cam_hd_lean", &FollowCamHDLeanGain, SETTING_CLAMP,
     FOLLOW_CAM_HD_LEAN_GAIN_DEFAULT, SETTING_MIN_NONE, SETTING_MAX_NONE, NULL, NULL, NULL, NULL,
     "HD recompose forward-lean cut (% per 1.0 of k-1)"},
    {"FollowCamGround", NULL, "cam_ground", &FollowCamGround, SETTING_TRUTHY,
     FOLLOW_CAM_GROUND_DEFAULT, 0, 1, NULL, NULL, NULL, NULL,
     "Auto cam: smooth terrain ground-clearance lift"},
    /* The ceiling keeps (g + clearance - hy) * 100 inside S32 in the occlusion
     * march below, which is why it is stated with the setting rather than at the
     * one surface that used to check it. */
    {"FollowCamGroundClearance", NULL, "cam_ground_clear", &FollowCamGroundClearance, SETTING_CLAMP,
     FOLLOW_CAM_GROUND_CLEARANCE_DEFAULT, 0, 20000, NULL, NULL, NULL, NULL,
     "Ground clearance: keep the eye this far above the ground under it"},
    {"FollowCamManualHoldFrames", NULL, "cam_manual_hold", &FollowCamManualHoldFrames, SETTING_CLAMP,
     FOLLOW_CAM_MANUAL_HOLD_DEFAULT, 0, FOLLOW_CAM_MANUAL_HOLD_MAX, NULL, NULL, NULL, NULL,
     "Frames the pitch/distance auto-assist yields after a manual camera nudge"},
    {"FollowCamHoldAngle", NULL, "cam_hold_angle", &FollowCamHoldAngle, SETTING_TRUTHY,
     FOLLOW_CAM_HOLD_ANGLE_DEFAULT, 0, 1, NULL, NULL, NULL, NULL,
     "Hold the manual camera rotation (free-camera); off = classic auto-recenter drift"},
    {"FollowCamZoneReturn", NULL, "cam_zone_return", &FollowCamZoneReturn, SETTING_TRUTHY,
     FOLLOW_CAM_ZONE_RETURN_DEFAULT, 0, 1, NULL, NULL, NULL, NULL,
     "Auto cam: give a scripted shot's yaw and pitch back when the hero leaves its box"},
    {"FollowCamZoneAnchor", NULL, "cam_zone_anchor", &FollowCamZoneAnchor, SETTING_TRUTHY,
     FOLLOW_CAM_ZONE_ANCHOR_DEFAULT, 0, 1, NULL, NULL, NULL, NULL,
     "Prototype: hold a latching shot's world yaw while the hero is inside its box"},
    {"FollowCamWorldAnchor", NULL, "cam_world_anchor", &FollowCamWorldAnchor, SETTING_TRUTHY,
     FOLLOW_CAM_WORLD_ANCHOR_DEFAULT, 0, 1, NULL, NULL, NULL, NULL,
     "Prototype: hold the camera's world direction while the hero turns (off = it turns with him)"},
    {"FollowCamWorldReturn", NULL, "cam_world_return", &FollowCamWorldReturn, SETTING_CLAMP,
     FOLLOW_CAM_WORLD_RETURN_DEFAULT, 0, FOLLOW_CAM_WORLD_RETURN_MAX, NULL, NULL, NULL, NULL,
     "cam_world_anchor with cam_hold_angle 0: units walked forward that pace the return behind the hero (0 = none)"},
    {"FollowCamEaseMs", NULL, "cam_ease_ms", &FollowCamEaseMs, SETTING_CLAMP,
     FOLLOW_CAM_EASE_MS_DEFAULT, 0, FOLLOW_CAM_EASE_MS_MAX, NULL, NULL, NULL, NULL,
     "Auto cam: time constant of the turn behind the hero, ms (0 = per-frame lerp, 300 = retail, 600 = the lerp at 60 fps)"},
    {"FollowCamTargetMs", NULL, "cam_target_ms", &FollowCamTargetMs, SETTING_CLAMP,
     FOLLOW_CAM_TARGET_MS_DEFAULT, 0, FOLLOW_CAM_EASE_MS_MAX, NULL, NULL, NULL, NULL,
     "Auto cam: time constant of the look-at point trailing the hero, ms (0 = pinned to him, 300 = retail)"},
    /* cvar only, and it must stay that way. FollowCamera's cfg row lives in
       CONFIG_FILE.CPP's table, which reads it with the legacy AutoCameraCenter
       fallback and its own rule; giving this row a key would read the same
       global twice under two rules, and this table runs last. */
    {NULL, NULL, "cam_follow", &FollowCamera, SETTING_TRUTHY, FALSE, 0, 1, NULL, NULL, NULL, NULL,
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

const T_SETTING *FollowCam_Settings(S32 *count) {
    *count = FOLLOW_CAM_SETTING_COUNT;
    return FollowCamSettings;
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
