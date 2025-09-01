#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
extern S32 TextXMin;
extern S32 TextXMax;
extern S32 TextYMin;
extern S32 TextYMax;

// -----------------------------------------------------------------------------
void GraphPrintf(U32 flagflip, U32 x, U32 y, const char *format, ...);

// =============================================================================
#ifdef __cplusplus
}
#endif
