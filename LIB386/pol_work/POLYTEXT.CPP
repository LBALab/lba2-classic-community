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
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00)); // eax
      U32 lineUDec = lineU << 16; // edx
      do
      {
        // @@Each_X:
        lineUDec = (lineUDec & 0xFFFFFF00) | PtrMap[mapIndex];

        U8 carry = ((U64)lineVDec + (U64)POLYTEXT_DecMapV) > 0xFFFFFFFF;
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | (((((Precalc_Inc >> 8) & 0xFF) + ((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        carry = ((U64)lineUDec + (U64)POLYTEXT_DecMapU) > 0xFFFFFFFF;
        lineUDec += POLYTEXT_DecMapU;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((Precalc_Inc & 0xFF) + (mapIndex & 0xFF) + carry) & 0xFF);

        *line = lineUDec & 0xFF;
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

        U8 carry = ((U64)incDecMapU + (U64)POLYTEXT_DecMapU) > 0xFFFFFFFF;
        incDecMapU += POLYTEXT_DecMapU;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry) & 0xFF);

        carry = ((U64)lineVDec + (U64)POLYTEXT_DecMapV) > 0xFFFFFFFF;
        lineVDec += POLYTEXT_DecMapV;
        mapIndex = (mapIndex & 0xFFFF00FF) | ((((incDecMapU >> 8) & 0xFF) + (((mapIndex >> 8) & 0xFF) + carry) & 0xFF) << 8);

        carry = ((U64)zDec + (U64)POLYTEXT_DecZBuf) > 0xFFFFFFFF;
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
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
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

        U8 carry = ((U64)zDec + (U64)POLYTEXT_DecZBuf) > 0xFFFFFFFF;
        zDec += POLYTEXT_DecZBuf;
        U8 carry2 = ((U64)decU_intZ + (U64)POLYTEXT_DecMapUIntZBuf + (U64)carry) > 0xFFFFFFFF;
        decU_intZ += POLYTEXT_DecMapUIntZBuf + carry;

        offset++;

        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry2) & 0xFF);

        carry = ((U64)lineVDec + (U64)POLYTEXT_DecMapV) > 0xFFFFFFFF;
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

        U8 carry = ((U64)zDec + (U64)POLYTEXT_DecZBuf) > 0xFFFFFFFF;
        zDec += POLYTEXT_DecZBuf;
        U8 carry2 = ((U64)decU_intZ + (U64)POLYTEXT_DecMapUIntZBuf + (U64)carry) > 0xFFFFFFFF;
        decU_intZ += POLYTEXT_DecMapUIntZBuf + carry;

        offset++;

        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry2) & 0xFF);

        carry = ((U64)lineVDec + (U64)POLYTEXT_DecMapV) > 0xFFFFFFFF;
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

        U8 carry = ((U64)decZ + (U64)POLYTEXT_DecZBuf) > 0xFFFFFFFF;
        decZ += POLYTEXT_DecZBuf;
        U8 carry2 = ((U64)decU_intZ + (U64)POLYTEXT_DecMapUIntZBuf + carry) > 0xFFFFFFFF;
        decU_intZ += POLYTEXT_DecMapUIntZBuf + carry;
        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry2) & 0xFF);

        carry = ((U64)lineVDec + (U64)POLYTEXT_DecMapV) > 0xFFFFFFFF;
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

        U8 carry = ((U64)zDec + (U64)POLYTEXT_DecZBuf) > 0xFFFFFFFF;
        zDec += POLYTEXT_DecZBuf;
        U8 carry2 = ((U64)decU_intZ + (U64)POLYTEXT_DecMapUIntZBuf + (U64)carry) > 0xFFFFFFFF;
        decU_intZ += POLYTEXT_DecMapUIntZBuf + carry;

        offset++;

        mapIndex = (mapIndex & 0xFFFFFF00) | (((POLYTEXT_IntMapU & 0xFF) + (mapIndex & 0xFF) + carry2) & 0xFF);

        carry = ((U64)lineVDec + (U64)POLYTEXT_DecMapV) > 0xFFFFFFFF;
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
