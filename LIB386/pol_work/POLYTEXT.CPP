/*
; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                           (Adeline Software Intl.)                 ***
; ***                           1995-96-97                               ***
; ***                                                                    ***
; **************************************************************************
; **************************************************************************
; **************************************************************************
*/

#include <POLYGON/POLYTEXT.H>

#include <stdio.h>
#include <POLYGON/POLY.H>

U32 Precalc_Inc = 0; // Precalc value

// Runtime code patch refactor, uses memory instead of instruction substitution
U32 PtrZBuffer1 = 0x12345678;
U32 PtrZBufferPatch = 0x12345678;
U32 PtrMapPatch = 0x12345678;
U32 PtrCLUT = 0x12345678;
//U32 PtrLog = 0x12345678;
U32 RepMaskPatch = 0x12345678;
U32 DecMapUIntZBuf = 0x12345678;
U32 DecMapV = 0x12345678;
U32 DecMapU = 0x12345678;
U32 DecZBuf = 0x12345678;
U32 IntZBuf = 0x12345678;
U8 IntMapV = 0x12;
U8 IntMapU = 0x12;

S32 Filler_Texture(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;

  if (Fill_Patch != 1)
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
  }
  else
  {
    --Fill_Patch;
    DecMapU = Fill_MapU_XSlope << 16;
    DecMapV = Fill_MapV_XSlope << 16;
    RepMaskPatch = RepMask;
    Precalc_Inc = ((Fill_MapV_XSlope & 0xFF0000u) >> 8) | ((U32)Fill_MapU_XSlope >> 16) & 0xFF;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((S32)(fillCurXMax >> 16) > (S32)(fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 lineV = v;
      U32 lineU = u;
      // Load 1st dst cache line
      U16 factor = ((U16)fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        U64 addU = ((U64)factor * (U64)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        lineU += addUDec;

        U64 addV = ((U64)factor * (U64)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = addVDec << 16;
        addVInt = addVInt >> 16;
        addVInt |= addVDec;
        lineV += addVInt;

        // There was some @@Preload code here, but it did nothing other
        // than reading some bytes but not doing anything with it. It
        // also had the comment "Load dst in WB cache", which means nothing
        // to me. I'm guessing it was some kind of prefetching, but I'm
        // not sure. I'm leaving it out for now.
      }
      // @@FastInit:
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU & 0xFF0000u) >> 16) | ((lineV >> 8) & 0xFF00));
      U32 lineUDec = lineU << 16;
      do
      {
        // @@Each_X:
        lineUDec = (lineUDec & 0xFFFFFF00) | (PtrMap[mapIndex] & 0xFF);
        U8 carryFlag = ((U64)DecMapV + (U64)lineVDec) > 0xFFFFFFFF;
        lineVDec += DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | ((((Precalc_Inc >> 8) & 0xFF) + ((mapIndex >> 8) & 0xFF) + carryFlag) << 8);
        carryFlag = ((U64)DecMapU + (U64)lineUDec) > 0xFFFFFFFF;
        lineUDec += DecMapU;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((Precalc_Inc & 0xFF) + (mapIndex & 0xFF) + carryFlag) & 0xFF);
        *line++ = lineUDec & 0xFF;
        mapIndex &= RepMaskPatch;
        --fillWidth;
      }
      while (fillWidth);
      // @@End_X:
    }
    // @@EndScanLine:
    diffY--;
    if (diffY == 0)
    {
      break;
    }

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureFlat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureFlatChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureChromaKeyFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureFlatZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureFlatChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureChromaKeyFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureFlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureFlatChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureChromaKeyFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}
