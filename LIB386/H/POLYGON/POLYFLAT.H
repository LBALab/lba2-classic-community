
#pragma once
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

S32 Filler_Flat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_FlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_FlatZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);

// =============================================================================
#ifdef __cplusplus
}
#endif
