#pragma once
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

S32 Filler_Gouraud(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_Dither(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_GouraudTable(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_DitherTable(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_GouraudFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_DitherFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_GouraudZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_DitherZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_GouraudTableZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_DitherTableZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_GouraudFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_DitherFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_GouraudNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_DitherNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_GouraudTableNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_DitherTableNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_GouraudFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_DitherFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);

// =============================================================================
#ifdef __cplusplus
}
#endif
