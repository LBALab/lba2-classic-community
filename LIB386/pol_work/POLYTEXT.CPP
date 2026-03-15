#include <POLYGON/POLYTEXT.H>

#include <stdio.h>
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/DEBUG_TRACES.H>

U32 Precalc_Inc = 0;

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

S32 Filler_Texture(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_Texture", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, xmin, xmax);
    LIB386_TRACE_CPP("Filler_Texture", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_Texture", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_Texture", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_Texture", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_Texture", "1", "entry");
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
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
       
        S32 addU = (S32)(((S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope) >> 16);
        lineU += addU;

        S32 addV = (S32)(((S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope) >> 16);
        lineV += addV;
      }
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00));
      U32 lineUDec = lineU << 16;
      do
      {
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
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_TextureChromaKey", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, xmin, xmax);
    LIB386_TRACE_CPP("Filler_TextureChromaKey", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureChromaKey", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureChromaKey", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureChromaKey", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureChromaKey", "1", "entry");
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

      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
       
        S32 addU = (S32)(((S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope) >> 16);
        lineU += addU;

        S32 addV = (S32)(((S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope) >> 16);
        lineV += addV;
      }
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00));
      U32 lineUDec = lineU << 16;
      do
      {
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
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureFlat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_TextureFlat", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, xmin, xmax);
    LIB386_TRACE_CPP("Filler_TextureFlat", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureFlat", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureFlat", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureFlat", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureFlat", "1", "entry");
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

      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        U32 addU = (U32)(((U64)factor * (U64)Fill_MapU_XSlope) >> 16);
        lineU += addU;

        U32 addV = (U32)(((U64)factor * (U64)Fill_MapV_XSlope) >> 16);
        lineV += addV;

      }

      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00));
      U32 lineUDec = lineU << 16;
      U8 colorBase = Fill_Color.Num & 0xFF;

      do
      {
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
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureFlatChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_TextureFlatChromaKey", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, xmin, xmax);
    LIB386_TRACE_CPP("Filler_TextureFlatChromaKey", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureFlatChromaKey", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureFlatChromaKey", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureFlatChromaKey", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureFlatChromaKey", "1", "entry");
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

      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00));
      U32 lineUDec = lineU << 16;
      U8 colorBase = Fill_Color.Num & 0xFF;

      do
      {
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00));
      U32 lineUDec = lineU << 16;
      do
      {
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU & 0xFF0000) >> 16) | ((lineV >> 8) & 0xFF00));
      U32 lineUDec = lineU << 16;
      do
      {
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
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 zInt = Fill_CurZBufMin >> 8;
      U32 zDec = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 incDecMapU = Precalc_Inc | (lineU << 16);

      do
      {
        if ((zInt & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
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

S32 Filler_TextureChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 zInt = Fill_CurZBufMin >> 8;
      U32 zDec = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 incDecMapU = Precalc_Inc | (lineU << 16);

      do
      {
        if ((zInt & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
        {
          POLYTEXT_PtrZBuffer1[offset] = zInt & 0xFFFF;
          U8 texel = POLYTEXT_PtrMapPatch[mapIndex];
          if (texel != 0)
          {
            POLYTEXT_PtrLog[offset] = texel;
          }
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 zDec = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16);

      do
      {
        if ((decU_intZ & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 zDec = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16);

      do
      {
        if ((decU_intZ & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 decZ = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16);

      do
      {
        if ((decU_intZ & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 decZ = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16);

      do
      {
        if ((decU_intZ & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
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

S32 Filler_TextureNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 zInt = Fill_CurZBufMin >> 8;
      U32 zDec = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 incDecMapU = Precalc_Inc | (lineU << 16);

      do
      {
        if ((zInt & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
        {
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

S32 Filler_TextureChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 zInt = Fill_CurZBufMin >> 8;
      U32 zDec = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 incDecMapU = Precalc_Inc | (lineU << 16);

      do
      {
        U8 texel = POLYTEXT_PtrMapPatch[mapIndex];
        if (texel != 0)
        {
          if ((zInt & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
          {
            POLYTEXT_PtrLog[offset] = texel;
          }
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 zDec = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16);

      do
      {
        if ((decU_intZ & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
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

S32 Filler_TextureFlatChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 zDec = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16);

      do
      {
        if ((decU_intZ & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
        {
          U8 colorIndex = POLYTEXT_PtrMapPatch[mapIndex];
          if (colorIndex != 0)
          {
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

S32 Filler_TextureFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 decZ = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16);

      do
      {
        if ((decU_intZ & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
        {
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

S32 Filler_TextureChromaKeyFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
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
      U32 lineVDec = lineV << 16;
      U32 mapIndex = RepMask & (((lineU >> 16) & 0xFF) | ((lineV & 0xFF0000) >> 8));
      U32 decZ = Fill_CurZBufMin << 24;
      U32 offset = line - (U8 *)Log;
      U32 decU_intZ = (Fill_CurZBufMin >> 8) | (lineU << 16);

      do
      {
        if ((decU_intZ & 0xFFFF) < POLYTEXT_PtrZBufferPatch[offset])
        {
          U8 textureColor = POLYTEXT_PtrMapPatch[mapIndex];
          if (textureColor != 0)
          {
            U8 foggedColor = Fill_Logical_Palette[textureColor];
            POLYTEXT_PtrLog[offset] = foggedColor;
          }
        }

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
