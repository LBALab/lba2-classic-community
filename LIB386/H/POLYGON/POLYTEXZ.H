
#pragma once
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

/* Perspective correction function pointers — defined as globals in
 * POLYTEXZ.CPP to support both CPP callers and ASM callers that
 * reference them as Extrn DWORD (indirect call via `call [symbol]`). */
extern void (*Fill_Init_Perspective)(S32 lineU, S32 lineV, U32 fillWidth);
extern void (*Fill_Loop_Perspective)(U32 fillWidth);

S32 Filler_TextureZ(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFlat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFlatChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZChromaKeyFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFlatZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFlatChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZChromaKeyFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFlatChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);
S32 Filler_TextureZChromaKeyFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);

#ifdef __cplusplus
}
#endif
