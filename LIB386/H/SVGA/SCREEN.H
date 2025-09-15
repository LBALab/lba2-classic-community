#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/LIMITS.H>
#include <SYSTEM/FASTCPY.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
extern void *Log;
extern void *Screen;
extern U32 ModeDesiredX; ///< X resolution for Log and Screen
extern U32 ModeDesiredY; ///< Y resolution for Log and Screen
extern U32 TabOffLine[ADELINE_MAX_Y_RES]; ///< Line offsets for Log and Screen

// --- Initialization ----------------------------------------------------------
bool InitScreen();
void EndScreen();

// --- Interface ---------------------------------------------------------------
bool CreateScreenMemory(U32 resX, U32 resY);

// -----------------------------------------------------------------------------
#define CopyScreen(src, dst) FastCopy(dst, src, ModeDesiredX *ModeDesiredY)
#define CopyScreenResized(src, dst, col) CopyScreenColour(src, dst, col)

// =============================================================================
#ifdef __cplusplus
}
#endif
