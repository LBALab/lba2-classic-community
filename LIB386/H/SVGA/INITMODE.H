#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Initialization ----------------------------------------------------------
bool InitGraphics(U32 resX, U32 resY, bool fullscreen);
void EndGraphics();

// =============================================================================
#ifdef __cplusplus
}
#endif
