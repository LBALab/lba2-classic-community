#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/LIMITS.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
extern void *Log;
extern void *Screen;
extern U32 ModeDesiredX;                  ///< X resolution for Log and Screen
extern U32 ModeDesiredY;                  ///< Y resolution for Log and Screen
extern U32 TabOffLine[ADELINE_MAX_Y_RES]; ///< Line offsets for Log and Screen

// --- Initialization ----------------------------------------------------------
bool InitScreen();
void EndScreen();

// --- Interface ---------------------------------------------------------------
bool CreateScreenMemory(U32 resX, U32 resY);

// --- Screen-space view zoom --------------------------------------------------
// Magnify a centred sub-region of the finished frame at present time (pure
// image-space; the render pipeline is untouched). Fixed-point: ZOOM_ONE is 1.0,
// 512 is 2x. At ZOOM_ONE the present is bit-identical to no zoom. This is a
// dev/experimental zoom: it magnifies pixels (no extra detail) and scales the
// whole frame including the UI.
#define ZOOM_ONE 256
extern S32 ScreenZoom;
void SetScreenZoom(S32 zoom);

// -----------------------------------------------------------------------------
#define CopyScreen(src, dst) FastCopy(dst, src, ModeDesiredX *ModeDesiredY)

// =============================================================================
#ifdef __cplusplus
}
#endif
