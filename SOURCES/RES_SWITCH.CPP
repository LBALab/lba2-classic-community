/*
 * Runtime resolution switching — orchestrator.  See RES_SWITCH.H + the design
 * doc at docs/RUNTIME_RESOLUTION.md.
 */

#include "RES_SWITCH.H"

#include "ANIMTEX.H"     /* InitPlasmaMenu, DoTextureAnimation (plasma fire bar) */
#include "RES_CATALOG.H" /* RES_MIN_W / RES_MAX_W / RES_MIN_H / RES_MAX_H bounds */
#include "C_EXTERN.H"
#include "CONTROL.H"     /* Control_HasResolution / Get */
#include "DIRECTORIES.H" /* CFG_NAME */
#include "FLOW.H"
#include "GAMEMENU.H"
#include "INTEXT.H" /* CameraCenter, SaveCamera / RestoreCamera */
#include "INVENT.H" /* BackupScreen / RestoreScreen / DrawCadre / ALL_ANGLES */
#include "PERSO.H"  /* WriteConfigFile (persist on dialog Keep) */
#include "PTRFUNC.H"

#include <3D/CAMERA.H>
#include <SVGA/CLIP.H>
#include <SVGA/DIRTYBOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/DEFFILE.H> /* DefFileBufferInit / ReadValueDefault (boot cfg read) */
#include <SYSTEM/EXIT.H>    /* PathConfigFile (extern char[ADELINE_MAX_PATH]) */
#include <SYSTEM/FILES.H>   /* ExistsFileOrDir */
#include <SYSTEM/LOG.H>     /* Log_Debug (per-switch trace, off by default) */
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/RESOLUTION.H> /* RESOLUTION_X / RESOLUTION_Y compile-time defaults */
#include <SYSTEM/STRING.H>
#include <SYSTEM/TIMER.H>
#include <SYSTEM/WINDOW.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

#include "INPUT.H"

#include <stdio.h>
#include <string.h>

/* Bounds (RES_MIN_W / RES_MAX_W / RES_MIN_H / RES_MAX_H) and the multiple-of-8
   width rule live in RES_CATALOG.H: the same limits the --resolution CLI, the
   console verb, and the Display submenu all derive from. */

/* True when the live resolution came from auto-detect or the compile-time
   default, i.e. the player never explicitly chose it. WriteConfigFile skips
   persisting ResolutionX/Y in that case, so an auto resolution re-derives from
   the display every launch instead of freezing into the cfg on first exit.
   Cleared the moment the player picks a resolution explicitly: a successful
   Res_Switch (menu / console verb), or a CLI / cfg value at boot. Defined here
   so Res_Switch (below) and the boot resolver (further down) both see it. */
static bool g_resIsAutoDerived = false;
bool Res_ResolutionIsAutoDerived(void) { return g_resIsAutoDerived; }

/* Scratch-buffer size for the transient, heap-free boot reads of lba2.cfg
   (Res_LoadBootDimensions / Res_LoadBootFullscreen). A fully-populated valid
   cfg is already ~6 KB (72 keyboard + 72 gamepad bindings from MAX_INPUT, plus
   ~70 settings/camera/audio keys), so the old 8 KB left almost no headroom and
   any bloat -- a corrupt over-long line, future keys -- tips DefFileBufferInit
   into its "buffer too small" reject, dropping the cfg's ResolutionX/Y and
   silently falling back to auto-detect. 64 KB is generous and stays in BSS. */
#define RES_BOOT_CFG_BUFSZ (64 * 1024)

bool Res_ValidateDimensions(U32 w, U32 h) {
    if (w < RES_MIN_W || w > RES_MAX_W)
        return false;
    if (h < RES_MIN_H || h > RES_MAX_H)
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
    /* Dirty-box clip is a parallel system to the SVGA clip — BoxStaticFullflip
       (which `FirstTime = AFF_ALL_FLIP` ultimately triggers a full-frame
       redraw through) bounds its full-screen invalidation to BoxClipX/Y,
       not ClipWindowX/Y.  Without this call after a 640->768 switch the
       dirty-box clip stays at the old 640, the right strip [640..768) never
       gets marked dirty + blitted + presented, and shows whatever leftover
       pixels the SDL surface had after ResizeVideoSurface — visible as
       trailing / smearing along the new-width strip until a relaunch.
       The TRUE clean flag also makes the next BoxBlit zero the physical
       surface outside the clip, closing the gap on shrinks too. Mirrors
       the BoxInit call at DIRTYBOX.CPP:572. */
    BoxChangeClip(0, 0, (S32)w - 1, (S32)h - 1, TRUE);
    return true;
}

bool Res_Switch(U32 newW, U32 newH) {
    if (!Res_ValidateDimensions(newW, newH)) {
        LogPrintf("Res_Switch: invalid target %ux%u (need %d..%dx%d..%d, W%%8==0)\n",
                  newW, newH, RES_MIN_W, RES_MAX_W,
                  RES_MIN_H, RES_MAX_H);
        return false;
    }

    /* If we're already at the requested mode, this is a no-op: tearing
       everything down just to put it back wastes time and would still drop
       a frame.  The caller (console verb / menu) can rely on this. */
    if (ModeDesiredX == newW && ModeDesiredY == newH) {
        /* Confirming the current mode is still an explicit choice. */
        g_resIsAutoDerived = false;
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
    /* Per-switch trace at DEBUG so it doesn't spam normal output when a player
       tries several resolutions in the menu; surface it with `loglevel debug`.
       Failures below/above still log loudly via LogPrintf. */
    Log_Debug("Res_Switch: %ux%u -> %ux%u OK (fullscreen %d -> %d)\n",
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
    /* The switch landed: this is now an explicit player resolution, so let
       WriteConfigFile persist it across launches. */
    g_resIsAutoDerived = false;
    return true;
}

/*══════════════════════════════════════════════════════════════════════════*
 *   Boot-time resolution resolution (CLI > cfg > auto-detect > default)     *
 *══════════════════════════════════════════════════════════════════════════*/

bool Res_AutoDetectDimensions(U32 *outW, U32 *outH) {
    U32 dispW = 0, dispH = 0;
    if (!Window_GetPrimaryDisplayDimensions(&dispW, &dispH))
        return false;
    const U32 w = Res_SnapWidescreenWidth(dispW, dispH);
    /* Only override the Classic default when the display is genuinely wider
       than 4:3; a 4:3 (or narrower) monitor keeps 640×480. No 16:9 cap: an
       ultra-wide display gets its true aspect up to the engine's max width,
       matching the Widescreen entry the Display options submenu offers. */
    if (w <= 640)
        return false;
    *outW = w;
    *outH = 480;
    return true;
}

/* Provenance of the last Res_LoadBootDimensions result, for the boot log. */
static const char *g_bootDimSource = "default";
const char *Res_BootDimensionsSource(void) { return g_bootDimSource; }

void Res_LoadBootDimensions(U32 *outW, U32 *outH) {
    /* --resolution CLI overrides everything (one-shot launch). */
    if (Control_HasResolution()) {
        *outW = (U32)Control_GetResolutionX();
        *outH = (U32)Control_GetResolutionY();
        g_bootDimSource = "CLI";
        g_resIsAutoDerived = false;
        return;
    }

    /* lba2.cfg's ResolutionX/Y, if present and valid. Reads via a transient
       DefFileBuffer against a static stack-of-BSS buffer so this function
       never touches the engine heap — safe to call BEFORE
       Mem_ConfigureScreenBuffers / InitMainBuffer, where ScreenAux isn't
       allocated yet. The later ReadConfigFile call (post-InitProgram) does
       its own DefFileBufferInit against ScreenAux which fully resets the
       module state; nothing carries over from this transient init. */
    if (PathConfigFile[0] != '\0' && ExistsFileOrDir(PathConfigFile)) {
        /* BSS scratch (see RES_BOOT_CFG_BUFSZ) rather than blowing main's stack. */
        static char tmpCfgBuf[RES_BOOT_CFG_BUFSZ];
        if (DefFileBufferInit(PathConfigFile, tmpCfgBuf, (S32)sizeof tmpCfgBuf)) {
            const long w = DefFileBufferReadValueDefault("ResolutionX", 0);
            const long h = DefFileBufferReadValueDefault("ResolutionY", 0);
            if (w == 0 && h == 0) {
                /* Neither key present — first launch / new cfg. Silent. */
            } else if (w > 0 && h > 0 && Res_ValidateDimensions((U32)w, (U32)h)) {
                *outW = (U32)w;
                *outH = (U32)h;
                g_bootDimSource = "cfg";
                g_resIsAutoDerived = false;
                return;
            } else {
                LogPrintf(
                    "Res_LoadBootDimensions: ignoring invalid cfg "
                    "ResolutionX=%ld ResolutionY=%ld (need W%%8==0, "
                    "range %dx%d - %dx%d); falling back to %dx%d.\n",
                    w, h, RES_MIN_W, RES_MIN_H,
                    RES_MAX_W, RES_MAX_H,
                    RESOLUTION_X, RESOLUTION_Y);
            }
        }
    }

    /* No CLI override and no saved resolution: auto-detect a widescreen render
       size from the display. Derive-each-launch (never persisted), so it
       adapts if the player changes monitors, and any explicit choice (Display
       submenu / console verb / --resolution) takes over by writing the cfg
       keys above. Falls through to the compile-time default on a 4:3 display,
       a failed display query, or the headless/dummy driver. See
       docs/WIDESCREEN.md Phase 4. */
    if (Res_AutoDetectDimensions(outW, outH)) {
        g_bootDimSource = "auto";
        g_resIsAutoDerived = true;
        return;
    }

    /* Compile-time fallback. */
    *outW = RESOLUTION_X;
    *outH = RESOLUTION_Y;
    g_bootDimSource = "default";
    g_resIsAutoDerived = true;
}

/*══════════════════════════════════════════════════════════════════════════*/
bool Res_LoadBootFullscreen(void) {
    /* Same transient, heap-free cfg read as Res_LoadBootDimensions (see the
       rationale there). Read DisplayFullScreen so InitGraphics can create the
       window fullscreen up front, instead of the historical "create windowed,
       then ReadConfigFile -> SetWindowFullscreen flips it" — which flashed a
       windowed frame on every fullscreen boot. ReadConfigFile still applies the
       same value later; on boot that becomes a no-op since the state matches,
       and it remains the authoritative path for the in-game menu toggle.

       Default windowed, matching ReadConfigFile's DisplayFullScreen default
       (FALSE) and its <0/>1 clamp — keep the two in sync. */
    if (PathConfigFile[0] != '\0' && ExistsFileOrDir(PathConfigFile)) {
        static char tmpCfgBuf[RES_BOOT_CFG_BUFSZ];
        if (DefFileBufferInit(PathConfigFile, tmpCfgBuf, (S32)sizeof tmpCfgBuf)) {
            const long fs = DefFileBufferReadValueDefault("DisplayFullScreen", 0);
            return fs == 1;
        }
    }
    return false;
}

/*══════════════════════════════════════════════════════════════════════════*
 *   Safety gate                                                             *
 *══════════════════════════════════════════════════════════════════════════*/

const char *Res_SwitchAllowedReason(void) {
    /* Refuse mid-cinematic: the Smacker decoder owns the framebuffer and
       partial state would be lost. */
    if (FlagPlayAcf)
        return "Not now: a cinematic is playing - wait for it to finish.";
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

/* Panel geometry — fixed physical size, vertically stacked buttons.
   580×252 fits the LargeurMenu (550) wide DrawOneString buttons with
   matching top/bottom padding (~17 px each — the top padding is
   determined by Font's y-18 offset, the bottom is panel height minus
   the second button's bottom edge). */
#define RES_DIALOG_PANEL_W 580
#define RES_DIALOG_PANEL_H 252

typedef enum {
    RES_BTN_REVERT = 0, /* top — default focus, carries the countdown */
    RES_BTN_KEEP = 1    /* bottom */
} ResDialogBtn;

static void RevertDialog_PanelRect(S32 *x0, S32 *y0, S32 *x1, S32 *y1) {
    const S32 cx = (S32)ModeDesiredX / 2;
    const S32 cy = (S32)ModeDesiredY / 2;
    *x0 = cx - RES_DIALOG_PANEL_W / 2;
    *y0 = cy - RES_DIALOG_PANEL_H / 2;
    *x1 = *x0 + RES_DIALOG_PANEL_W - 1;
    *y1 = *y0 + RES_DIALOG_PANEL_H - 1;
}

/* Draws a horizontally-centred string at the engine's standard text-in-
   menu-item offset. Mirrors DrawSingleString's drop-shadow + colour, but
   omits the BoxUpdate present — caller batches via BoxStaticFullflip so
   the modal doesn't trigger multiple presents per redraw. */
static void RevertDialog_DrawCenteredText(S32 cx, S32 y, const char *utf8) {
    char cp850[128];
    CopyUtf8ToCp850(cp850, sizeof cp850, utf8);
    S32 sf = SizeFont(cp850);
    /* Drop shadow + main text — same colours and (+4, +4) shadow offset as
       DrawSingleString. Using LBAWHITE directly instead of CoulTextMenu
       (which isn't declared extern in GAMEMENU.H) — LBAWHITE is the default
       menu text colour, only swapped to "disabled" grey in places we're not
       touching. */
    ColorFont(BLACK);
    Font(cx - sf / 2 + 4, y - 18 + 4, cp850);
    ColorFont(LBAWHITE);
    Font(cx - sf / 2, y - 18, cp850);
}

/* Renders the whole modal into Log. Caller restores the background
   (CopyScreen Screen->Log) before this runs and presents after via
   BoxStaticFullflip. */
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

    /* Three text lines using direct Font calls (no per-call BoxUpdate). */
    RevertDialog_DrawCenteredText(cx, py0 + 35,
                                  GetLocalizedResDialogLabel(RES_DIALOG_TITLE));

    snprintf(buf, sizeof buf, GetLocalizedResDialogLabel(RES_DIALOG_NOW_AT),
             (unsigned)s_revertNewW, (unsigned)s_revertNewH);
    RevertDialog_DrawCenteredText(cx, py0 + 68, buf);

    RevertDialog_DrawCenteredText(cx, py0 + 100,
                                  GetLocalizedResDialogLabel(RES_DIALOG_KEEP_Q));

    /* Two buttons stacked vertically via DrawOneString — the engine's
       standard menu-item painter (same one MainGameMenu / OptionsMenu
       use). Handles cadre, shading, optional plasma (draw == 1), and
       text at the engine's calibrated y-18 offset, all matching the
       rest of the game pixel-for-pixel. LargeurMenu (550) wide ×
       HAUTEUR_STANDARD (50) tall, centred horizontally at cx. */
    char cp850_revert[64], cp850_keep[64];
    snprintf(buf, sizeof buf,
             GetLocalizedResDialogLabel(RES_DIALOG_BTN_REVERT_FMT),
             (int)secondsLeft);
    CopyUtf8ToCp850(cp850_revert, sizeof cp850_revert, buf);
    snprintf(buf, sizeof buf, "%s",
             GetLocalizedResDialogLabel(RES_DIALOG_BTN_KEEP));
    CopyUtf8ToCp850(cp850_keep, sizeof cp850_keep, buf);

    const S32 btn_revert_y = py0 + 150;
    const S32 btn_keep_y = py0 + 210;

    DrawOneString(cx, btn_revert_y, cp850_revert,
                  focused == RES_BTN_REVERT ? 1 : 0);
    DrawOneString(cx, btn_keep_y, cp850_keep,
                  focused == RES_BTN_KEEP ? 1 : 0);

    /* DrawOneString accumulates dirty rects via BoxStaticAdd but doesn't
       present. Mark the panel + small margin too (for the cadre border
       and text drop-shadow that extend slightly outside the buttons)
       and flip once for the whole modal. */
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
    InitWaitNoInput(I_FIRE | I_ACTION | I_MENUS | I_UP | I_DOWN);

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

        /* Directional input toggles focus between the two stacked buttons.
           UP / DOWN matches the rest of the engine's menus (NPC choice
           menus, options menu, save list — all vertical stacks navigated
           with the same keys, on both keyboard and gamepad). */
        if (Input & I_UP)
            focused = RES_BTN_REVERT;
        if (Input & I_DOWN)
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

    if (choice == RES_REVERT_KEEP) {
        /* Player confirmed the new mode is usable. Persist to lba2.cfg
           so the next launch boots straight at this resolution
           (Res_LoadBootDimensions reads the same keys). WriteConfigFile
           writes the full cfg; ModeDesiredX/Y is the source of truth at
           this point. Don't write on Revert / timeout — the cfg should
           keep recording the last *confirmed* resolution. */
        WriteConfigFile();
    } else /* RES_REVERT_REVERT */ {
        /* Recursive Res_Switch to the previous (W, H). The orchestrator
           handles the no-op case (already at prev W,H) cleanly. */
        Res_Switch(s_revertPrevW, s_revertPrevH);
    }

    return choice;
}
