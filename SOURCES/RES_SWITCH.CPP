/*
 * Runtime resolution switching — orchestrator.  See RES_SWITCH.H + the design
 * doc at docs/RUNTIME_RESOLUTION.md.
 */

#include "RES_SWITCH.H"

#include "C_EXTERN.H"
#include "FLOW.H"
#include "PTRFUNC.H"

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/WINDOW.H>

#include <stdio.h>

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
    SetClipWindow(0, 0, (S32)w - 1, (S32)h - 1);
    SetClip(0, 0, (S32)w - 1, (S32)h - 1);
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

    /* InitPartFlow caches MAX_FLOWS pointers into ListFlowDots (also in
       MainBuffer) in ListPartFlow[i].PtrListDot at boot; those go stale
       when teardown frees the buffer.  Re-running it rewires them to the
       new ListFlowDots and zeroes NbDot for every slot, which also resets
       any in-flight particle flow from before the switch — acceptable
       cost (a quick flash of dust effects vanishing) for a switch
       that's expected to be rare. */
    InitPartFlow();

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
