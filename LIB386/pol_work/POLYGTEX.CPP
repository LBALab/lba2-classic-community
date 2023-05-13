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
#include <SVGA/SCREEN.H>

#include <stdio.h>

PTR_U8 GTEX_PtrLog = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrCLUT = (PTR_U8)0x12345678;
PTR_U16 GTEX_PtrZBuffer1 = NULL;
PTR_U16 GTEX_PtrZBufferPatch = NULL;
PTR_U8 GTEX_PtrMap2 = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrMapPatch = (PTR_U8)0x12345678;
U32 GTEX_RepMask2 = 0x12345678;
U32 GTEX_RepMaskPatch = 0x12345678;
U32 GTEX_DecZBuf_Gour = 0x12345678;
U32 GTEX_DecMapU = 0x12345678;
U32 GTEX_DecMapU2 = 0x12345678;
U32 GTEX_DecMapV_Gour = 0x12345678;
U32 GTEX_DecMapV_Gour2 = 0x12345678;
U32 GTEX_DecMapV_IntZBuf = 0x12345678;
U8 GTEX_IntMapV = 0x12;
U8 GTEX_IntMapV2 = 0x12;
U8 GTEX_IntMapU = 0x12;
U8 GTEX_IntMapU2 = 0x12;

// Verified
S32 Filler_TextureGouraud(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
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
    GTEX_DecMapV_Gour = (Fill_MapV_XSlope << 16) | ((Fill_Gouraud_XSlope >> 8) & 0xFFFF);
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  while (1)
  {
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *screen = (fillCurXMin >> 16) + curOffLine;

      U32 lineV = v;
      U32 lineU = u;
      U32 curGouraudMin = Fill_CurGouraudMin;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (diffX > 16)
      {
        U64 addGouraud = ((U64)factor * (U64)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        curGouraudMin += addGouraudDec;

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

      U32 gouraudXSlope = Fill_Gouraud_XSlope;
      gouraudXSlope = (gouraudXSlope & 0xFFFF00FF) | (((lineV >> 16) & 0xFF) << 8);
      U32 decU = lineU << 16; // esi
      gouraudXSlope = (gouraudXSlope & 0xFFFFFF00) | ((lineU >> 16) & 0xFF);
      U32 decV = (lineV << 16) | (curGouraudMin >> 8) & 0xFFFF; // edx
      U32 colorIndex = (curGouraudMin >> 8) & 0xFFFF; // ebx
      U32 mask = RepMask & gouraudXSlope; // eax
      do
      {
        colorIndex = (colorIndex & 0xFFFFFF00) | GTEX_PtrMapPatch[mask];

        U8 carry = ((U64)decU + (U64)GTEX_DecMapU) > 0xFFFFFFFF;
        decU += GTEX_DecMapU;
        mask = (mask & 0xFFFFFF00) | (((GTEX_IntMapU & 0xFF) + carry + (mask & 0xFF)) & 0xFF);

        carry = ((U64)decV + (U64)GTEX_DecMapV_Gour) > 0xFFFFFFFF;
        decV += GTEX_DecMapV_Gour;
        mask = (mask & 0xFFFF00FF) | (((((mask >> 8) & 0xFF) + (GTEX_IntMapV & 0xFF) + carry) & 0xFF) << 8);

        colorIndex = (colorIndex & 0xFFFFFF00) | GTEX_PtrCLUT[colorIndex];

        *screen = colorIndex & 0xFF;
        screen++;

        colorIndex = (colorIndex & 0xFFFF00FF) | (decV & 0xFF00);

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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_TextureDither(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  U32 u = Fill_CurMapUMin; // esi
  U32 v = Fill_CurMapVMin; // ebp
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
    GTEX_DecMapV_Gour = (Fill_MapV_XSlope << 16) | ((Fill_Gouraud_XSlope >> 8) & 0xFFFF);
    GTEX_RepMaskPatch = RepMask;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  while (1)
  {
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *screen = (fillCurXMin >> 16) + curOffLine;

      U32 lineV = v;
      U32 lineU = u;
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;

      if (diffX > 16)
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

      U32 gouraudXSlope = Fill_Gouraud_XSlope;
      gouraudXSlope = (gouraudXSlope & 0xFFFF00FF) | (((lineV >> 16) & 0xFF) << 8);
      U32 decU = lineU << 16; // esi
      gouraudXSlope = (gouraudXSlope & 0xFFFFFF00) | ((lineU >> 16) & 0xFF);
      U32 curGouraudMin = (Fill_CurGouraudMin >> 8) & 0xFFFF;
      U32 decV_gour = (lineV << 16) | curGouraudMin; // ebp
      U32 colorIndex = curGouraudMin; // ebx
      U32 mask = RepMask & gouraudXSlope; // eax
      do
      {
        colorIndex = (colorIndex & 0xFFFFFF00) | GTEX_PtrMapPatch[mask];

        U8 carry = ((U64)decU + (U64)GTEX_DecMapU) > 0xFFFFFFFF;
        decU += GTEX_DecMapU;
        mask = (mask & 0xFFFFFF00) | (((GTEX_IntMapU & 0xFF) + carry + (mask & 0xFF)) & 0xFF);

        carry = ((U64)decV_gour + (U64)GTEX_DecMapV_Gour) > 0xFFFFFFFF;
        decV_gour += GTEX_DecMapV_Gour;
        mask = (mask & 0xFFFF00FF) | (((((mask >> 8) & 0xFF) + (GTEX_IntMapV & 0xFF) + carry) & 0xFF) << 8);

        colorIndex = (colorIndex & 0xFFFFFF00) | GTEX_PtrCLUT[colorIndex];

        *screen = colorIndex & 0xFF;

        curGouraudMin &= 0xFFFF00FF;
        curGouraudMin += decV_gour;

        screen++;

        colorIndex = (colorIndex & 0xFFFF00FF) | (curGouraudMin & 0xFF00);
        mask &= GTEX_RepMaskPatch;
        --diffX;
      }
      while (diffX);
    }

    // @@EndScanLine:
    if (!--count)
    {
      break;
    }

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

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

// Verified
S32 Filler_TextureGouraudZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  Fill_CurY += count;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    GTEX_PtrZBufferPatch = PtrZBuffer;
    GTEX_PtrZBuffer1 = PtrZBuffer;
    GTEX_PtrLog = (U8 *)Log;
    GTEX_PtrMapPatch = PtrMap;
    GTEX_PtrCLUT = PtrCLUTGouraud;
    GTEX_RepMaskPatch = RepMask;
    GTEX_DecMapU = Fill_MapU_XSlope << 16;
    GTEX_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    GTEX_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    GTEX_DecMapV_IntZBuf = (Fill_MapV_XSlope << 16) | (Fill_ZBuf_XSlope >> 8);
    GTEX_DecZBuf_Gour = (Fill_ZBuf_XSlope << 24) | ((Fill_Gouraud_XSlope >> 8) & 0xFFFF);
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *screen = (fillCurXMin >> 16) + curOffLine;
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      U32 decZ_gour = Fill_CurGouraudMin;
      U32 decV_intZ = v;
      U32 decU = u;

      if (diffX > 16)
      {
        U64 addGouraud = ((U64)factor * (U64)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        decZ_gour += addGouraudDec;

        U64 addU = ((U64)factor * (U64)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        decU += addUDec;

        U64 addV = ((U64)factor * (U64)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = addVDec << 16;
        addVInt = addVInt >> 16;
        addVInt |= addVDec;
        decV_intZ += addVInt;

        // There was some @@Preload code here, but it did nothing other
        // than reading some bytes but not doing anything with it. It
        // also had the comment "Load dst in WB cache", which means nothing
        // to me. I'm guessing it was some kind of prefetching, but I'm
        // not sure. I'm leaving it out for now.
      }

      U32 offset = screen - (U8 *)Log; // edi

      U32 mask = (decU >> 16); // eax
      mask = (mask & 0xFFFF00FF) | (((decV_intZ >> 16) & 0xFF) << 8);
      mask &= RepMask;

      decU <<= 16; // esi
      decV_intZ <<= 16; // ebp
      decV_intZ |= Fill_CurZBufMin >> 8;

      decZ_gour = (decZ_gour >> 8) | (Fill_CurZBufMin << 24); // edx
      U32 colorIndex = 0; // ebx
      colorIndex = (colorIndex & 0xFFFF00FF) | (decZ_gour & 0xFF00);

      do
      {
        if (GTEX_PtrZBufferPatch[offset] >= (decV_intZ & 0xFFFF))
        {
          colorIndex = (colorIndex & 0xFFFFFF00) | (GTEX_PtrMapPatch[mask] & 0xFF);
          GTEX_PtrZBuffer1[offset] = (decV_intZ & 0xFFFF);
          colorIndex = (colorIndex & 0xFFFFFF00) | GTEX_PtrCLUT[colorIndex];
          GTEX_PtrLog[offset] = colorIndex & 0xFF;
        }

        U8 carry = ((U64)decU + (U64)GTEX_DecMapU) > 0xFFFFFFFF;
        decU += GTEX_DecMapU;

        mask = (mask & 0xFFFFFF00) | (((GTEX_IntMapU & 0xFF) + carry + (mask & 0xFF)) & 0xFF);

        carry = ((U64)decZ_gour + (U64)GTEX_DecZBuf_Gour) > 0xFFFFFFFF;
        decZ_gour += GTEX_DecZBuf_Gour;

        U8 carry2 = ((U64)decV_intZ + (U64)GTEX_DecMapV_IntZBuf + carry) > 0xFFFFFFFF;
        decV_intZ += carry + GTEX_DecMapV_IntZBuf;

        offset++;

        mask = (mask & 0xFFFF00FF) | (((((mask >> 8) & 0xFF) + (GTEX_IntMapV & 0xFF) + carry2) & 0xFF) << 8);

        colorIndex = (colorIndex & 0xFFFF00FF) | (decZ_gour & 0xFF00);
        mask &= GTEX_RepMaskPatch;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_TextureGouraudChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  Fill_CurY += count;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    GTEX_PtrZBufferPatch = PtrZBuffer;
    GTEX_PtrZBuffer1 = PtrZBuffer;
    GTEX_PtrLog = (U8 *)Log;
    GTEX_PtrMapPatch = PtrMap;
    GTEX_PtrCLUT = PtrCLUTGouraud;
    GTEX_RepMaskPatch = RepMask;
    GTEX_DecMapU = Fill_MapU_XSlope << 16;
    GTEX_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    GTEX_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    GTEX_DecMapV_IntZBuf = (Fill_MapV_XSlope << 16) | (Fill_ZBuf_XSlope >> 8);
    GTEX_DecZBuf_Gour = (Fill_ZBuf_XSlope << 24) | ((Fill_Gouraud_XSlope >> 8) & 0xFFFF);
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *screen = (fillCurXMin >> 16) + curOffLine;
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      U32 decZ_gour = Fill_CurGouraudMin;
      U32 decV_intZ = v;
      U32 decU = u;

      if (diffX > 16)
      {
        U64 addGouraud = ((U64)factor * (U64)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        decZ_gour += addGouraudDec;

        U64 addU = ((U64)factor * (U64)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        decU += addUDec;

        U64 addV = ((U64)factor * (U64)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = addVDec << 16;
        addVInt = addVInt >> 16;
        addVInt |= addVDec;
        decV_intZ += addVInt;

        // There was some @@Preload code here, but it did nothing other
        // than reading some bytes but not doing anything with it. It
        // also had the comment "Load dst in WB cache", which means nothing
        // to me. I'm guessing it was some kind of prefetching, but I'm
        // not sure. I'm leaving it out for now.
      }

      U32 offset = screen - (U8 *)Log; // edi

      U32 mask = (decU >> 16); // eax
      mask = (mask & 0xFFFF00FF) | (((decV_intZ >> 16) & 0xFF) << 8);
      mask &= RepMask;

      decU <<= 16; // esi
      decV_intZ <<= 16; // ebp
      decV_intZ |= Fill_CurZBufMin >> 8;

      decZ_gour = (decZ_gour >> 8) | (Fill_CurZBufMin << 24); // edx
      U32 colorIndex = 0; // ebx
      colorIndex = (colorIndex & 0xFFFF00FF) | (decZ_gour & 0xFF00);

      do
      {
        U8 mapColor = GTEX_PtrMapPatch[mask];
        colorIndex = (colorIndex & 0xFFFFFF00) | mapColor;

        if (mapColor != 0 && GTEX_PtrZBufferPatch[offset] >= (decV_intZ & 0xFFFF))
        {
          GTEX_PtrZBuffer1[offset] = (decV_intZ & 0xFFFF);
          colorIndex = (colorIndex & 0xFFFFFF00) | GTEX_PtrCLUT[colorIndex];
          GTEX_PtrLog[offset] = colorIndex & 0xFF;
        }

        U8 carry = ((U64)decU + (U64)GTEX_DecMapU) > 0xFFFFFFFF;
        decU += GTEX_DecMapU;

        mask = (mask & 0xFFFFFF00) | (((GTEX_IntMapU & 0xFF) + carry + (mask & 0xFF)) & 0xFF);

        carry = ((U64)decZ_gour + (U64)GTEX_DecZBuf_Gour) > 0xFFFFFFFF;
        decZ_gour += GTEX_DecZBuf_Gour;

        U8 carry2 = ((U64)decV_intZ + (U64)GTEX_DecMapV_IntZBuf + carry) > 0xFFFFFFFF;
        decV_intZ += carry + GTEX_DecMapV_IntZBuf;

        offset++;

        mask = (mask & 0xFFFF00FF) | (((((mask >> 8) & 0xFF) + (GTEX_IntMapV & 0xFF) + carry2) & 0xFF) << 8);

        colorIndex = (colorIndex & 0xFFFF00FF) | (decZ_gour & 0xFF00);
        mask &= GTEX_RepMaskPatch;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_TextureGouraudNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  Fill_CurY += count;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    GTEX_PtrZBufferPatch = PtrZBuffer;
    GTEX_PtrLog = (U8 *)Log;
    GTEX_PtrMapPatch = PtrMap;
    GTEX_PtrCLUT = PtrCLUTGouraud;
    GTEX_RepMaskPatch = RepMask;
    GTEX_DecMapU = Fill_MapU_XSlope << 16;
    GTEX_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    GTEX_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    GTEX_DecMapV_IntZBuf = (Fill_MapV_XSlope << 16) | (Fill_ZBuf_XSlope >> 8);
    GTEX_DecZBuf_Gour = (Fill_ZBuf_XSlope << 24) | ((Fill_Gouraud_XSlope >> 8) & 0xFFFF);
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *screen = (fillCurXMin >> 16) + curOffLine;
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      U32 decZ_gour = Fill_CurGouraudMin;
      U32 decV_intZ = v;
      U32 decU = u;

      if (diffX > 16)
      {
        U64 addGouraud = ((U64)factor * (U64)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        decZ_gour += addGouraudDec;

        U64 addU = ((U64)factor * (U64)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        decU += addUDec;

        U64 addV = ((U64)factor * (U64)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = addVDec << 16;
        addVInt = addVInt >> 16;
        addVInt |= addVDec;
        decV_intZ += addVInt;

        // There was some @@Preload code here, but it did nothing other
        // than reading some bytes but not doing anything with it. It
        // also had the comment "Load dst in WB cache", which means nothing
        // to me. I'm guessing it was some kind of prefetching, but I'm
        // not sure. I'm leaving it out for now.
      }

      U32 offset = screen - (U8 *)Log; // edi

      U32 mask = (decU >> 16); // eax
      mask = (mask & 0xFFFF00FF) | (((decV_intZ >> 16) & 0xFF) << 8);
      mask &= RepMask;

      decU <<= 16; // esi
      decV_intZ <<= 16; // ebp
      decV_intZ |= Fill_CurZBufMin >> 8;

      decZ_gour = (decZ_gour >> 8) | (Fill_CurZBufMin << 24); // edx
      U32 colorIndex = 0; // ebx
      colorIndex = (colorIndex & 0xFFFF00FF) | (decZ_gour & 0xFF00);

      do
      {
        if (GTEX_PtrZBufferPatch[offset] >= (decV_intZ & 0xFFFF))
        {
          colorIndex = (colorIndex & 0xFFFFFF00) | (GTEX_PtrMapPatch[mask] & 0xFF);
          colorIndex = (colorIndex & 0xFFFFFF00) | GTEX_PtrCLUT[colorIndex];
          GTEX_PtrLog[offset] = colorIndex & 0xFF;
        }

        U8 carry = ((U64)decU + (U64)GTEX_DecMapU) > 0xFFFFFFFF;
        decU += GTEX_DecMapU;

        mask = (mask & 0xFFFFFF00) | (((GTEX_IntMapU & 0xFF) + carry + (mask & 0xFF)) & 0xFF);

        carry = ((U64)decZ_gour + (U64)GTEX_DecZBuf_Gour) > 0xFFFFFFFF;
        decZ_gour += GTEX_DecZBuf_Gour;

        U8 carry2 = ((U64)decV_intZ + (U64)GTEX_DecMapV_IntZBuf + carry) > 0xFFFFFFFF;
        decV_intZ += carry + GTEX_DecMapV_IntZBuf;

        offset++;

        mask = (mask & 0xFFFF00FF) | (((((mask >> 8) & 0xFF) + (GTEX_IntMapV & 0xFF) + carry2) & 0xFF) << 8);

        colorIndex = (colorIndex & 0xFFFF00FF) | (decZ_gour & 0xFF00);
        mask &= GTEX_RepMaskPatch;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureGouraudChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

