/*
 * Runtime resolution switching — orchestrator.  See RES_SWITCH.H + the design
 * doc at docs/RUNTIME_RESOLUTION.md.
 */

#include "RES_SWITCH.H"

#include "ANIMTEX.H" /* InitPlasmaMenu, DoTextureAnimation (plasma fire bar) */
#include "C_EXTERN.H"
#include "FLOW.H"
#include "GAMEMENU.H"
#include "INTEXT.H" /* CameraCenter, SaveCamera / RestoreCamera */
#include "INVENT.H" /* BackupScreen / RestoreScreen / DrawCadre / ALL_ANGLES */
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

/* Panel + button geometry — fixed physical size, centred for any resolution.
   The 520x220 panel comfortably fits 3 text lines + 2 buttons at the engine's
   ~16px font height with breathing room. */
#define RES_DIALOG_PANEL_W 520
#define RES_DIALOG_PANEL_H 220
#define RES_DIALOG_BTN_W 170
#define RES_DIALOG_BTN_H 42
#define RES_DIALOG_BTN_GAP 30

typedef enum {
    RES_BTN_REVERT = 0, /* left, default focus (matches Windows/macOS/Steam) */
    RES_BTN_KEEP = 1    /* right */
} ResDialogBtn;

static void RevertDialog_PanelRect(S32 *x0, S32 *y0, S32 *x1, S32 *y1) {
    const S32 cx = (S32)ModeDesiredX / 2;
    const S32 cy = (S32)ModeDesiredY / 2;
    *x0 = cx - RES_DIALOG_PANEL_W / 2;
    *y0 = cy - RES_DIALOG_PANEL_H / 2;
    *x1 = *x0 + RES_DIALOG_PANEL_W - 1;
    *y1 = *y0 + RES_DIALOG_PANEL_H - 1;
}

static void RevertDialog_ButtonRect(ResDialogBtn b, S32 *bx0, S32 *by0, S32 *bx1, S32 *by1) {
    const S32 cx = (S32)ModeDesiredX / 2;
    S32 px0, py0, px1, py1;
    RevertDialog_PanelRect(&px0, &py0, &px1, &py1);
    const S32 by_top = py1 - 33 - RES_DIALOG_BTN_H + 1; /* 33px bottom margin */
    if (b == RES_BTN_REVERT) {
        *bx0 = cx - RES_DIALOG_BTN_GAP / 2 - RES_DIALOG_BTN_W;
    } else {
        *bx0 = cx + RES_DIALOG_BTN_GAP / 2;
    }
    *bx1 = *bx0 + RES_DIALOG_BTN_W - 1;
    *by0 = by_top;
    *by1 = by_top + RES_DIALOG_BTN_H - 1;
}

/* Renders the whole modal — panel + 3 text lines + 2 buttons + plasma on the
   focused button — into Log.  Caller is responsible for first restoring the
   background (CopyScreen Log<-Screen) and presenting (BoxStaticFullflip)
   after this returns. */
static void RevertDialog_Draw(ResDialogBtn focused, U32 secondsLeft) {
    /* Whole-screen darken so the panel reads on any underlying scene. */
    ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);

    /* Panel: a second shade level for extra contrast against the scene,
       then the rounded-corner cadre border. Same DrawCadre that
       DrawCadreConfirm uses for in-game confirmation dialogs. */
    S32 px0, py0, px1, py1;
    RevertDialog_PanelRect(&px0, &py0, &px1, &py1);
    ShadeBoxBlk(px0, py0, px1, py1, SCREEN_SHADE_LVL);
    DrawCadre(px0, py0, px1, py1, ALL_ANGLES);

    const S32 cx = (S32)ModeDesiredX / 2;
    char buf[128];
    char cp850[128];

    /* Three text lines, top-aligned within the panel. */
    CopyUtf8ToCp850(cp850, sizeof cp850, GetLocalizedResDialogLabel(RES_DIALOG_TITLE));
    DrawSingleString(cx, py0 + 30, cp850);

    snprintf(buf, sizeof buf, GetLocalizedResDialogLabel(RES_DIALOG_NOW_AT),
             (unsigned)s_revertNewW, (unsigned)s_revertNewH);
    CopyUtf8ToCp850(cp850, sizeof cp850, buf);
    DrawSingleString(cx, py0 + 65, cp850);

    CopyUtf8ToCp850(cp850, sizeof cp850, GetLocalizedResDialogLabel(RES_DIALOG_KEEP_Q));
    DrawSingleString(cx, py0 + 105, cp850);

    /* Two buttons side-by-side. Plasma highlight on the focused one
       (drawn first so the cadre border sits on top of the fire). */
    for (int b = 0; b < 2; b++) {
        S32 bx0, by0, bx1, by1;
        RevertDialog_ButtonRect((ResDialogBtn)b, &bx0, &by0, &bx1, &by1);

        if (focused == (ResDialogBtn)b) {
            /* DrawCadre(ALL_ANGLES) chamfers each corner with a 5-pixel
               diagonal (see DrawAngles in INVENT.CPP). A full-rect
               DrawFireBar leaves the chamfered corner triangles filled
               with plasma OUTSIDE the visible cadre line — barely
               noticeable on the engine's 550x50 menu items but very
               visible on our 170x42 buttons. Inset by 4 (just inside
               the chamfer) so the plasma matches the cadre's visible
               interior. The remaining 1-px halo is invisible at all
               supported scales. */
            DrawFireBar(bx0 + 4, by0 + 4, bx1 - 4, by1 - 4, FIRE_SELECT_MENU);
        }
        DrawCadre(bx0, by0, bx1, by1, ALL_ANGLES);

        if (b == RES_BTN_REVERT) {
            snprintf(buf, sizeof buf,
                     GetLocalizedResDialogLabel(RES_DIALOG_BTN_REVERT_FMT),
                     (int)secondsLeft);
        } else {
            snprintf(buf, sizeof buf, "%s",
                     GetLocalizedResDialogLabel(RES_DIALOG_BTN_KEEP));
        }
        CopyUtf8ToCp850(cp850, sizeof cp850, buf);
        /* Pass the button vertical centre directly to DrawSingleString.
           Its internal Font(..., y - 18, ...) is calibrated for the
           engine's HAUTEUR_STANDARD=50 menu items, but the *visible*
           glyph centre lands very close to the y arg in practice (as
           ShowSaveConfirmation relies on when it passes the screen
           centre for the "Game saved" splash). Earlier attempts at -8
           and +10 were both wrong (text above / below) — passing the
           plain centre is what looks right. */
        DrawSingleString((bx0 + bx1) / 2, (by0 + by1) / 2, cp850);
    }

    /* Mark only the panel rect dirty — the screen-wide darken + scene
       backup behind it are static for the modal's lifetime, so per-frame
       redraw doesn't have to walk the whole framebuffer. */
    BoxStaticAdd(px0 - 1, py0 - 1, px1 + 1, py1 + 1);
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

    /* Initialise the plasma/fire-bar texture (writes to SkySeaTexture). Without
       this the DrawFireBar call below renders whatever bytes were in that
       texture region. Same call MainGameMenu / OptionsMenu make before
       showing a plasma-highlighted menu. */
    InitPlasmaMenu();

    ManageTime();
    const U32 startMs = TimerRefHR;
    const U32 endMs = startMs + RES_REVERT_TIMEOUT_MS;
    ResRevertChoice choice = RES_REVERT_REVERT; /* default if we time out */
    ResDialogBtn focused = RES_BTN_REVERT;      /* default focus: the safe one */

    /* Drain stale input so a key still held from before doesn't immediately
       pick a side or toggle focus. */
    InitWaitNoKey();
    InitWaitNoInput(I_FIRE | I_ACTION | I_MENUS | I_LEFT | I_RIGHT);

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

        /* Directional input (D-pad on gamepad, arrow keys on keyboard, or
           the steering keys via the engine's input remap) toggles focus
           between the two buttons. */
        if (Input & I_LEFT)
            focused = RES_BTN_REVERT;
        if (Input & I_RIGHT)
            focused = RES_BTN_KEEP;

        /* Confirm selected. I_FIRE covers the action / inventory / return /
           comportement bindings (so the gamepad's A button works regardless
           of which keyboard mapping the player uses); Enter and Space are
           explicit keyboard shortcuts. */
        if ((Input & I_FIRE) || MyKey == K_ENTER || MyKey == K_SPACE) {
            choice = (focused == RES_BTN_KEEP) ? RES_REVERT_KEEP : RES_REVERT_REVERT;
            break;
        }

        /* Direct shortcuts (regardless of focus). Esc / B-button / N → Revert.
           Y/O (oui)/J (ja)/S (sí/sì/sim) → Keep. */
        if ((Input & I_MENUS) || MyKey == K_ESC || MyKey == K_N) {
            choice = RES_REVERT_REVERT;
            break;
        }
        if (MyKey == K_Y || MyKey == K_O || MyKey == K_J || MyKey == K_S) {
            choice = RES_REVERT_KEEP;
            break;
        }

        const U32 remainingMs = endMs - TimerRefHR;
        const U32 secondsLeft = (remainingMs + 999u) / 1000u;

        /* Per-frame redraw so the plasma animates smoothly. DoTextureAnimation
           advances the fire texture state; CopyScreen restores Log from the
           Screen backup so the modal paints over a clean copy of the
           captured scene (instead of compounding the per-frame ShadeBoxBlk
           darken on top of the previous frame's already-darkened pixels —
           which manifested as a flickering / progressively-black panel
           before this fix). CopyScreen(src, dst) — direction matters. */
        CopyScreen(Screen, Log);
        DoTextureAnimation();
        RevertDialog_Draw(focused, secondsLeft);
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
