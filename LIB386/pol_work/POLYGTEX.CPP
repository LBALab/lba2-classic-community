/*
; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                         (Adeline Software Intl.)                   ***
; ***                           1995-96-97                               ***
; ***                                                                    ***
; **************************************************************************
; **************************************************************************
; **************************************************************************
*/

#include <POLYGON/POLYGTEX.H>

#include <POLYGON/POLY.H>

#include <stdio.h>

PTR_U8 GTEX_PtrLog = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrCLUT = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrZBuffer1 = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrZBufferPatch = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrMap2 = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrMapPatch = (PTR_U8)0x12345678;
U32 GTEX_RepMask2 = 0x12345678;
U32 GTEX_RepMaskPatch = 0x12345678;
S32 GTEX_DecZBuf_Gour = 0x12345678;
S32 GTEX_DecMapU = 0x12345678;
S32 GTEX_DecMapU2 = 0x12345678;
S32 GTEX_DecMapV_Gour = 0x12345678;
S32 GTEX_DecMapV_Gour2 = 0x12345678;
S32 GTEX_DecMapV_IntZBuf = 0x12345678;
S8 GTEX_IntMapV = 0x12;
S8 GTEX_IntMapV2 = 0x12;
S8 GTEX_IntMapU = 0x12;
S8 GTEX_IntMapU2 = 0x12;

S32 Filler_TextureGouraud(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  U32 curMapU = Fill_CurMapUMin;
  U32 curMapV = Fill_CurMapVMin;
  Fill_CurY += count;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    GTEX_PtrMapPatch = PtrMap;
    GTEX_PtrCLUT = PtrCLUTGouraud;
    GTEX_DecMapU = Fill_MapU_XSlope << 16;
    GTEX_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    GTEX_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    GTEX_DecMapV_Gour = (Fill_MapV_XSlope << 16) | (Fill_Gouraud_XSlope >> 8) & 0xFFFF;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curMapU += Fill_MapU_LeftSlope;
    curMapV += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  while (1)
  {
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *screen = (fillCurXMin >> 16) + curOffLine;
      U32 gouraudXSlope = Fill_Gouraud_XSlope;
      S32 curGouraudMin = Fill_CurGouraudMin;
      gouraudXSlope = (gouraudXSlope & 0xFFFF00FF) | (((curMapV >> 16) & 0xFF) << 8);
      U32 u = curMapU << 16;
      gouraudXSlope = (gouraudXSlope & 0xFFFFFF00) | ((curMapU >> 16) & 0xFF);
      U32 v = (curMapV << 16) | (curGouraudMin >> 8) & 0xFFFF;
      U32 colorIndex = (curGouraudMin >> 8);
      U32 mask = RepMask & gouraudXSlope;
      do
      {
        colorIndex = (colorIndex & 0xFFFFFF00) | GTEX_PtrMapPatch[mask];
        U8 carry = ((U64)GTEX_DecMapU + (U64)u) > 0xFFFFFFFF;
        u += GTEX_DecMapU;
        mask = (mask & 0xFFFFFF00) | ((GTEX_IntMapU + carry + mask) & 0xFF);
        carry = ((U64)GTEX_DecMapV_Gour + (U64)v) > 0xFFFFFFFF;
        v += GTEX_DecMapV_Gour;
        mask = (mask & 0xFFFF00FF) | ((((mask >> 8) & 0xFF) + GTEX_IntMapV + carry) & 0xFF);
        *screen++ = GTEX_PtrCLUT[colorIndex];
        colorIndex = (colorIndex & 0xFFFF00FF) | (v & 0xFF00);
        mask &= RepMask;
        --diffX;
      }
      while (diffX);
    }
    // @@EndScanLine:
    if (!--count)
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curMapU += Fill_MapU_LeftSlope;
    curMapV += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurMapUMin = curMapU;
  Fill_CurMapVMin = curMapV;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureDither(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  U32 curMapU = Fill_CurMapUMin;
  U32 curMapV = Fill_CurMapVMin;
  Fill_CurY += count;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    GTEX_PtrMapPatch = PtrMap;
    GTEX_PtrCLUT = PtrCLUTGouraud;
    GTEX_DecMapU = Fill_MapU_XSlope << 16;
    GTEX_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    GTEX_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    GTEX_DecMapV_Gour = (Fill_MapV_XSlope << 16) | (Fill_Gouraud_XSlope >> 8) & 0xFFFF;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curMapU += Fill_MapU_LeftSlope;
    curMapV += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  while (1)
  {
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *screen = (fillCurXMin >> 16) + curOffLine;
      U32 gouraudXSlope = Fill_Gouraud_XSlope;
      S32 curGouraudMin = Fill_CurGouraudMin;
      gouraudXSlope = (gouraudXSlope & 0xFFFF00FF) | (((curMapV >> 16) & 0xFF) << 8);
      U32 u = curMapU << 16;
      gouraudXSlope = (gouraudXSlope & 0xFFFFFF00) | ((curMapU >> 16) & 0xFF);
      U32 v = (curMapV << 16) | (curGouraudMin >> 8) & 0xFFFF;
      U32 colorIndex = (curGouraudMin >> 8);
      U32 mask = RepMask & gouraudXSlope;
      do
      {
        colorIndex = (colorIndex & 0xFFFFFF00) | (*(mask + GTEX_PtrMapPatch) & 0xFF);
        U8 carry = ((U64)GTEX_DecMapU + (U64)u) > 0xFFFFFFFF;
        u += GTEX_DecMapU;
        mask = (mask & 0xFFFFFF00) | ((GTEX_IntMapU + carry + mask) & 0xFF);
        carry = ((U64)GTEX_DecMapV_Gour + (U64)v) > 0xFFFFFFFF;
        v += GTEX_DecMapV_Gour;
        mask = (mask & 0xFFFF00FF) | ((((mask >> 8) & 0xFF) + GTEX_IntMapV + carry) & 0xFF);
        *screen++ = *(colorIndex + GTEX_PtrCLUT);
        colorIndex = (colorIndex & 0xFFFF00FF) | (v & 0xFF00);
        mask &= RepMask;
        --diffX;
      }
      while (diffX);
    }
    // @@EndScanLine:
    if (!--count)
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curMapU += Fill_MapU_LeftSlope;
    curMapV += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurMapUMin = curMapU;
  Fill_CurMapVMin = curMapV;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureGouraudChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureDitherChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureGouraudZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureGouraudChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureGouraudNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureGouraudChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

