/*
 * Runtime resolution switching — orchestrator.  See RES_SWITCH.H + the design
 * doc at docs/RUNTIME_RESOLUTION.md.
 */

#include "RES_SWITCH.H"

#include "C_EXTERN.H"
#include "FLOW.H"
#include "GAMEMENU.H"
#include "INTEXT.H" /* CameraCenter, SaveCamera / RestoreCamera */
#include "INVENT.H" /* BackupScreen / RestoreScreen */
#include "PTRFUNC.H"

#include <3D/CAMERA.H>
#include <SVGA/CLIP.H>
#include <SVGA/DIRTYBOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/STRING.H>
#include <SYSTEM/TIMER.H>
#include <SYSTEM/WINDOW.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

#include "INPUT.H"

#include <stdio.h>
#include <string.h>

/* Same validator the --resolution CLI uses (see CONTROL.CPP).  Kept local for
   now to avoid yet another shared header; if a third caller appears (the menu)
   we'll lift these out. */
#define RES_SWITCH_MIN_X 320
#define RES_SWITCH_MIN_Y 200
#define RES_SWITCH_MAX_X 1920
#define RES_SWITCH_MAX_Y 1024

static bool ValidateRes(U32 w, U32 h) {
    if (w < RES_SWITCH_MIN_X || w > RES_SWITCH_MAX_X)
        return false;
    if (h < RES_SWITCH_MIN_Y || h > RES_SWITCH_MAX_Y)
        return false;
    if ((w & 7) != 0)
        return false;
    return true;
}

static void TeardownEngineBuffers() {
    /* Order matters: Screen is a slice of MainBuffer (ListMem[MEM_SCREEN]),
       not an independent allocation, but EndScreen() tries to Free() it.
       Mem_TeardownMainBuffer nulls the pointer first, so EndScreen's
       `if (Screen)` guard makes the would-be Free(InteriorPointer) a no-op.
       EndScreen still has to run because Log IS independently malloc'd by
       CreateScreenMemory and needs its own free. */
    Mem_TeardownMainBuffer();
    EndScreen();
}

static bool RebuildEngineBuffers(U32 w, U32 h) {
    Mem_ConfigureScreenBuffers(w, h);
    InitMainBuffer();
    if (!CreateScreenMemory(w, h))
        return false;
    return true;
}

static bool ResizeSdlSurfaces(U32 w, U32 h) {
    /* Resize in place rather than destroy+recreate.  The SDL window itself
       (and therefore fullscreen state, focus, and the player's window
       position in windowed mode) survives the switch.  Only the streaming
       texture and the Phys present-buffer change dimensions. */
    if (!ResizeWindowSurface(w, h))
        return false;
    if (!ResizeVideoSurface(w, h))
        return false;
    /* Clip window MUST be set after ResizeVideoSurface — SetClipWindow
       clamps to ModeResX / ModeResY, which is the video-surface size, not
       the Log size.  If we set the clip with the old ModeResX still in
       place (as the boot path can get away with because everything inits
       in one shot), ClipWindowXMax sticks at old_ModeResX-1 and every
       subsequent draw is clipped to the previous resolution's width. */
    SetClipWindow(0, 0, (S32)w - 1, (S32)h - 1);
    SetClip(0, 0, (S32)w - 1, (S32)h - 1);
    return true;
}

bool Res_Switch(U32 newW, U32 newH) {
    if (!ValidateRes(newW, newH)) {
        LogPrintf("Res_Switch: invalid target %ux%u (need %d..%dx%d..%d, W%%8==0)\n",
                  newW, newH, RES_SWITCH_MIN_X, RES_SWITCH_MAX_X,
                  RES_SWITCH_MIN_Y, RES_SWITCH_MAX_Y);
        return false;
    }

    /* If we're already at the requested mode, this is a no-op: tearing
       everything down just to put it back wastes time and would still drop
       a frame.  The caller (console verb / menu) can rely on this. */
    if (ModeDesiredX == newW && ModeDesiredY == newH) {
        return true;
    }

    const U32 oldW = ModeDesiredX;
    const U32 oldH = ModeDesiredY;
    const bool wasFullscreen = GetWindowFullscreen();

    TeardownEngineBuffers();

    if (!RebuildEngineBuffers(newW, newH) || !ResizeSdlSurfaces(newW, newH)) {
        LogPrintf("Res_Switch: rebuild at %ux%u failed, reverting to %ux%u\n",
                  newW, newH, oldW, oldH);
        TeardownEngineBuffers();
        if (!RebuildEngineBuffers(oldW, oldH) || !ResizeSdlSurfaces(oldW, oldH)) {
            /* The previous mode worked moments ago, so this should be
               impossible.  If it does happen we have no surface to fall back
               to — the engine is wedged.  TheEnd() is the existing fail-fast
               for unrecoverable memory states. */
            TheEnd(NOT_ENOUGH_MEM, "Res_Switch revert");
            return false;
        }
        /* Cube reload at the old W,H — the per-cube buffers in MainBuffer
           were zeroed by the teardown and need their contents back before
           the next AffScene() runs. */
        if (PtrInitGrille && NumCube >= 0)
            PtrInitGrille((U32)NumCube);
        return false;
    }

    /* Repopulate the per-cube buffers in the freshly-allocated MainBuffer.
       InitGrille reloads the cube's grille header into BufMap, the block
       list into TabBlock, the brick graphics into BufferBrick, builds
       BufferMaskBrick from those, and finally fills BufCube via
       CopyMapToCube.  Player position, animation state, and the rest of
       ListObjet live outside MainBuffer and are untouched. */
    if (PtrInitGrille && NumCube >= 0)
        PtrInitGrille((U32)NumCube);

    /* PtrXplPalette is also in MainBuffer, and a long chain of palette /
       fog / CLUT pointers (PtrNuances, PtrCLUTFog, PtrCLUTGouraud,
       PtrPalCurrent, PtrTransPal) cache offsets into it.  ChoicePalette
       re-derives them all, but it short-circuits the HQR reload when
       LastIdxPalette matches the current one.  Invalidate it so the
       reload runs and PtrXplPalette gets refilled. */
    LastIdxPalette = -1;
    ChoicePalette();

    /* Push the freshly-derived PtrPal into the SDL palette + paletteLUT.
       Without this the next frame's index-to-RGB mapping uses whatever the
       SDL palette held before the switch, which mostly works (palette data
       is unchanged) but the dialog's text colours come out wrong until the
       next AffScene-driven Palette() call refreshes them — visible as a
       garbage-colour dialog that snaps to the correct palette the instant
       the player presses Y/Enter and the main loop resumes drawing. */
    Palette(PtrPal);

    /* Re-anchor the 3D projection origin to the new framebuffer centre.
       Init3DExtView / Init3DView read ModeDesiredX/Y to compute XCentre /
       YCentre at scene-init time; they're not re-evaluated each frame, so
       at runtime-switch time the camera still projects to the old (320,
       240) and the scene renders into the left-top corner of the wider
       framebuffer — visible as the "window grew but the game image
       didn't". Calling the dispatch pointer re-runs whichever Init3D*View
       matches the current cube mode (interior iso vs exterior camera). */
    if (PtrInit3DView)
        PtrInit3DView();

    /* Init3DView (the interior dispatch) also does SetPosCamera(0, 0, 0),
       which leaves the iso camera at the world origin instead of the
       player's current position — visible as "characters out of position
       vs the scene" after a switch on iso cubes. CameraCenter(0) re-
       applies VueOffsetX/Y/Z via SetPosCamera. This is the same fixup
       MenuComportement does on exit; exterior scenes don't need it
       (Init3DExtView calls SetFollowCamera with the saved offsets). */
    if (CubeMode == CUBE_INTERIEUR)
        CameraCenter(0);

    /* InitPartFlow caches MAX_FLOWS pointers into ListFlowDots (also in
       MainBuffer) in ListPartFlow[i].PtrListDot at boot; those go stale
       when teardown frees the buffer.  Re-running it rewires them to the
       new ListFlowDots and zeroes NbDot for every slot, which also resets
       any in-flight particle flow from before the switch — acceptable
       cost (a quick flash of dust effects vanishing) for a switch
       that's expected to be rare. */
    InitPartFlow();

    /* Engine renders in dirty-box mode: AffScene only redraws regions that
       changed since the last frame, assuming the rest of Log carries over.
       After teardown the new Log is zero-initialised, so without a full
       redraw the next frame is mostly black. AFF_ALL_FLIP is the engine's
       "this is the first frame, redraw everything" sentinel — same value
       MainLoop seeds on every restartloop entry. */
    FirstTime = AFF_ALL_FLIP;

    const bool isFullscreen = GetWindowFullscreen();
    LogPrintf("Res_Switch: %ux%u -> %ux%u OK (fullscreen %d -> %d)\n",
              oldW, oldH, newW, newH, (int)wasFullscreen, (int)isFullscreen);
    /* Fullscreen state must round-trip cleanly; SDL_SetWindowSize on the
       existing window doesn't take the window out of fullscreen. If this
       assertion ever trips, the new code path destroyed the window somewhere
       it shouldn't have. */
    if (wasFullscreen != isFullscreen) {
        fprintf(stderr,
                "Res_Switch: WARNING fullscreen state changed across switch "
                "(was=%d now=%d) -- texture resize path is supposed to "
                "preserve it\n",
                (int)wasFullscreen, (int)isFullscreen);
    }
    return true;
}

/*══════════════════════════════════════════════════════════════════════════*
 *   Safety gate                                                             *
 *══════════════════════════════════════════════════════════════════════════*/

const char *Res_SwitchAllowedReason(void) {
    /* Refuse mid-cinematic: the Smacker decoder owns the framebuffer and
       partial state would be lost. */
    if (FlagPlayAcf)
        return "Not now: a cinematic is playing — wait for it to finish.";
    /* Refuse before the first scene loads. PtrInitGrille has nothing to
       reload, and the engine isn't in a steady main-loop state yet. */
    if (PtrScene == NULL || NumCube < 0)
        return "Not now: no scene loaded yet.";
    /* Holomap is *not* gated here despite the design doc listing it as a
       safety gate — HoloMap() owns its own modal frame loop while open, so
       MainLoop (and therefore Res_RunRevertDialogIfPending) doesn't tick
       until the player closes it. Gating on HoloMode would be wrong: that's
       0=Globe / 1=Plan, not an "open" sentinel. The genuine
       holomap-is-active flag would need to be added in HOLOGLOB.CPP first;
       deferred to the menu UX work in Phase 2. */
    return NULL;
}

/*══════════════════════════════════════════════════════════════════════════*
 *   Revert dialog                                                           *
 *══════════════════════════════════════════════════════════════════════════*/

/* The 15 s figure mirrors what modern OSes use for display-mode preview
   ("revert if not confirmed"). Long enough to let the player realise the
   new mode is unreadable; short enough that an unattended PC reverts soon. */
#define RES_REVERT_TIMEOUT_MS 15000u
#define RES_REVERT_TICK_MS 100u /* poll cadence inside the modal */

static bool s_revertPending = false;
static U32 s_revertPrevW = 0;
static U32 s_revertPrevH = 0;
static U32 s_revertNewW = 0; /* what the user picked (for the "now at" line) */
static U32 s_revertNewH = 0;

void Res_BeginRevertCountdown(U32 prevW, U32 prevH) {
    s_revertPending = true;
    s_revertPrevW = prevW;
    s_revertPrevH = prevH;
    s_revertNewW = ModeDesiredX;
    s_revertNewH = ModeDesiredY;
}

bool Res_HasPendingRevert(void) {
    return s_revertPending;
}

void Res_CancelPendingRevert(void) {
    s_revertPending = false;
}

static void RevertDialog_Draw(U32 secondsLeft) {
    /* Full-screen darken so the message reads cleanly against the just-
       rendered scene without needing its own panel art. Same SCREEN_SHADE_LVL
       (8) ShowSaveConfirmation uses for the in-game "Saved" box. */
    ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);

    const S32 cx = (S32)ModeDesiredX / 2;
    /* Five text lines, evenly spaced around the vertical centre. Spacing is
       picked to match DrawSingleString's ~20px line height across the modes
       we ship at. */
    const S32 lineH = 28;
    const S32 baseY = (S32)ModeDesiredY / 2 - 2 * lineH;

    char buf[128];
    char cp850[128];

    /* "Resolution changed" */
    CopyUtf8ToCp850(cp850, sizeof cp850, GetLocalizedResDialogLabel(RES_DIALOG_TITLE));
    DrawSingleString(cx, baseY + 0 * lineH, cp850);

    /* "Now running at WxH." */
    snprintf(buf, sizeof buf, GetLocalizedResDialogLabel(RES_DIALOG_NOW_AT),
             (unsigned)s_revertNewW, (unsigned)s_revertNewH);
    CopyUtf8ToCp850(cp850, sizeof cp850, buf);
    DrawSingleString(cx, baseY + 1 * lineH, cp850);

    /* "Keep this resolution?" */
    CopyUtf8ToCp850(cp850, sizeof cp850, GetLocalizedResDialogLabel(RES_DIALOG_KEEP_Q));
    DrawSingleString(cx, baseY + 2 * lineH, cp850);

    /* "[Y] Keep      [N] Revert" */
    CopyUtf8ToCp850(cp850, sizeof cp850, GetLocalizedResDialogLabel(RES_DIALOG_KEEP_REVERT));
    DrawSingleString(cx, baseY + 3 * lineH, cp850);

    /* "Reverts automatically in Ns..." */
    snprintf(buf, sizeof buf, GetLocalizedResDialogLabel(RES_DIALOG_REVERT_IN),
             (int)secondsLeft);
    CopyUtf8ToCp850(cp850, sizeof cp850, buf);
    DrawSingleString(cx, baseY + 4 * lineH, cp850);

    BoxStaticAdd(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1);
    BoxStaticFullflip();
}

ResRevertChoice Res_RunRevertDialogIfPending(void) {
    if (!s_revertPending)
        return RES_REVERT_NONE;
    s_revertPending = false; /* claim the pending state up front so a recursive
                                Res_Switch on revert doesn't re-trigger us */

    /* Modal pattern lifted from MenuComportement (SOURCES/COMPORTE.CPP).
       That's the in-game behaviour-change menu and it handles palette,
       camera state, dirty-box invalidation, and full-screen repaint
       correctly across both 3D and iso scenes.

       Outline:
         entry: SaveTimer / SaveCamera / UnsetClip / Palette(PtrPal)
                paint a fresh AffScene into Log (post-switch Log is empty)
                BackupScreen(TRUE) snapshots that frame
         loop:  each redraw restores the background from Screen, then
                draws darken + 5 text lines on top
         exit:  RestoreScreen() drives a full-screen AffScene repaint,
                so dirty-box rendering picks up cleanly on the next tick
                RestoreCamera / RestoreTimer
                if Revert: Res_Switch back to (prev) */

    SaveTimer();
    SaveCamera();
    UnsetClip();
    /* PtrPal was already pushed to SDL by Res_Switch, but Palette() also
       triggers RebuildPaletteLUT which is cheap; belt + braces matches
       MenuComportement's "au cas ou l'on soit appelé pendant un eclair". */
    Palette(PtrPal);

    /* Drive a full repaint into Log before BackupScreen. The post-switch
       Log was just allocated and is zero — without this, the snapshot the
       dialog uses as its background would be blank.  BoxStaticAdd over
       the full frame plus AffScene(AFF_ALL_FLIP) is what MainLoop runs
       at restartloop entry, and it's the same shape the engine uses when
       it needs "definitely repaint everything". */
    BoxReset();
    BoxStaticAdd(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1);
    AffScene(AFF_ALL_FLIP);

    BackupScreen(TRUE); /* Screen → ScreenAux, then Log → Screen */

    ManageTime();
    const U32 startMs = TimerRefHR;
    const U32 endMs = startMs + RES_REVERT_TIMEOUT_MS;
    ResRevertChoice choice = RES_REVERT_REVERT; /* default if we time out */
    U32 lastSecondsDrawn = (U32)-1;

    /* Drain stale input so a key still held from before doesn't immediately
       pick a side. */
    InitWaitNoKey();

    while (TimerRefHR < endMs) {
        /* Under --fixed-dt the harness pins time to the per-tick advance done
           in Control_TickHook; ManageTime() called from a modal inner loop
           would see the same virtual time forever and the dialog would never
           time out. Timer_FixedDtPump() is the documented "non-presenting
           wait loop owns every iteration's time" helper; it's a no-op in
           normal (wall-clock) play, where ManageTime() already advances
           TimerRefHR via SDL_GetTicks(). */
        Timer_FixedDtPump();
        ManageTime();
        MyGetInput();

        if (MyKey == K_Y || MyKey == K_O || MyKey == K_J || MyKey == K_S ||
            MyKey == K_ENTER || MyKey == K_SPACE) {
            choice = RES_REVERT_KEEP;
            break;
        }
        if (MyKey == K_N || MyKey == K_ESC) {
            choice = RES_REVERT_REVERT;
            break;
        }

        const U32 remainingMs = endMs - TimerRefHR;
        const U32 secondsLeft = (remainingMs + 999u) / 1000u;
        if (secondsLeft != lastSecondsDrawn) {
            /* Re-fill Log from the Screen backup so the previous countdown
               digit doesn't leave ghost pixels and the scene stays visible
               under the darken. */
            CopyScreen(Log, Screen);
            RevertDialog_Draw(secondsLeft);
            lastSecondsDrawn = secondsLeft;
        }
    }

    LogPrintf("Res_Switch: revert dialog -> %s (target was %ux%u)\n",
              choice == RES_REVERT_KEEP ? "KEEP" : "REVERT",
              (unsigned)s_revertNewW, (unsigned)s_revertNewH);

    /* Restore engine state regardless of choice. RestoreScreen drives a
       full-screen AffScene(AFF_OBJETS_NO_FLIP) + BoxBlit so dirty-box
       rendering picks up from a known-good frame on the next tick. */
    RestoreScreen();
    RestoreCamera();
    RestoreTimer();

    if (choice == RES_REVERT_REVERT) {
        /* Recursive Res_Switch to the previous (W, H). The orchestrator
           handles the no-op case (already at prev W,H) cleanly. */
        Res_Switch(s_revertPrevW, s_revertPrevH);
    }

    return choice;
}
