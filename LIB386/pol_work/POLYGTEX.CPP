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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (diffX > 16)
      {
        // Simple multiply for sub-pixel adjustment, matching assembly imul instruction
        S32 addGouraud = ((S32)factor * (S32)Fill_Gouraud_XSlope) >> 16;
        curGouraudMin += addGouraud;

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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);

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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
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

// Converted from ASM - Dithered texture rendering with chroma key transparency testing
S32 Filler_TextureDitherChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    
    // Initialize global patch variables for texture mapping
    GTEX_PtrMapPatch = PtrMap;
    GTEX_PtrMap2 = PtrMap;
    GTEX_PtrCLUT = PtrCLUTGouraud;
    
    // Precalculate U slope components
    U32 mapUSlope = Fill_MapU_XSlope;
    GTEX_DecMapU = mapUSlope << 16;
    GTEX_DecMapU2 = GTEX_DecMapU;
    GTEX_IntMapU = (mapUSlope >> 16) & 0xFF;
    GTEX_IntMapU2 = GTEX_IntMapU;
    
    // Precalculate V slope combined with Gouraud slope
    U32 mapVSlope = Fill_MapV_XSlope;
    U32 gouraudSlope = (Fill_Gouraud_XSlope >> 8) & 0xFFFF;
    GTEX_DecMapV_Gour = (mapVSlope << 16) | gouraudSlope;
    GTEX_DecMapV_Gour2 = GTEX_DecMapV_Gour;
    GTEX_IntMapV = (mapVSlope >> 16) & 0xFF;
    GTEX_IntMapV2 = GTEX_IntMapV;
    
    // Initialize mask values
    GTEX_RepMaskPatch = RepMask;
    GTEX_RepMask2 = RepMask;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    
    // Update Gouraud shading for scanline
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
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
      subPixelOffset = 0x10000 - subPixelOffset; // Invert for proper calculation
      
      // Get current Gouraud intensity
      U32 currentGouraud = Fill_CurGouraudMin;
      
      // Calculate texture coordinate adjustments for sub-pixel precision
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
      U32 texV = (v >> 16) & 0xFF;
      U32 texCoord = (texV << 8) | texU;
      
      // Prepare sub-pixel components for interpolation
      U32 subU = u << 16;
      U32 subV = v << 16;
      
      // Extract Gouraud intensity (shift by 8 for proper range)
      U32 gouraudIntensity = (currentGouraud >> 8) & 0xFFFF;
      U32 combinedTexV_Gouraud = subV | gouraudIntensity;
      
      // Apply texture coordinate mask
      texCoord &= RepMask;
      
      // Render scanline pixels
      for (U32 x = 0; x < fillWidth; x++)
      {
        // Sample texture
        U8 texel = GTEX_PtrMapPatch[texCoord & GTEX_RepMaskPatch];
        
        // Update texture coordinates with sub-pixel precision
        subU += GTEX_DecMapU;
        texU += (subU < GTEX_DecMapU) ? GTEX_IntMapU + 1 : GTEX_IntMapU; // Handle carry
        
        combinedTexV_Gouraud += GTEX_DecMapV_Gour;
        texV += (combinedTexV_Gouraud < GTEX_DecMapV_Gour) ? GTEX_IntMapV + 1 : GTEX_IntMapV; // Handle carry
        
        // Test for chroma key (transparent pixel)
        if (texel != 0)
        {
          // Apply color lookup table (CLUT) transformation
          U8 finalColor = GTEX_PtrCLUT[texel];
          dstPtr[x] = finalColor;
        }
        
        // Update texture coordinate for next pixel
        texCoord = ((texV & 0xFF) << 8) | (texU & 0xFF);
        texCoord &= GTEX_RepMaskPatch;
        
        // Extract updated Gouraud component
        gouraudIntensity = (combinedTexV_Gouraud >> 16) & 0xFFFF;
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
    
    // Update Gouraud shading for next scanline
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
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

// Converted from ASM - Gouraud-shaded texture rendering with chroma key and Z-buffer testing (no Z-write)
S32 Filler_TextureGouraudChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    
    // Initialize Z-buffer and texture mapping components
    GTEX_PtrZBufferPatch = PtrZBuffer;
    GTEX_PtrLog = (PTR_U8)Log;
    GTEX_PtrMapPatch = PtrMap;
    GTEX_PtrCLUT = PtrCLUTGouraud;
    GTEX_RepMaskPatch = RepMask;
    
    // Precalculate U slope components
    U32 mapUSlope = Fill_MapU_XSlope;
    GTEX_IntMapU = (mapUSlope >> 16) & 0xFF;
    GTEX_DecMapU = mapUSlope << 16;
    
    // Combine Gouraud and Z-buffer slopes
    U32 gouraudSlope = (Fill_Gouraud_XSlope >> 8) & 0xFFFF;
    U32 zBufSlope = Fill_ZBuf_XSlope;
    GTEX_DecZBuf_Gour = (zBufSlope << (32-8)) | gouraudSlope;
    
    // Combine V slope with Z-buffer integer component
    U32 mapVSlope = Fill_MapV_XSlope;
    U32 zBufInt = zBufSlope >> 8;
    GTEX_DecMapV_IntZBuf = (mapVSlope << 16) | zBufInt;
    GTEX_IntMapV = (mapVSlope >> 16) & 0xFF;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    
    // Update Gouraud and Z-buffer for scanline
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
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
      subPixelOffset = 0x10000 - subPixelOffset; // Invert for proper calculation
      
      // Get current Gouraud and Z-buffer values
      U32 currentGouraud = Fill_CurGouraudMin;
      U32 currentZBuf = Fill_CurZBufMin;
      
      // Calculate coordinate adjustments for sub-pixel precision
      if (fillWidth > 16)
      {
        // Adjust Gouraud intensity
        S32 gouraudAdjust = (S32)(Fill_Gouraud_XSlope * subPixelOffset) >> 16;
        currentGouraud += gouraudAdjust;
        
        // Adjust U coordinate
        S32 uAdjust = (S32)(Fill_MapU_XSlope * subPixelOffset) >> 16;
        u += uAdjust;
        
        // Adjust V coordinate  
        S32 vAdjust = (S32)(Fill_MapV_XSlope * subPixelOffset) >> 16;
        v += vAdjust;
      }
      
      // Prepare texture coordinate extraction
      U32 texU = (u >> 16) & 0xFF;
      U32 texV = (v >> 16) & 0xFF;
      U32 texCoord = (texV << 8) | texU;
      texCoord &= RepMask;
      
      // Prepare sub-pixel components for interpolation
      U32 subU = u << 16;
      U32 subV = v << 16;
      
      // Adjust destination pointer for Log offset
      PTR_U8 logPtr = (PTR_U8)Log;
      S32 dstOffset = dstPtr - logPtr;
      
      // Prepare Z-buffer values
      U32 zBufLow = (currentZBuf >> 8) & 0xFFFF;
      U32 zBufHigh = currentZBuf << (32-8);
      U32 combinedMapV_ZBuf = subV | zBufLow;
      U32 combinedZBuf_Gouraud = (currentGouraud >> 8) | zBufHigh;
      
      // Render scanline pixels
      for (U32 x = 0; x < fillWidth; x++)
      {
        // Sample texture
        U8 texel = GTEX_PtrMapPatch[texCoord & GTEX_RepMaskPatch];
        
        // Test for chroma key (transparent pixel)
        if (texel != 0)
        {
          // Perform Z-buffer test (compare depth)
          U16 currentDepth = combinedMapV_ZBuf & 0xFFFF;
          U16 bufferDepth = GTEX_PtrZBufferPatch[dstOffset];
          
          if (currentDepth >= bufferDepth)
          {
            // Depth test passed - apply color lookup table (CLUT) transformation
            U8 finalColor = GTEX_PtrCLUT[texel];
            logPtr[dstOffset] = finalColor;
          }
        }
        
        // Update texture coordinates with sub-pixel precision
        subU += GTEX_DecMapU;
        texU += (subU < GTEX_DecMapU) ? GTEX_IntMapU + 1 : GTEX_IntMapU; // Handle carry
        
        combinedZBuf_Gouraud += GTEX_DecZBuf_Gour;
        combinedMapV_ZBuf += GTEX_DecMapV_IntZBuf;
        texV += (combinedMapV_ZBuf < GTEX_DecMapV_IntZBuf) ? GTEX_IntMapV + 1 : GTEX_IntMapV; // Handle carry
        
        // Update texture coordinate for next pixel
        texCoord = ((texV & 0xFF) << 8) | (texU & 0xFF);
        texCoord &= GTEX_RepMaskPatch;
        
        // Extract updated Gouraud component for next Z-buffer calculation
        U32 gouraudComponent = (combinedZBuf_Gouraud >> 16) & 0xFFFF;
        
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
    
    // Update Gouraud and Z-buffer for next scanline
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

