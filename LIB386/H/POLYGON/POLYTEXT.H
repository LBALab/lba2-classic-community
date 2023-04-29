
#pragma once
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

S32 Filler_Texture(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureFlat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureFlatChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureChromaKeyFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureFlatZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureFlatChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureChromaKeyFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureFlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureFlatChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureChromaKeyFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);

#ifdef __cplusplus
}
#endif
