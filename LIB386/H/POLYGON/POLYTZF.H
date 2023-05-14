
#pragma once
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

S32 Filler_TextureZFogSmooth(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFogSmoothZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFogSmoothNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);

#ifdef __cplusplus
}
#endif
