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
#include <SVGA/SCREEN.H>

U32 Precalc_Inc = 0; // Precalc value

// Runtime code patch refactor, uses memory instead of instruction substitution
PTR_U16 POLYTEXT_PtrZBuffer1 = NULL;
PTR_U16 POLYTEXT_PtrZBufferPatch = NULL;
PTR_U8 POLYTEXT_PtrMapPatch = NULL;
PTR_U8 POLYTEXT_PtrCLUT = NULL;
PTR_U8 POLYTEXT_PtrLog = NULL;
U32 POLYTEXT_RepMaskPatch = 0x12345678;
U32 POLYTEXT_DecMapUIntZBuf = 0x12345678;
U32 POLYTEXT_DecMapV = 0x12345678;
U32 POLYTEXT_DecMapU = 0x12345678;
U32 POLYTEXT_DecZBuf = 0x12345678;
U32 POLYTEXT_IntZBuf = 0x12345678;
U8 POLYTEXT_IntMapV = 0x12;
U8 POLYTEXT_IntMapU = 0x12;

// Verified
S32 Filler_Texture(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_RepMaskPatch = RepMask;
    Precalc_Inc = ((Fill_MapV_XSlope & 0xFF0000) >> 8) | (Fill_MapU_XSlope >> 16) & 0xFF;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 lineV = v;
      U32 lineU = u;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        // Simple multiply for sub-pixel adjustment, matching assembly imul instruction
        S32 addU = ((S32)factor * (S32)Fill_MapU_XSlope) >> 16;
        lineU += addU;

        S32 addV = ((S32)factor * (S32)Fill_MapV_XSlope) >> 16;
        lineV += addV;

        // There was some @@Preload code here, but it did nothing other
        // than reading some bytes but not doing anything with it. It
        // also had the comment "Load dst in WB cache", which means nothing
        // to me. I'm guessing it was some kind of prefetching, but I'm
        // not sure. I'm leaving it out for now.
      }
      // @@FastInit:
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00)); // eax
      U32 lineUDec = lineU << 16; // edx
      do
      {
        // @@Each_X:
        U8 texel = PtrMap[mapIndex];

        U8 carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((((Precalc_Inc >> 8) & 0xFF) + ((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        carry = (lineUDec > (0xFFFFFFFF - POLYTEXT_DecMapU));
        lineUDec += POLYTEXT_DecMapU;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((Precalc_Inc & 0xFF) + (mapIndex & 0xFF) + carry) & 0xFF);

        *line = texel;
        line++;

        mapIndex &= POLYTEXT_RepMaskPatch;

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
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_RepMaskPatch = RepMask;
    Precalc_Inc = ((Fill_MapV_XSlope & 0xFF0000) >> 8) | ((Fill_MapU_XSlope >> 16) & 0xFF);
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      S32 lineU = u;
      S32 lineV = v;

      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        // Simple multiply for sub-pixel adjustment, matching assembly imul instruction
        S32 addU = ((S32)factor * (S32)Fill_MapU_XSlope) >> 16;
        lineU += addU;

        S32 addV = ((S32)factor * (S32)Fill_MapV_XSlope) >> 16;
        lineV += addV;
      }
      // @@FastInit:
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00)); // eax
      U32 lineUDec = lineU << 16; // edx
      do
      {
        // @@Each_X:
        U8 texel = POLYTEXT_PtrMapPatch[mapIndex];

        U8 carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((((Precalc_Inc >> 8) & 0xFF) + ((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        carry = (lineUDec > (0xFFFFFFFF - POLYTEXT_DecMapU));
        lineUDec += POLYTEXT_DecMapU;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((Precalc_Inc & 0xFF) + (mapIndex & 0xFF) + carry) & 0xFF);

        if (texel != 0) // Chroma key test - skip transparent pixels
        {
          *line = texel;
        }
        line++;

        mapIndex &= POLYTEXT_RepMaskPatch;

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

S32 Filler_TextureFlat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_PtrCLUT = (PTR_U8)PtrCLUTGouraud;
    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_RepMaskPatch = RepMask;
    Precalc_Inc = ((Fill_MapV_XSlope & 0xFF0000) >> 8) | ((Fill_MapU_XSlope >> 16) & 0xFF);
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      S32 lineU = u;
      S32 lineV = v;

      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        U32 addU = factor * Fill_MapU_XSlope >> 16;
        lineU += addU;

        U32 addV = factor * Fill_MapV_XSlope >> 16;
        lineV += addV;

        // @@Preload - cache preloading (omitted)
      }

      // @@FastInit:
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00)); // eax
      U32 lineUDec = lineU << 16; // esi
      U8 colorBase = Fill_Color.Num & 0xFF; // edx

      do
      {
        // @@Each_X:
        U8 texel = POLYTEXT_PtrMapPatch[mapIndex];

        U8 carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((((Precalc_Inc >> 8) & 0xFF) + ((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        carry = (lineUDec > (0xFFFFFFFF - POLYTEXT_DecMapU));
        lineUDec += POLYTEXT_DecMapU;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((Precalc_Inc & 0xFF) + (mapIndex & 0xFF) + carry) & 0xFF);

        U8 color = POLYTEXT_PtrCLUT[texel + colorBase];
        *line = color;
        line++;

        mapIndex &= POLYTEXT_RepMaskPatch;

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

S32 Filler_TextureFlatChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_PtrCLUT = (PTR_U8)PtrCLUTGouraud;
    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_RepMaskPatch = RepMask;
    Precalc_Inc = ((Fill_MapV_XSlope & 0xFF0000) >> 8) | ((Fill_MapU_XSlope >> 16) & 0xFF);
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      S32 lineU = u;
      S32 lineV = v;

      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        U64 addU = ((U64)factor * (U64)Fill_MapU_XSlope);
        U32 addUHigh = (U32)(addU >> 32);
        U32 addULow = (U32)(addU & 0xFFFFFFFF);
        addUHigh = addUHigh << 16;
        addULow = addULow >> 16;
        addUHigh |= addULow;
        lineU += addUHigh;

        U64 addV = ((U64)factor * (U64)Fill_MapV_XSlope);
        U32 addVHigh = (U32)(addV >> 32);
        U32 addVLow = (U32)(addV & 0xFFFFFFFF);
        addVHigh = addVHigh << 16;
        addVLow = addVLow >> 16;
        addVLow |= addVHigh;
        lineV += addVLow;
      }

      // @@FastInit:
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00)); // eax
      U32 lineUDec = lineU << 16; // esi
      U8 colorBase = Fill_Color.Num & 0xFF; // edx

      do
      {
        // @@Each_X:
        U8 texel = POLYTEXT_PtrMapPatch[mapIndex];

        U8 carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((((Precalc_Inc >> 8) & 0xFF) + ((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        carry = (lineUDec > (0xFFFFFFFF - POLYTEXT_DecMapU));
        lineUDec += POLYTEXT_DecMapU;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((Precalc_Inc & 0xFF) + (mapIndex & 0xFF) + carry) & 0xFF);

        if (texel != 0) // Chroma key test - skip transparent pixels
        {
          U8 color = POLYTEXT_PtrCLUT[texel + colorBase];
          *line = color;
        }
        line++;

        mapIndex &= POLYTEXT_RepMaskPatch;

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

S32 Filler_TextureFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_RepMaskPatch = RepMask;
    POLYTEXT_PtrMapPatch = PtrMap;
    Precalc_Inc = ((Fill_MapV_XSlope & 0xFF0000) >> 8) | (Fill_MapU_XSlope >> 16) & 0xFF;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 lineV = v;
      U32 lineU = u;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
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
      }
      // @@FastInit:
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00)); // eax
      U32 lineUDec = lineU << 16; // edx
      do
      {
        // @@Each_X:
        U8 textureColor = POLYTEXT_PtrMapPatch[mapIndex];
        U8 foggedColor = Fill_Logical_Palette[textureColor];
        *line = foggedColor;

        U8 carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((((Precalc_Inc >> 8) & 0xFF) + ((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        carry = (lineUDec > (0xFFFFFFFF - POLYTEXT_DecMapU));
        lineUDec += POLYTEXT_DecMapU;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((Precalc_Inc & 0xFF) + (mapIndex & 0xFF) + carry) & 0xFF);

        line++;
        mapIndex &= POLYTEXT_RepMaskPatch;
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

S32 Filler_TextureChromaKeyFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_RepMaskPatch = RepMask;
    POLYTEXT_PtrMapPatch = PtrMap;
    Precalc_Inc = ((Fill_MapV_XSlope & 0xFF0000) >> 8) | (Fill_MapU_XSlope >> 16) & 0xFF;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 lineV = v;
      U32 lineU = u;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
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
      }
      // @@FastInit:
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00)); // eax
      U32 lineUDec = lineU << 16; // edx
      do
      {
        // @@Each_X:
        U8 textureColor = POLYTEXT_PtrMapPatch[mapIndex];
        if (textureColor != 0) {
          U8 foggedColor = Fill_Logical_Palette[textureColor];
          *line = foggedColor;
        }

        U8 carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((((Precalc_Inc >> 8) & 0xFF) + ((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        carry = (lineUDec > (0xFFFFFFFF - POLYTEXT_DecMapU));
        lineUDec += POLYTEXT_DecMapU;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((Precalc_Inc & 0xFF) + (mapIndex & 0xFF) + carry) & 0xFF);

        line++;
        mapIndex &= POLYTEXT_RepMaskPatch;
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

// Verified
S32 Filler_TextureZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_RepMaskPatch = RepMask;
    Precalc_Inc = ((Fill_MapV_XSlope & 0xFF0000) >> 8) | (Fill_MapU_XSlope >> 16) & 0xFF;
    POLYTEXT_IntMapU = Precalc_Inc & 0xFF;
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_PtrZBuffer1 = PtrZBuffer;
    POLYTEXT_DecZBuf = Fill_ZBuf_XSlope << 24;
    POLYTEXT_IntZBuf = Fill_ZBuf_XSlope >> 8;
    POLYTEXT_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 lineV = v;
      U32 lineU = u;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
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
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8)); // eax
      U32 zInt = Fill_CurZBufMin >> 8; // edx
      U32 zDec = Fill_CurZBufMin << 24; // esi
      U32 offset = line - (U8 *)Log; // edi
      U32 incDecMapU = Precalc_Inc | (lineU << 16); // ebx

      do
      {
        // @@Each_X:
        if (POLYTEXT_PtrZBufferPatch[offset] >= (zInt & 0xFFFF))
        {
          POLYTEXT_PtrZBuffer1[offset] = zInt & 0xFFFF;
          POLYTEXT_PtrLog[offset] = POLYTEXT_PtrMapPatch[mapIndex];
        }

        offset++;

        U8 carry = (incDecMapU > (0xFFFFFFFF - POLYTEXT_DecMapU));
        incDecMapU += POLYTEXT_DecMapU;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry) & 0xFF);

        carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | ((((incDecMapU >> 8) & 0xFF) + (((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        carry = (zDec > (0xFFFFFFFF - POLYTEXT_DecZBuf));
        zDec += POLYTEXT_DecZBuf;
        zInt += POLYTEXT_IntZBuf + carry;

        mapIndex &= POLYTEXT_RepMaskPatch;
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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_DecMapU = (U32)Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = (U32)Fill_MapV_XSlope << 16;
    POLYTEXT_RepMaskPatch = RepMask;
    
    // Integer increments for texture mapping
    POLYTEXT_IntMapU = ((U32)Fill_MapU_XSlope >> 16) & 0xFF;
    POLYTEXT_IntMapV = ((U32)Fill_MapV_XSlope >> 16) & 0xFF;
    
    // Z-buffer setup
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_PtrZBuffer1 = PtrZBuffer;
    POLYTEXT_DecZBuf = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXT_IntZBuf = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXT_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;

      // Adjust for sub-pixel start position
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        S32 uAdjust = Fill_MapU_XSlope * factor;
        u += (uAdjust >> 16);
        
        S32 vAdjust = Fill_MapV_XSlope * factor;
        v += (vAdjust >> 16);

        // Cache line preloading for performance
        U8 *cacheStart = (U8 *)((uintptr_t)line & ~31);
        U8 *cacheEnd = (U8 *)(((uintptr_t)line + fillWidth + 31) & ~31);
        for (U8 *cache = cacheStart; cache <= cacheEnd; cache += 32)
        {
          volatile U8 dummy = *cache;
          (void)dummy;
        }
      }

      // Prepare texture coordinates and Z-buffer values
      U32 mapU = (u >> 16) & 0xFF;
      U32 mapV = (v & 0xFF0000) >> 8;
      U32 mapAddr = (mapU | mapV) & RepMask;
      
      U32 decMapU = u << 16;
      U32 decMapV = v << 16;
      
      // Convert line pointer to offset for Z-buffer operations
      U32 offset = line - (U8 *)Log;
      
      // Z-buffer values: split Fill_CurZBufMin into integer and fractional parts
      U32 zBufFrac = Fill_CurZBufMin << 24;  // Fractional part
      U32 zBufInt = Fill_CurZBufMin >> 8;    // Integer part

      // Process each pixel with chroma key and Z-buffer testing/writing
      for (U32 i = 0; i < fillWidth; i++)
      {
        // Z-buffer test
        if (POLYTEXT_PtrZBufferPatch[offset + i] <= (U16)zBufInt)
        {
          // Update Z-buffer
          POLYTEXT_PtrZBuffer1[offset + i] = (U16)zBufInt;
          
          // Sample texture
          U8 texel = POLYTEXT_PtrMapPatch[mapAddr];
          
          // Chroma key test: only draw if texel is not 0 (transparent)
          if (texel != 0)
          {
            POLYTEXT_PtrLog[offset + i] = texel;
          }
        }
        
        // Update texture coordinates
        decMapU += POLYTEXT_DecMapU;
        mapU += (decMapU < POLYTEXT_DecMapU) ? POLYTEXT_IntMapU + 1 : POLYTEXT_IntMapU;
        
        decMapV += POLYTEXT_DecMapV;
        mapV += (decMapV < POLYTEXT_DecMapV) ? POLYTEXT_IntMapV : 0;
        
        mapAddr = (mapU & 0xFF) | (mapV & 0xFF00);
        mapAddr &= POLYTEXT_RepMaskPatch;
        
        // Update Z-buffer values for interpolation
        zBufFrac += POLYTEXT_DecZBuf;
        zBufInt += POLYTEXT_IntZBuf;
        if (zBufFrac < POLYTEXT_DecZBuf)  // Carry detection
        {
          zBufInt++;
        }
      }
    }

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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_TextureFlatZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_DecMapUIntZBuf = (Fill_MapU_XSlope << 16) | ((Fill_ZBuf_XSlope >> 8) & 0xFFFF);
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_PtrCLUT = Fill_Color.Ptr;
    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_RepMaskPatch = RepMask;
    POLYTEXT_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    POLYTEXT_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_PtrZBuffer1 = PtrZBuffer;
    POLYTEXT_DecZBuf = Fill_ZBuf_XSlope << 24;
    POLYTEXT_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 lineV = v;
      U32 lineU = u;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
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
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8)); // eax
      U32 zDec = Fill_CurZBufMin << 24; // esi
      U32 offset = line - (U8 *)Log; // edi
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16); // edx

      do
      {
        // @@Each_X:
        if (POLYTEXT_PtrZBufferPatch[offset] >= (decU_intZ & 0xFFFF))
        {
          U8 colorIndex = POLYTEXT_PtrMapPatch[mapIndex];
          POLYTEXT_PtrZBuffer1[offset] = decU_intZ & 0xFFFF;
          POLYTEXT_PtrLog[offset] = POLYTEXT_PtrCLUT[colorIndex];
        }

        U8 carry = (zDec > (0xFFFFFFFF - POLYTEXT_DecZBuf));
        zDec += POLYTEXT_DecZBuf;
        U8 carry2 = (decU_intZ > (0xFFFFFFFF - POLYTEXT_DecMapUIntZBuf - carry));
        decU_intZ += POLYTEXT_DecMapUIntZBuf + carry;

        offset++;

        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry2) & 0xFF);

        carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((POLYTEXT_IntMapV & 0xFF) + (((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        mapIndex &= POLYTEXT_RepMaskPatch;
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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_TextureFlatChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_DecMapUIntZBuf = (Fill_MapU_XSlope << 16) | ((Fill_ZBuf_XSlope >> 8) & 0xFFFF);
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_PtrCLUT = Fill_Color.Ptr;
    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_RepMaskPatch = RepMask;
    POLYTEXT_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    POLYTEXT_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_PtrZBuffer1 = PtrZBuffer;
    POLYTEXT_DecZBuf = Fill_ZBuf_XSlope << 24;
    POLYTEXT_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 lineV = v;
      U32 lineU = u;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
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
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8)); // eax
      U32 zDec = Fill_CurZBufMin << 24; // esi
      U32 offset = line - (U8 *)Log; // edi
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16); // edx

      do
      {
        // @@Each_X:
        if (POLYTEXT_PtrZBufferPatch[offset] >= (decU_intZ & 0xFFFF))
        {
          U8 colorIndex = POLYTEXT_PtrMapPatch[mapIndex];
          if (colorIndex != 0)
          {
            POLYTEXT_PtrZBuffer1[offset] = decU_intZ & 0xFFFF;
            POLYTEXT_PtrLog[offset] = POLYTEXT_PtrCLUT[colorIndex];
          }
        }

        U8 carry = (zDec > (0xFFFFFFFF - POLYTEXT_DecZBuf));
        zDec += POLYTEXT_DecZBuf;
        U8 carry2 = (decU_intZ > (0xFFFFFFFF - POLYTEXT_DecMapUIntZBuf - carry));
        decU_intZ += POLYTEXT_DecMapUIntZBuf + carry;

        offset++;

        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry2) & 0xFF);

        carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((POLYTEXT_IntMapV & 0xFF) + (((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        mapIndex &= POLYTEXT_RepMaskPatch;
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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_TextureFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_DecMapUIntZBuf = (Fill_MapU_XSlope << 16) | ((Fill_ZBuf_XSlope >> 8) & 0xFFFF);
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_RepMaskPatch = RepMask;
    POLYTEXT_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    POLYTEXT_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_PtrZBuffer1 = PtrZBuffer;
    POLYTEXT_DecZBuf = Fill_ZBuf_XSlope << 24;
    POLYTEXT_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 lineV = v;
      U32 lineU = u;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
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
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8)); // eax
      U32 decZ = Fill_CurZBufMin << 24; // esi
      U32 offset = line - (U8 *)Log; // edi
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16); // edx

      do
      {
        // @@Each_X:
        if (POLYTEXT_PtrZBufferPatch[offset] >= (decU_intZ & 0xFFFF))
        {
          POLYTEXT_PtrZBuffer1[offset] = decU_intZ & 0xFFFF;
          U8 color = POLYTEXT_PtrMapPatch[mapIndex];
          POLYTEXT_PtrLog[offset] = Fill_Logical_Palette[color];
        }

        offset++;

        U8 carry = (decZ > (0xFFFFFFFF - POLYTEXT_DecZBuf));
        decZ += POLYTEXT_DecZBuf;
        U8 carry2 = (decU_intZ > (0xFFFFFFFF - POLYTEXT_DecMapUIntZBuf - carry));
        decU_intZ += POLYTEXT_DecMapUIntZBuf + carry;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry2) & 0xFF);

        carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((POLYTEXT_IntMapV & 0xFF) + (((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        mapIndex &= POLYTEXT_RepMaskPatch;
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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureChromaKeyFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_RepMaskPatch = RepMask;
    POLYTEXT_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    POLYTEXT_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_PtrZBuffer1 = PtrZBuffer;
    POLYTEXT_DecZBuf = Fill_ZBuf_XSlope << 24;
    POLYTEXT_IntZBuf = (Fill_ZBuf_XSlope >> 8) & 0xFFFF;
    POLYTEXT_DecMapUIntZBuf = (Fill_MapU_XSlope << 16) | POLYTEXT_IntZBuf;
    POLYTEXT_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 lineV = v;
      U32 lineU = u;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
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
      }
      // @@FastInit:
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8)); // eax
      U32 decZ = Fill_CurZBufMin << 24; // esi
      U32 offset = line - (U8 *)Log; // edi
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16); // edx

      do
      {
        // @@Each_X:
        if (POLYTEXT_PtrZBufferPatch[offset] >= (decU_intZ & 0xFFFF))
        {
          U8 textureColor = POLYTEXT_PtrMapPatch[mapIndex];
          
          // Chroma key test - skip transparent pixels (color 0)
          if (textureColor != 0)
          {
            POLYTEXT_PtrZBuffer1[offset] = decU_intZ & 0xFFFF;
            U8 foggedColor = Fill_Logical_Palette[textureColor];
            POLYTEXT_PtrLog[offset] = foggedColor;
          }
        }

        // @@NoDisp:
        U8 carry = (decZ > (0xFFFFFFFF - POLYTEXT_DecZBuf));
        decZ += POLYTEXT_DecZBuf;
        U8 carry2 = (decU_intZ > (0xFFFFFFFF - POLYTEXT_DecMapUIntZBuf - carry));
        decU_intZ += POLYTEXT_DecMapUIntZBuf + carry;

        offset++;

        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry2) & 0xFF);

        carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((POLYTEXT_IntMapV & 0xFF) + (((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        mapIndex &= POLYTEXT_RepMaskPatch;
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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Converted from ASM - Basic texture rendering with Z-buffer testing (no Z-write)
S32 Filler_TextureNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    
    // Initialize texture mapping components
    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_RepMaskPatch = RepMask;
    
    // Precalculate texture coordinate increments
    U32 mapUSlope = Fill_MapU_XSlope;
    U32 mapVSlope = Fill_MapV_XSlope;
    U32 uInt = (mapUSlope >> 16) & 0xFF;
    U32 vInt = (mapVSlope & 0xFF0000) >> 8;
    Precalc_Inc = uInt | vInt;
    POLYTEXT_IntMapU = uInt & 0xFF;
    
    // Initialize Z-buffer components
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_DecZBuf = Fill_ZBuf_XSlope << (32-8);
    POLYTEXT_IntZBuf = Fill_ZBuf_XSlope >> 8;
    POLYTEXT_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    
    // Update Z-buffer for scanline
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 startX = fillCurXMin >> 16;
    U32 endX = fillCurXMax >> 16;
    U32 fillWidth = endX - startX;
    
    if (endX > startX)
    {
      PTR_U8 dstPtr = Fill_CurOffLine + startX;
      
      // Calculate sub-pixel offset for texture coordinates
      U32 subPixelOffset = fillCurXMin & 0xFFFF;
      subPixelOffset ^= 0xFFFF; // Invert for proper calculation
      
      // Calculate coordinate adjustments for sub-pixel precision
      if (fillWidth > 16)
      {
        // Adjust U coordinate
        S32 uAdjust = (S32)(Fill_MapU_XSlope * subPixelOffset) >> 16;
        u += uAdjust;
        
        // Adjust V coordinate  
        S32 vAdjust = (S32)(Fill_MapV_XSlope * subPixelOffset) >> 16;
        v += vAdjust;
      }
      
      // Prepare texture coordinate extraction
      U32 texU = (u >> 16) & 0xFF;
      U32 texV = (v & 0xFF0000) >> 8;
      U32 texCoord = texU | texV;
      
      // Adjust destination pointer for Log offset
      PTR_U8 logPtr = (PTR_U8)Log;
      S32 dstOffset = dstPtr - logPtr;
      
      // Prepare sub-pixel components for interpolation
      U32 subU = u << 16;
      U32 subV = v << 16;
      U32 combinedIncrement = Precalc_Inc | subU;
      texCoord &= RepMask;
      
      // Prepare Z-buffer values
      U32 zBufHigh = Fill_CurZBufMin << (32-8);
      U32 zBufLow = Fill_CurZBufMin >> 8;
      
      // Render scanline pixels
      for (U32 x = 0; x < fillWidth; x++)
      {
        // Perform Z-buffer test (compare depth)
        U16 currentDepth = zBufLow & 0xFFFF;
        U16 bufferDepth = POLYTEXT_PtrZBufferPatch[dstOffset];
        
        if (currentDepth >= bufferDepth)
        {
          // Depth test passed - sample texture and write pixel
          U8 texel = POLYTEXT_PtrMapPatch[texCoord & POLYTEXT_RepMaskPatch];
          POLYTEXT_PtrLog[dstOffset] = texel;
        }
        
        // Update texture coordinates with sub-pixel precision
        combinedIncrement += POLYTEXT_DecMapU;
        texU += (combinedIncrement < POLYTEXT_DecMapU) ? POLYTEXT_IntMapU + 1 : POLYTEXT_IntMapU; // Handle carry
        
        subV += POLYTEXT_DecMapV;
        texV += (subV < POLYTEXT_DecMapV) ? ((subV >> 24) & 0xFF) + 1 : (subV >> 24) & 0xFF; // Handle carry
        
        // Update Z-buffer values
        zBufHigh += POLYTEXT_DecZBuf;
        zBufLow += POLYTEXT_IntZBuf + ((zBufHigh < POLYTEXT_DecZBuf) ? 1 : 0); // Handle carry
        
        // Update texture coordinate for next pixel
        texCoord = (texU & 0xFF) | ((texV & 0xFF) << 8);
        texCoord &= POLYTEXT_RepMaskPatch;
        
        // Move to next pixel
        dstOffset++;
      }
    }

    diffY--;
    if (diffY == 0)
      break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    
    // Update Z-buffer for next scanline
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Converted from ASM - Texture rendering with chroma key and Z-buffer testing (no Z-write)
S32 Filler_TextureChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    
    // Initialize texture mapping components
    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_RepMaskPatch = RepMask;
    
    // Precalculate texture coordinate increments
    U32 mapUSlope = Fill_MapU_XSlope;
    U32 mapVSlope = Fill_MapV_XSlope;
    U32 uInt = (mapUSlope >> 16) & 0xFF;
    U32 vInt = (mapVSlope & 0xFF0000) >> 8;
    Precalc_Inc = uInt | vInt;
    POLYTEXT_IntMapU = uInt & 0xFF;
    
    // Initialize Z-buffer components
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_DecZBuf = Fill_ZBuf_XSlope << (32-8);
    POLYTEXT_IntZBuf = Fill_ZBuf_XSlope >> 8;
    POLYTEXT_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    
    // Update Z-buffer for scanline
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 startX = fillCurXMin >> 16;
    U32 endX = fillCurXMax >> 16;
    U32 fillWidth = endX - startX;
    
    if (endX > startX)
    {
      PTR_U8 dstPtr = Fill_CurOffLine + startX;
      
      // Calculate sub-pixel offset for texture coordinates
      U32 subPixelOffset = fillCurXMin & 0xFFFF;
      subPixelOffset ^= 0xFFFF; // Invert for proper calculation
      
      // Calculate coordinate adjustments for sub-pixel precision
      if (fillWidth > 16)
      {
        // Adjust U coordinate
        S32 uAdjust = (S32)(Fill_MapU_XSlope * subPixelOffset) >> 16;
        u += uAdjust;
        
        // Adjust V coordinate  
        S32 vAdjust = (S32)(Fill_MapV_XSlope * subPixelOffset) >> 16;
        v += vAdjust;
      }
      
      // Prepare texture coordinate extraction
      U32 texU = (u >> 16) & 0xFF;
      U32 texV = (v & 0xFF0000) >> 8;
      U32 texCoord = texU | texV;
      
      // Adjust destination pointer for Log offset
      PTR_U8 logPtr = (PTR_U8)Log;
      S32 dstOffset = dstPtr - logPtr;
      
      // Prepare sub-pixel components for interpolation
      U32 subU = u << 16;
      U32 subV = v << 16;
      U32 combinedIncrement = Precalc_Inc | subU;
      texCoord &= RepMask;
      
      // Prepare Z-buffer values
      U32 zBufHigh = Fill_CurZBufMin << (32-8);
      U32 zBufLow = Fill_CurZBufMin >> 8;
      
      // Render scanline pixels
      for (U32 x = 0; x < fillWidth; x++)
      {
        // Perform Z-buffer test (compare depth)
        U16 currentDepth = zBufLow & 0xFFFF;
        U16 bufferDepth = POLYTEXT_PtrZBufferPatch[dstOffset];
        
        if (currentDepth >= bufferDepth)
        {
          // Depth test passed - sample texture
          U8 texel = POLYTEXT_PtrMapPatch[texCoord & POLYTEXT_RepMaskPatch];
          
          // Test for chroma key (transparent pixel)
          if (texel != 0)
          {
            // Chroma key test passed - write pixel
            POLYTEXT_PtrLog[dstOffset] = texel;
          }
        }
        
        // Update texture coordinates with sub-pixel precision
        combinedIncrement += POLYTEXT_DecMapU;
        texU += (combinedIncrement < POLYTEXT_DecMapU) ? POLYTEXT_IntMapU + 1 : POLYTEXT_IntMapU; // Handle carry
        
        subV += POLYTEXT_DecMapV;
        texV += (subV < POLYTEXT_DecMapV) ? ((subV >> 24) & 0xFF) + 1 : (subV >> 24) & 0xFF; // Handle carry
        
        // Update Z-buffer values
        zBufHigh += POLYTEXT_DecZBuf;
        zBufLow += POLYTEXT_IntZBuf + ((zBufHigh < POLYTEXT_DecZBuf) ? 1 : 0); // Handle carry
        
        // Update texture coordinate for next pixel
        texCoord = (texU & 0xFF) | ((texV & 0xFF) << 8);
        texCoord &= POLYTEXT_RepMaskPatch;
        
        // Move to next pixel
        dstOffset++;
      }
    }

    diffY--;
    if (diffY == 0)
      break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    
    // Update Z-buffer for next scanline
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_TextureFlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_DecMapUIntZBuf = (Fill_MapU_XSlope << 16) | ((Fill_ZBuf_XSlope >> 8) & 0xFFFF);
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_PtrCLUT = Fill_Color.Ptr;
    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_RepMaskPatch = RepMask;
    POLYTEXT_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    POLYTEXT_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_DecZBuf = Fill_ZBuf_XSlope << 24;
    POLYTEXT_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 lineV = v;
      U32 lineU = u;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
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
      U32 lineVDec = lineV << 16; // ebp
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8)); // eax
      U32 zDec = Fill_CurZBufMin << 24; // esi
      U32 offset = line - (U8 *)Log; // edi
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16); // edx

      do
      {
        // @@Each_X:
        if (POLYTEXT_PtrZBufferPatch[offset] >= (decU_intZ & 0xFFFF))
        {
          U8 colorIndex = POLYTEXT_PtrMapPatch[mapIndex];
          POLYTEXT_PtrLog[offset] = POLYTEXT_PtrCLUT[colorIndex];
        }

        U8 carry = (zDec > (0xFFFFFFFF - POLYTEXT_DecZBuf));
        zDec += POLYTEXT_DecZBuf;
        U8 carry2 = (decU_intZ > (0xFFFFFFFF - POLYTEXT_DecMapUIntZBuf - carry));
        decU_intZ += POLYTEXT_DecMapUIntZBuf + carry;

        offset++;

        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry2) & 0xFF);

        carry = (lineVDec > (0xFFFFFFFF - POLYTEXT_DecMapV));
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((POLYTEXT_IntMapV & 0xFF) + (((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        mapIndex &= POLYTEXT_RepMaskPatch;
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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Converted from ASM - Flat texture rendering with chroma key transparency, Z-buffer testing (no Z-write)
S32 Filler_TextureFlatChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_PtrCLUT = Fill_Color.Ptr;
    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_RepMaskPatch = RepMask;
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_PtrLog = (PTR_U8)Log;

    // Precalc texture mapping parameters for flat shading
    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    POLYTEXT_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;

    // Z-buffer parameters
    POLYTEXT_DecZBuf = (U32)Fill_ZBuf_XSlope << (32 - 8);
    POLYTEXT_IntZBuf = ((U32)Fill_ZBuf_XSlope >> 8) & 0xFFFF;
    POLYTEXT_DecMapUIntZBuf = POLYTEXT_DecMapU | POLYTEXT_IntZBuf;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      PTR_U8 destPtr = (PTR_U8)Fill_CurOffLine + (fillCurXMin >> 16);
      PTR_U8 logPtr = (PTR_U8)Log;
      
      // Calculate starting texture coordinates
      U32 subPixelX = fillCurXMin & 0xFFFF;
      subPixelX ^= 0xFFFF;
      
      S32 startU = u;
      S32 startV = v;
      
      // Sub-pixel correction for texture coordinates (for wide spans)
      if (fillWidth > 16)
      {
        S32 uCorrection = (Fill_MapU_XSlope * (S32)subPixelX) >> 16;
        S32 vCorrection = (Fill_MapV_XSlope * (S32)subPixelX) >> 16;
        startU += uCorrection;
        startV += vCorrection;
        
        // Preload destination cache lines for better performance
        PTR_U8 cachePtr = (PTR_U8)((uintptr_t)destPtr & ~31);
        PTR_U8 cacheEnd = (PTR_U8)((uintptr_t)(destPtr + fillWidth) & ~31);
        while (cachePtr <= cacheEnd)
        {
          volatile U8 dummy = *cachePtr;
          (void)dummy; // Suppress unused variable warning
          cachePtr += 32;
        }
      }
      
      // Setup texture coordinate interpolation
      U32 texU = ((U32)startU >> 16) & 0xFF;
      U32 texV = ((U32)startV >> 8) & 0xFF00;
      U32 texCoord = texU | texV;
      
      U32 fracU = (U32)startU << 16;
      U32 fracV = (U32)startV << 16;
      
      // Z-buffer setup
      U32 zFrac = (U32)Fill_CurZBufMin << (32 - 8);
      U32 zInt = (U32)Fill_CurZBufMin >> 8;
      U32 combinedZ = zInt | fracU;
      
      // Adjust destination pointer for log addressing
      S32 logOffset = (S32)((PTR_U8)Log - destPtr);
      
      // Main rendering loop
      for (U32 x = 0; x < fillWidth; x++)
      {
        // Z-buffer test (no Z-write for NZW variant)
        PTR_U16 zBufferPtr = POLYTEXT_PtrZBufferPatch + (uintptr_t)destPtr;
        if (*zBufferPtr >= (U16)(zInt & 0xFFFF))
        {
          // Sample texture and apply chroma key test
          U8 texel = POLYTEXT_PtrMapPatch[texCoord & POLYTEXT_RepMaskPatch];
          if (texel != 0)
          {
            // Apply color lookup for flat shading and write pixel
            U8 finalColor = POLYTEXT_PtrCLUT[texel];
            destPtr[logOffset] = finalColor;
          }
        }
        
      skipPixel:
        // Advance to next pixel
        destPtr++;
        
        // Update Z-buffer coordinate
        zFrac += POLYTEXT_DecZBuf;
        combinedZ += POLYTEXT_DecMapUIntZBuf;
        zInt = combinedZ >> 16;
        
        // Update texture coordinates with carry propagation
        fracU += POLYTEXT_DecMapU;
        texU += (fracU < POLYTEXT_DecMapU) ? 1 : 0; // Carry from U
        texU += POLYTEXT_IntMapU;
        
        fracV += POLYTEXT_DecMapV;
        texV += (fracV < POLYTEXT_DecMapV) ? 0x100 : 0; // Carry from V (shifted for upper byte)
        texV += (POLYTEXT_IntMapV << 8);
        
        // Mask texture coordinates
        texCoord = (texU & 0xFF) | (texV & 0xFF00);
        texCoord &= POLYTEXT_RepMaskPatch;
      }
    }

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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Converted from ASM - Texture rendering with fog effects, Z-buffer testing (no Z-write)
S32 Filler_TextureFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_RepMaskPatch = RepMask;
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_PtrLog = (PTR_U8)Log;

    // Precalc texture mapping parameters
    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    POLYTEXT_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;

    // Z-buffer parameters
    POLYTEXT_DecZBuf = (U32)Fill_ZBuf_XSlope << (32 - 8);
    POLYTEXT_IntZBuf = ((U32)Fill_ZBuf_XSlope >> 8) & 0xFFFF;
    POLYTEXT_DecMapUIntZBuf = POLYTEXT_DecMapU | POLYTEXT_IntZBuf;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      PTR_U8 destPtr = (PTR_U8)Fill_CurOffLine + (fillCurXMin >> 16);
      PTR_U8 logPtr = (PTR_U8)Log;
      
      // Calculate starting texture coordinates
      U32 subPixelX = fillCurXMin & 0xFFFF;
      subPixelX ^= 0xFFFF;
      
      S32 startU = u;
      S32 startV = v;
      
      // Sub-pixel correction for texture coordinates (for wide spans)
      if (fillWidth > 16)
      {
        S32 uCorrection = (Fill_MapU_XSlope * (S32)subPixelX) >> 16;
        S32 vCorrection = (Fill_MapV_XSlope * (S32)subPixelX) >> 16;
        startU += uCorrection;
        startV += vCorrection;
        
        // Preload destination cache lines for better performance
        PTR_U8 cachePtr = (PTR_U8)((uintptr_t)destPtr & ~31);
        PTR_U8 cacheEnd = (PTR_U8)((uintptr_t)(destPtr + fillWidth) & ~31);
        while (cachePtr <= cacheEnd)
        {
          volatile U8 dummy = *cachePtr;
          (void)dummy; // Suppress unused variable warning
          cachePtr += 32;
        }
      }
      
      // Setup texture coordinate interpolation
      U32 texU = ((U32)startU >> 16) & 0xFF;
      U32 texV = ((U32)startV >> 8) & 0xFF00;
      U32 texCoord = texU | texV;
      
      U32 fracU = (U32)startU << 16;
      U32 fracV = (U32)startV << 16;
      
      // Z-buffer setup
      U32 zFrac = (U32)Fill_CurZBufMin << (32 - 8);
      U32 zInt = (U32)Fill_CurZBufMin >> 8;
      U32 combinedZ = zInt | fracU;
      
      // Adjust destination pointer for log addressing
      S32 logOffset = (S32)(logPtr - destPtr);
      
      // Main rendering loop
      for (U32 x = 0; x < fillWidth; x++)
      {
        // Z-buffer test (no Z-write for NZW variant)
        PTR_U16 zBufferPtr = POLYTEXT_PtrZBufferPatch + (uintptr_t)destPtr;
        if (*zBufferPtr >= (U16)(zInt & 0xFFFF))
        {
          // Sample texture
          U8 texel = POLYTEXT_PtrMapPatch[texCoord & POLYTEXT_RepMaskPatch];
          
          // Apply fog effect using Fill_Logical_Palette and write pixel
          U8 foggedColor = Fill_Logical_Palette[texel];
          destPtr[logOffset] = foggedColor;
        }
        
        // Advance to next pixel
        destPtr++;
        
        // Update Z-buffer coordinate
        zFrac += POLYTEXT_DecZBuf;
        combinedZ += POLYTEXT_DecMapUIntZBuf;
        zInt = combinedZ >> 16;
        
        // Update texture coordinates with carry propagation
        fracU += POLYTEXT_DecMapU;
        texU += (fracU < POLYTEXT_DecMapU) ? 1 : 0; // Carry from U
        texU += POLYTEXT_IntMapU;
        
        fracV += POLYTEXT_DecMapV;
        texV += (fracV < POLYTEXT_DecMapV) ? 0x100 : 0; // Carry from V (shifted for upper byte)
        texV += (POLYTEXT_IntMapV << 8);
        
        // Mask texture coordinates
        texCoord = (texU & 0xFF) | (texV & 0xFF00);
        texCoord &= POLYTEXT_RepMaskPatch;
      }
    }

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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Converted from ASM - Texture rendering with chroma key transparency, fog effects, Z-buffer testing (no Z-write)
S32 Filler_TextureChromaKeyFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXT_PtrMapPatch = PtrMap;
    POLYTEXT_RepMaskPatch = RepMask;
    POLYTEXT_PtrZBufferPatch = PtrZBuffer;
    POLYTEXT_PtrLog = (PTR_U8)Log;

    // Precalc texture mapping parameters
    POLYTEXT_DecMapU = Fill_MapU_XSlope << 16;
    POLYTEXT_DecMapV = Fill_MapV_XSlope << 16;
    POLYTEXT_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    POLYTEXT_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;

    // Z-buffer parameters
    POLYTEXT_DecZBuf = (U32)Fill_ZBuf_XSlope << (32 - 8);
    POLYTEXT_IntZBuf = ((U32)Fill_ZBuf_XSlope >> 8) & 0xFFFF;
    POLYTEXT_DecMapUIntZBuf = POLYTEXT_DecMapU | POLYTEXT_IntZBuf;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      PTR_U8 destPtr = (PTR_U8)Fill_CurOffLine + (fillCurXMin >> 16);
      PTR_U8 logPtr = (PTR_U8)Log;
      
      // Calculate starting texture coordinates
      U32 subPixelX = fillCurXMin & 0xFFFF;
      subPixelX ^= 0xFFFF;
      
      S32 startU = u;
      S32 startV = v;
      
      // Sub-pixel correction for texture coordinates (for wide spans)
      if (fillWidth > 16)
      {
        S32 uCorrection = (Fill_MapU_XSlope * (S32)subPixelX) >> 16;
        S32 vCorrection = (Fill_MapV_XSlope * (S32)subPixelX) >> 16;
        startU += uCorrection;
        startV += vCorrection;
        
        // Preload destination cache lines for better performance
        PTR_U8 cachePtr = (PTR_U8)((uintptr_t)destPtr & ~31);
        PTR_U8 cacheEnd = (PTR_U8)((uintptr_t)(destPtr + fillWidth) & ~31);
        while (cachePtr <= cacheEnd)
        {
          volatile U8 dummy = *cachePtr;
          (void)dummy; // Suppress unused variable warning
          cachePtr += 32;
        }
      }
      
      // Setup texture coordinate interpolation
      U32 texU = ((U32)startU >> 16) & 0xFF;
      U32 texV = ((U32)startV >> 8) & 0xFF00;
      U32 texCoord = texU | texV;
      
      U32 fracU = (U32)startU << 16;
      U32 fracV = (U32)startV << 16;
      
      // Z-buffer setup
      U32 zFrac = (U32)Fill_CurZBufMin << (32 - 8);
      U32 zInt = (U32)Fill_CurZBufMin >> 8;
      U32 combinedZ = zInt | fracU;
      
      // Adjust destination pointer for log addressing
      S32 logOffset = (S32)(logPtr - destPtr);
      
      // Main rendering loop
      for (U32 x = 0; x < fillWidth; x++)
      {
        // Z-buffer test (no Z-write for NZW variant)
        PTR_U16 zBufferPtr = POLYTEXT_PtrZBufferPatch + (uintptr_t)destPtr;
        if (*zBufferPtr >= (U16)(zInt & 0xFFFF))
        {
          // Sample texture
          U8 texel = POLYTEXT_PtrMapPatch[texCoord & POLYTEXT_RepMaskPatch];
          
          // Chroma key test - skip transparent pixels (texel == 0)
          if (texel != 0)
          {
            // Apply fog effect using Fill_Logical_Palette and write pixel
            U8 foggedColor = Fill_Logical_Palette[texel];
            destPtr[logOffset] = foggedColor;
          }
        }
        
        // Advance to next pixel
        destPtr++;
        
        // Update Z-buffer coordinate
        zFrac += POLYTEXT_DecZBuf;
        combinedZ += POLYTEXT_DecMapUIntZBuf;
        zInt = combinedZ >> 16;
        
        // Update texture coordinates with carry propagation
        fracU += POLYTEXT_DecMapU;
        texU += (fracU < POLYTEXT_DecMapU) ? 1 : 0; // Carry from U
        texU += POLYTEXT_IntMapU;
        
        fracV += POLYTEXT_DecMapV;
        texV += (fracV < POLYTEXT_DecMapV) ? 0x100 : 0; // Carry from V (shifted for upper byte)
        texV += (POLYTEXT_IntMapV << 8);
        
        // Mask texture coordinates
        texCoord = (texU & 0xFF) | (texV & 0xFF00);
        texCoord &= POLYTEXT_RepMaskPatch;
      }
    }

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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}
