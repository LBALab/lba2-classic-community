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
#include <SYSTEM/DEBUG_TRACES.H>

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
    LIB386_TRACE_CPP("Filler_TextureGouraud", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TextureGouraud", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureGouraud", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureGouraud", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureGouraud", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureGouraud", "1", "entry");
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
      U32 curGouraudMin = Fill_CurGouraudMin;
      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (diffX > 16)
      {
        // Simple multiply for sub-pixel adjustment, matching assembly imul instruction
        S32 addGouraud = (S32)(((S64)(S32)factor * (S64)(S32)Fill_Gouraud_XSlope) >> 16);
        curGouraudMin += addGouraud;

        S32 addU = (S32)(((S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope) >> 16);
        lineU += addU;

        S32 addV = (S32)(((S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope) >> 16);
        lineV += addV;

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

        U8 carry = (decU > (0xFFFFFFFF - GTEX_DecMapU));
        decU += GTEX_DecMapU;
        mask = (mask & 0xFFFFFF00) | (((GTEX_IntMapU & 0xFF) + carry + (mask & 0xFF)) & 0xFF);

        carry = (decV > (0xFFFFFFFF - GTEX_DecMapV_Gour));
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
    LIB386_TRACE_CPP("Filler_TextureDither", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TextureDither", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureDither", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureDither", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureDither", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureDither", "1", "entry");
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
        U64 addU = ((S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        lineU += addUDec;

        U64 addV = ((S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope);
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

        U8 carry = (decU > (0xFFFFFFFF - GTEX_DecMapU));
        decU += GTEX_DecMapU;
        mask = (mask & 0xFFFFFF00) | (((GTEX_IntMapU & 0xFF) + carry + (mask & 0xFF)) & 0xFF);

        carry = (decV_gour > (0xFFFFFFFF - GTEX_DecMapV_Gour));
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
    LIB386_TRACE_CPP("Filler_TextureGouraudChromaKey", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TextureGouraudChromaKey", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureGouraudChromaKey", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureGouraudChromaKey", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureGouraudChromaKey", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureGouraudChromaKey", "1", "entry");
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
    GTEX_PtrMap2 = PtrMap;
    GTEX_PtrCLUT = PtrCLUTGouraud;
    GTEX_DecMapU = Fill_MapU_XSlope << 16;
    GTEX_DecMapU2 = Fill_MapU_XSlope << 16;
    GTEX_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    GTEX_IntMapU2 = (Fill_MapU_XSlope >> 16) & 0xFF;
    GTEX_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    GTEX_IntMapV2 = (Fill_MapV_XSlope >> 16) & 0xFF;
    GTEX_DecMapV_Gour = (Fill_MapV_XSlope << 16) | ((Fill_Gouraud_XSlope >> 8) & 0xFFFF);
    GTEX_DecMapV_Gour2 = (Fill_MapV_XSlope << 16) | ((Fill_Gouraud_XSlope >> 8) & 0xFFFF);
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
        U64 addGouraud = ((S64)(S32)factor * (S64)(S32)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        curGouraudMin += addGouraudDec;

        U64 addU = ((S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        lineU += addUDec;

        U64 addV = ((S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = addVDec << 16;
        addVInt = addVInt >> 16;
        addVInt |= addVDec;
        lineV += addVInt;
      }

      U32 gouraudXSlope = Fill_Gouraud_XSlope;
      gouraudXSlope = (gouraudXSlope & 0xFFFF00FF) | (((lineV >> 16) & 0xFF) << 8);
      U32 decU = lineU << 16; // esi
      gouraudXSlope = (gouraudXSlope & 0xFFFFFF00) | ((lineU >> 16) & 0xFF);
      U32 decV = lineV << 16; // ebp
      curGouraudMin = (curGouraudMin >> 8) & 0xFFFF;
      U32 decV_gour = curGouraudMin | decV; // edx
      U32 colorIndex = curGouraudMin; // ebx
      U32 mask = RepMask & gouraudXSlope; // eax

      // Main pixel loop with chroma key testing
      while (diffX > 0)
      {
        // Sample texture
        U8 texel = GTEX_PtrMapPatch[mask];

        // Update texture coordinates
        U8 carry = ((U64)decU + (U64)GTEX_DecMapU) > 0xFFFFFFFF;
        decU += GTEX_DecMapU;
        mask = (mask & 0xFFFFFF00) | (((GTEX_IntMapU & 0xFF) + carry + (mask & 0xFF)) & 0xFF);

        carry = ((U64)decV_gour + (U64)GTEX_DecMapV_Gour) > 0xFFFFFFFF;
        decV_gour += GTEX_DecMapV_Gour;
        mask = (mask & 0xFFFF00FF) | (((((mask >> 8) & 0xFF) + (GTEX_IntMapV & 0xFF) + carry) & 0xFF) << 8);

        // Chroma key test: only draw if texel is not 0 (transparent)
        if (texel != 0)
        {
          // Apply CLUT for final color
          colorIndex = (colorIndex & 0xFFFFFF00) | texel;
          U8 finalColor = GTEX_PtrCLUT[colorIndex];
          *screen = finalColor;
        }

        screen++;
        mask &= RepMask;
        colorIndex = (colorIndex & 0xFFFF00FF) | (decV_gour & 0xFF00);
        diffX--;
      }
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

// Dithered texture + chroma key — same as Filler_TextureDither but skips
// transparent texels (texel == 0).  Dither update happens unconditionally.
S32 Filler_TextureDitherChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_TextureDitherChromaKey", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TextureDitherChromaKey", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureDitherChromaKey", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureDitherChromaKey", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureDitherChromaKey", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureDitherChromaKey", "1", "entry");
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
    GTEX_PtrMap2 = PtrMap;
    GTEX_PtrCLUT = PtrCLUTGouraud;
    GTEX_DecMapU = Fill_MapU_XSlope << 16;
    GTEX_DecMapU2 = GTEX_DecMapU;
    GTEX_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
    GTEX_IntMapU2 = GTEX_IntMapU;
    GTEX_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
    GTEX_IntMapV2 = GTEX_IntMapV;
    GTEX_DecMapV_Gour = (Fill_MapV_XSlope << 16) | ((Fill_Gouraud_XSlope >> 8) & 0xFFFF);
    GTEX_DecMapV_Gour2 = GTEX_DecMapV_Gour;
    GTEX_RepMaskPatch = RepMask;
    GTEX_RepMask2 = RepMask;
  }
  else
  {
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
        U64 addU = ((S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        lineU += addUDec;

        U64 addV = ((S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = addVDec << 16;
        addVInt = addVInt >> 16;
        addVInt |= addVDec;
        lineV += addVInt;
      }

      U32 gouraudXSlope = Fill_Gouraud_XSlope;
      gouraudXSlope = (gouraudXSlope & 0xFFFF00FF) | (((lineV >> 16) & 0xFF) << 8);
      U32 decU = lineU << 16;
      gouraudXSlope = (gouraudXSlope & 0xFFFFFF00) | ((lineU >> 16) & 0xFF);
      U32 curGouraudMin = (Fill_CurGouraudMin >> 8) & 0xFFFF;
      U32 decV_gour = (lineV << 16) | curGouraudMin;
      U32 colorIndex = curGouraudMin;
      U32 mask = RepMask & gouraudXSlope;

      do
      {
        colorIndex = (colorIndex & 0xFFFFFF00) | GTEX_PtrMapPatch[mask];

        U8 carry = (decU > (0xFFFFFFFF - GTEX_DecMapU));
        decU += GTEX_DecMapU;
        mask = (mask & 0xFFFFFF00) | (((GTEX_IntMapU & 0xFF) + carry + (mask & 0xFF)) & 0xFF);

        carry = (decV_gour > (0xFFFFFFFF - GTEX_DecMapV_Gour));
        decV_gour += GTEX_DecMapV_Gour;
        mask = (mask & 0xFFFF00FF) | (((((mask >> 8) & 0xFF) + (GTEX_IntMapV & 0xFF) + carry) & 0xFF) << 8);

        if ((colorIndex & 0xFF) != 0)
        {
          colorIndex = (colorIndex & 0xFFFFFF00) | GTEX_PtrCLUT[colorIndex];
          *screen = colorIndex & 0xFF;
        }

        curGouraudMin &= 0xFFFF00FF;
        curGouraudMin += decV_gour;

        screen++;

        colorIndex = (colorIndex & 0xFFFF00FF) | (curGouraudMin & 0xFF00);
        mask &= GTEX_RepMaskPatch;
        --diffX;
      }
      while (diffX);
    }

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
S32 Filler_TextureGouraudZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  Fill_CurY += count;
  U32 patch = Fill_Patch - 1;

  LIB386_TRACE_CPP("FillTGZB", "entry", "nbLines=%u xMin=0x%08X xMax=0x%08X u=0x%08X v=0x%08X gour=0x%08X zbuf=0x%08X patch=%u",
    nbLines, fillCurXMin, fillCurXMax, u, v, Fill_CurGouraudMin, Fill_CurZBufMin, patch + 1);

  // Set up GTEX globals on first call (match ASM @@PtrPatch / @@SetCachedValues):
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
    GTEX_DecMapV_IntZBuf = (Fill_MapV_XSlope << 16) | ((U32)Fill_ZBuf_XSlope >> 8);
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

  U32 scanline_idx_TGZB = 0;
  while (1)
  {
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    LIB386_TRACE_CPP("FillTGZB", "SL", "scan=%u diffX=%u xMin=0x%08X xMax=0x%08X u=0x%08X v=0x%08X gour=0x%08X zbuf=0x%08X",
      scanline_idx_TGZB, diffX, fillCurXMin, fillCurXMax, u, v, Fill_CurGouraudMin, Fill_CurZBufMin);
    scanline_idx_TGZB++;
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *screen = (fillCurXMin >> 16) + curOffLine;
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      U32 decZ_gour = Fill_CurGouraudMin;
      U32 decV_intZ = v;
      U32 decU = u;

      if (diffX > 16)
      {
        S64 addGouraud = (S64)(S32)factor * (S64)(S32)Fill_Gouraud_XSlope;
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        decZ_gour += addGouraudDec;

        S64 addU = (S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope;
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        decU += addUDec;

        S64 addV = (S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope;
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
    GTEX_DecMapV_IntZBuf = (Fill_MapV_XSlope << 16) | ((U32)Fill_ZBuf_XSlope >> 8);
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
        U64 addGouraud = ((S64)(S32)factor * (S64)(S32)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        decZ_gour += addGouraudDec;

        U64 addU = ((S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        decU += addUDec;

        U64 addV = ((S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope);
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
    GTEX_DecMapV_IntZBuf = (Fill_MapV_XSlope << 16) | ((U32)Fill_ZBuf_XSlope >> 8);
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
        U64 addGouraud = ((S64)(S32)factor * (S64)(S32)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        decZ_gour += addGouraudDec;

        U64 addU = ((S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        decU += addUDec;

        U64 addV = ((S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope);
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

// Gouraud texture + chroma key + Z-buffer read (no Z-write).
// Same as Filler_TextureGouraudNZW but skips transparent texels.
S32 Filler_TextureGouraudChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
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
    GTEX_DecMapV_IntZBuf = (Fill_MapV_XSlope << 16) | ((U32)Fill_ZBuf_XSlope >> 8);
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
        U64 addGouraud = ((S64)(S32)factor * (S64)(S32)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        decZ_gour += addGouraudDec;

        U64 addU = ((S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = addUDec << 16;
        addUInt = addUInt >> 16;
        addUDec |= addUInt;
        decU += addUDec;

        U64 addV = ((S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = addVDec << 16;
        addVInt = addVInt >> 16;
        addVInt |= addVDec;
        decV_intZ += addVInt;
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
        U8 texel = GTEX_PtrMapPatch[mask];
        colorIndex = (colorIndex & 0xFFFFFF00) | texel;

        if (texel != 0 && GTEX_PtrZBufferPatch[offset] >= (decV_intZ & 0xFFFF))
        {
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

