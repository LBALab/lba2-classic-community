
#pragma once
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

S32 Filler_TextureZGouraud(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZGouraudChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZGouraudZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZGouraudChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZGouraudNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZGouraudChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);

#ifdef __cplusplus
}
#endif
