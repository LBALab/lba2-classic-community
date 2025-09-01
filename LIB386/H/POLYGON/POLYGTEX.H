
#pragma once
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

S32 Filler_TextureGouraud(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureDither(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureGouraudChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureDitherChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureGouraudZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureGouraudChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureGouraudNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureGouraudChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);

// =============================================================================
#ifdef __cplusplus
}
#endif
