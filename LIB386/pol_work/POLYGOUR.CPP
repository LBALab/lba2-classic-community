#include <POLYGON/POLYGOUR.H>

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/UTILS.H>

#include <stdio.h>

/*
; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                              (Adeline Software Intl.)              ***
; ***                           1995-96-97                               ***
; ***                                                                    ***
; **************************************************************************
; **************************************************************************
; **************************************************************************
*/
U32 POLYGOUR_AddGour = 0x12345678;
U32 POLYGOUR_AddGour1 = 0x12345678;
U32 POLYGOUR_AddGour2 = 0x12345678;
U32 POLYGOUR_AddGour3 = 0x12345678;
U32 POLYGOUR_IntZBuf = 0x12345678;
U32 POLYGOUR_IntZBuf1 = 0x12345678;
U32 POLYGOUR_IntZBuf2 = 0x12345678;
U32 POLYGOUR_IntZBuf3 = 0x12345678;
U32 POLYGOUR_DecGour = 0x12345678;
U32 POLYGOUR_DecZBuf = 0x12345678;
U32 POLYGOUR_DecZBuf1 = 0x12345678;
U32 POLYGOUR_DecZBuf2 = 0x12345678;
U32 POLYGOUR_DecZBuf3 = 0x12345678;
U32 POLYGOUR_DecZBuf_AddGour = 0x12345678;
U32 POLYGOUR_DecZBuf_AddGour1 = 0x12345678;
U32 POLYGOUR_DecZBuf_AddGour2 = 0x12345678;
U32 POLYGOUR_DecZBuf_AddGour3 = 0x12345678;
U8 *POLYGOUR_PtrLog = NULL;
U8 *POLYGOUR_PtrLog_1 = NULL;
U8 *POLYGOUR_PtrLog2 = NULL;
U8 *POLYGOUR_PtrLog2_1 = NULL;
U8 *POLYGOUR_PtrLog4 = NULL;
U16 *POLYGOUR_PtrZBuffer_1 = NULL;
U16 *POLYGOUR_PtrZBuffer1 = NULL;
U16 *POLYGOUR_PtrZBuffer1_1 = NULL;
U16 *POLYGOUR_PtrZBuffer2 = NULL;
U16 *POLYGOUR_PtrZBuffer2_1 = NULL;
U16 *POLYGOUR_PtrZBuffer3 = NULL;
U16 *POLYGOUR_PtrZBuffer3_1 = NULL;
U16 *POLYGOUR_PtrZBuffer4 = NULL;
U16 *POLYGOUR_PtrZBuffer5 = NULL;
U16 *POLYGOUR_PtrZBufferPatch = NULL;
U8 *POLYGOUR_PtrColor = NULL;
U8 *POLYGOUR_PtrColor1 = NULL;
U8 *POLYGOUR_PtrColor2 = NULL;
U8 *POLYGOUR_PtrColor3 = NULL;
U8 *POLYGOUR_PtrColor4 = NULL;
U8 POLYGOUR_IntGour = 0x12;

// Verified
S32 Filler_Gouraud(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 curGouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
  }

  while ( 1 )
  {
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      U32 lineGouraud = curGouraud;

      if (diffX > 16)
      {
        U64 addGouraud = ((U64)factor * (U64)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        lineGouraud += addGouraudDec;

        // There is a bunch of cache-related stuff here in the original ASM, but
        // it's not needed in the C version AFAICT.
      }

      // @@NoPreload:
      U32 gouraudSlopeDec = Fill_Gouraud_XSlope << 16; // ebp
      U32 gouraudSlopeInt = Fill_Gouraud_XSlope >> 16; // ebx
      U32 color = lineGouraud >> 16; // eax
      U32 gouraud = lineGouraud << 16; // esi
      do
      {
        *screen = color & 0xFF;
        U8 carry = ((U64)gouraud + (U64)gouraudSlopeDec) > 0xFFFFFFFF;
        gouraud += gouraudSlopeDec;
        color = (color & 0xFFFFFF00) | (((gouraudSlopeInt & 0xFF) + carry + (color & 0xFF)) & 0xFF);
        ++screen;
        --diffX;
      }
      while (diffX);
    }

    if ( !--count )
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_Dither(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  Fill_CurY += count;
  U8 *curOffLine = Fill_CurOffLine;

  U32 patch = Fill_Patch - 1;
  U32 curGouraud = Fill_CurGouraudMin;

  if (patch == 0)
  {
    Fill_Patch = patch;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
  }

  while ( 1 )
  {
    // @@EnterLoop:
    U8 *screen = curOffLine + (fillCurXMin >> 16);
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      // *** Gouraud dither loop ***

      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 gouraud = curGouraud >> 8; // edx
      U32 gouraudSlope = Fill_Gouraud_XSlope >> 8; // ebp
      U32 color = gouraud; // eax
      U8 isOdd = diffX & 1;
      U32 pixelCount = diffX >> 1;
      if (isOdd)
      {
        if (pixelCount != 0)
        {
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
          do
          {
            // @@Each_X:
            color = (color & 0xFF) + gouraud;
            gouraud += gouraudSlope;

            *screen = (color >> 8) & 0xFF;

            color = (color & 0xFF) + gouraud;
            gouraud += gouraudSlope;

            *(screen + 1) = (color >> 8) & 0xFF;
            screen += 2;

            color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);

            pixelCount--;
          }
          while ( pixelCount );
        }
        // @@Add_One:
        color = (color & 0xFF) + gouraud;
        *screen = (color >> 8) & 0xFF;
      }
      else
      {
        // @@Pas_Add1:
        for ( ; pixelCount != 0; --pixelCount )
        {
          // @@Each_X2:
          color = (color & 0xFF) + gouraud;
          gouraud += gouraudSlope;

          *screen = (color >> 8) & 0xFF;

          color = (color & 0xFF) + gouraud;
          gouraud += gouraudSlope;

          *(screen + 1) = (color >> 8) & 0xFF;
          screen += 2;
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
        }
      }
      // @@End_X:
    }
    // @@EndScanLine:
    if ( !--count )
      break;

    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_GouraudTable(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 gouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    gouraud += Fill_Gouraud_LeftSlope;
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
        S32 gouraudAdjust = Fill_Gouraud_XSlope * factor;
        gouraud += (gouraudAdjust >> 16);

        // Cache line preloading for performance
        U8 *cacheStart = (U8 *)((uintptr_t)line & ~31);
        U8 *cacheEnd = (U8 *)(((uintptr_t)line + fillWidth + 31) & ~31);
        for (U8 *cache = cacheStart; cache <= cacheEnd; cache += 32)
        {
          volatile U8 dummy = *cache;
          (void)dummy;
        }
      }

      // Fill_Color contains the base address of the Gouraud color lookup table
      PTR_U8 gouraudTable = Fill_Color.Ptr;
      U32 gouraudValue = gouraud;
      U32 gouraudSlope = Fill_Gouraud_XSlope;

      // Process each pixel with Gouraud shading using color lookup table
      for (U32 i = 0; i < fillWidth; i++)
      {
        // Extract the lighting value (upper 8 bits) and use as index into color table
        U32 lightIndex = (gouraudValue >> 8) & 0xFF00;
        U8 shadedColor = gouraudTable[lightIndex];
        
        line[i] = shadedColor;
        gouraudValue += gouraudSlope;
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
    gouraud += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurGouraudMin = gouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_DitherTable(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 gouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    gouraud += Fill_Gouraud_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;

      // Cache line preloading for performance (if scanline is large enough)
      if (fillWidth >= 32)
      {
        U8 *cacheStart = (U8 *)((uintptr_t)line & ~31);
        U8 *cacheEnd = (U8 *)(((uintptr_t)line + fillWidth + 31) & ~31);
        for (U8 *cache = cacheStart; cache <= cacheEnd; cache += 32)
        {
          volatile U8 dummy = *cache;
          (void)dummy;
        }
      }

      // Dithered Gouraud shading with color lookup table
      PTR_U8 colorTable = Fill_Color.Ptr;
      S32 gouraudValue = gouraud >> 8;  // Extract lighting value
      S32 gouraudSlope = Fill_Gouraud_XSlope >> 8;  // Extract slope
      U32 ditherValue = gouraudValue & 0xFF;
      U32 ditherCount = fillWidth;

      // Process pixels in pairs for dithering effect
      U32 i = 0;
      if (ditherCount > 1)
      {
        // Rotate dither pattern based on pixel position
        ditherValue = ((ditherValue << (ditherCount & 31)) | (ditherValue >> (32 - (ditherCount & 31)))) & 0xFF;
        
        // Process pairs of pixels
        U32 pairCount = ditherCount >> 1;
        for (U32 pair = 0; pair < pairCount; pair++)
        {
          // First pixel of pair
          U32 index1 = (ditherValue & 0xFF) + gouraudValue;
          U32 tableIndex1 = index1 & 0xFF00;
          U8 color1 = colorTable[tableIndex1];
          line[i++] = color1;
          
          ditherValue &= 0xFF;
          ditherValue += gouraudValue;
          gouraudValue += gouraudSlope;
          
          // Second pixel of pair
          U32 index2 = (ditherValue & 0xFF) + gouraudValue;
          U32 tableIndex2 = index2 & 0xFF00;
          U8 color2 = colorTable[tableIndex2];
          line[i++] = color2;
          
          ditherValue &= 0xFF;
          ditherValue = ((ditherValue << (ditherCount & 31)) | (ditherValue >> (32 - (ditherCount & 31)))) & 0xFF;
          ditherValue += gouraudValue;
          gouraudValue += gouraudSlope;
        }
        
        // Handle odd pixel if fillWidth is odd
        if (ditherCount & 1)
        {
          U32 tableIndex = ditherValue & 0xFF00;
          U8 color = colorTable[tableIndex];
          line[i] = color;
        }
      }
      else if (ditherCount == 1)
      {
        // Single pixel
        U32 tableIndex = ditherValue & 0xFF00;
        U8 color = colorTable[tableIndex];
        line[0] = color;
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
    gouraud += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurGouraudMin = gouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_GouraudFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 curGouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
  }

  while ( 1 )
  {
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      U32 lineGouraud = curGouraud;

      if (diffX > 16)
      {
        U64 addGouraud = ((U64)factor * (U64)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        lineGouraud += addGouraudDec;

        // There is a bunch of cache-related stuff here in the original ASM, but
        // it's not needed in the C version AFAICT.
      }

      // @@NoPreload:
      U32 gouraudSlopeDec = Fill_Gouraud_XSlope << 16; // ebp
      U32 gouraudSlopeInt = Fill_Gouraud_XSlope >> 16; // ebx
      U32 color = (lineGouraud >> 16) & 0xFF; // eax
      U32 gouraud = lineGouraud << 16; // esi
      do
      {
        U8 carry = ((U64)gouraud + (U64)gouraudSlopeDec) > 0xFFFFFFFF;
        gouraud += gouraudSlopeDec;

        *screen = Fill_Logical_Palette[color];

        color = (color & 0xFFFFFF00) | (((gouraudSlopeInt & 0xFF) + carry + (color & 0xFF)) & 0xFF);
        ++screen;
        --diffX;
      }
      while (diffX);
    }

    if ( !--count )
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_DitherFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  Fill_CurY += count;
  U8 *curOffLine = Fill_CurOffLine;

  U32 patch = Fill_Patch - 1;
  U32 curGouraud = Fill_CurGouraudMin;
  U32 gouraudSlope = Fill_Gouraud_XSlope >> 8;

  if (patch == 0)
  {
    Fill_Patch = patch;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
  }

  while ( 1 )
  {
    // @@EnterLoop:
    U8 *screen = curOffLine + (fillCurXMin >> 16);
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      // *** Gouraud dither loop ***

      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 gouraud = curGouraud >> 8; // edx
      U32 color = gouraud; // eax
      U8 isOdd = diffX & 1;
      U32 pixelCount = diffX >> 1;
      if (isOdd)
      {
        if (pixelCount != 0)
        {
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
          do
          {
            // @@Each_X:
            color = (color & 0xFF) + gouraud;
            gouraud += gouraudSlope;

            *screen = Fill_Logical_Palette[(color >> 8) & 0xFF];

            color = (color & 0xFF) + gouraud;
            gouraud += gouraudSlope;

            *(screen + 1) = Fill_Logical_Palette[(color >> 8) & 0xFF];
            screen += 2;

            color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
            pixelCount--;
          }
          while ( pixelCount );
        }
        // @@Add_One:
        color = (color & 0xFF) + gouraud;
        *screen = Fill_Logical_Palette[(color >> 8) & 0xFF];
      }
      else
      {
        // @@Pas_Add1:
        for ( ; pixelCount != 0; --pixelCount )
        {
          // @@Each_X2:
          color = (color & 0xFF) + gouraud;
          gouraud += gouraudSlope;

          *screen = Fill_Logical_Palette[(color >> 8) & 0xFF];

          color = (color & 0xFF) + gouraud;
          gouraud += gouraudSlope;

          *(screen + 1) = Fill_Logical_Palette[(color >> 8) & 0xFF];
          screen += 2;

          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
        }
      }
      // @@End_X:
    }
    // @@EndScanLine:
    if ( !--count )
      break;

    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_GouraudZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 curGouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYGOUR_DecGour = Fill_Gouraud_XSlope << 16;
    POLYGOUR_DecZBuf = Fill_ZBuf_XSlope << 24;
    POLYGOUR_IntZBuf = Fill_ZBuf_XSlope >> 8;
    POLYGOUR_PtrZBufferPatch = PtrZBuffer;
    POLYGOUR_PtrZBuffer1 = PtrZBuffer;
    POLYGOUR_PtrLog = (U8 *)Log;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while ( 1 )
  {
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      U32 lineGouraud = curGouraud;

      if (diffX > 16)
      {
        U64 addGouraud = ((U64)factor * (U64)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        lineGouraud += addGouraudDec;
      }

      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 gouraudSlopeInt = Fill_Gouraud_XSlope >> 16; // ebx
      U32 curZBufMinDec = Fill_CurZBufMin << 24; // ebp
      U32 curZBufMinInt = Fill_CurZBufMin >> 8; // edx

      U32 offset = screen - (U8 *)Log; // edi
      U32 color = lineGouraud >> 16; // eax
      U32 gouraud = lineGouraud << 16;

      do
      {
        if (POLYGOUR_PtrZBufferPatch[offset] >= (curZBufMinInt & 0xFFFF))
        {
          POLYGOUR_PtrZBuffer1[offset] = curZBufMinInt & 0xFFFF;
          POLYGOUR_PtrLog[offset] = color & 0xFF;
        }

        U8 carry = ((U64)gouraud + (U64)POLYGOUR_DecGour) > 0xFFFFFFFF;
        gouraud += POLYGOUR_DecGour;
        color = (color & 0xFFFFFF00) | (((gouraudSlopeInt & 0xFF) + carry + (color & 0xFF)) & 0xFF);

        carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf) > 0xFFFFFFFF;
        curZBufMinDec += POLYGOUR_DecZBuf;
        curZBufMinInt += POLYGOUR_IntZBuf + carry;

        ++offset;
        --diffX;
      }
      while (diffX);
    }

    if (!--count)
    {
      break;
    }

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_DitherZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  Fill_CurY += count;
  U8 *curOffLine = Fill_CurOffLine;

  U32 patch = Fill_Patch - 1;
  U32 curGouraud = Fill_CurGouraudMin;
  U32 gouraudSlope = Fill_Gouraud_XSlope >> 8;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYGOUR_AddGour = gouraudSlope;
    POLYGOUR_AddGour1 = gouraudSlope;
    POLYGOUR_AddGour2 = gouraudSlope;
    POLYGOUR_AddGour3 = gouraudSlope;
    POLYGOUR_PtrZBufferPatch = PtrZBuffer;
    POLYGOUR_PtrZBuffer1 = PtrZBuffer;
    POLYGOUR_PtrZBuffer2 = PtrZBuffer;
    POLYGOUR_PtrZBuffer3 = PtrZBuffer;
    POLYGOUR_PtrZBuffer4 = PtrZBuffer;
    POLYGOUR_PtrZBuffer5 = PtrZBuffer;
    POLYGOUR_PtrLog = (U8 *)Log;
    POLYGOUR_PtrLog2 = (U8 *)Log;
    POLYGOUR_PtrLog4 = (U8 *)Log;

    POLYGOUR_PtrZBuffer_1 = PtrZBuffer + 1;
    POLYGOUR_PtrZBuffer1_1 = PtrZBuffer + 1;
    POLYGOUR_PtrZBuffer2_1 = PtrZBuffer + 1;
    POLYGOUR_PtrZBuffer3_1 = PtrZBuffer + 1;

    POLYGOUR_PtrLog_1 = (U8 *)Log + 1;
    POLYGOUR_PtrLog2_1 = (U8 *)Log + 1;

    U32 zBufSlopeInt = Fill_ZBuf_XSlope >> 8;
    POLYGOUR_IntZBuf = zBufSlopeInt;
    POLYGOUR_IntZBuf1 = zBufSlopeInt;
    POLYGOUR_IntZBuf2 = zBufSlopeInt;
    POLYGOUR_IntZBuf3 = zBufSlopeInt;

    U32 zBufSlopeDec = Fill_ZBuf_XSlope << 24;
    POLYGOUR_DecZBuf = zBufSlopeDec;
    POLYGOUR_DecZBuf1 = zBufSlopeDec;
    POLYGOUR_DecZBuf2 = zBufSlopeDec;
    POLYGOUR_DecZBuf3 = zBufSlopeDec;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while ( 1 )
  {
    // @@EnterLoop:
    U8 *screen = curOffLine + (fillCurXMin >> 16);
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      // *** Gouraud dither loop ***

      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 curZBufMinDec = Fill_CurZBufMin << 24; // ebp
      U32 curZBufMinInt = Fill_CurZBufMin >> 8; // ebx

      U32 offset = screen - (U8 *)Log; // edi
      U32 gouraud = curGouraud >> 8; // edx
      U32 color = gouraud; // eax
      U8 isOdd = diffX & 1;
      U32 pixelCount = diffX >> 1;
      if (isOdd)
      {
        if (pixelCount != 0)
        {
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
          do
          {
            // @@Each_X:
            color = (color & 0xFF) + gouraud;
            gouraud += POLYGOUR_AddGour;

            if (POLYGOUR_PtrZBufferPatch[offset] >= (curZBufMinInt & 0xFFFF))
            {
              POLYGOUR_PtrZBuffer1[offset] = curZBufMinInt & 0xFFFF;
              POLYGOUR_PtrLog[offset] = (color >> 8) & 0xFF;
            }

            color = (color & 0xFF) + gouraud;

            U8 carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf) > 0xFFFFFFFF;
            curZBufMinDec += POLYGOUR_DecZBuf;
            curZBufMinInt += POLYGOUR_IntZBuf + carry;

            if (POLYGOUR_PtrZBuffer_1[offset] >= (curZBufMinInt & 0xFFFF))
            {
              POLYGOUR_PtrZBuffer1_1[offset] = curZBufMinInt & 0xFFFF;
              POLYGOUR_PtrLog_1[offset] = (color >> 8) & 0xFF;
            }

            gouraud += POLYGOUR_AddGour1;

            carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf1) > 0xFFFFFFFF;
            curZBufMinDec += POLYGOUR_DecZBuf1;
            curZBufMinInt += POLYGOUR_IntZBuf1 + carry;

            color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
            pixelCount--;
            offset += 2;
          }
          while ( pixelCount );
        }
        // @@Add_One:
        if (POLYGOUR_PtrZBuffer4[offset] >= (curZBufMinInt & 0xFFFF))
        {
          POLYGOUR_PtrZBuffer5[offset] = curZBufMinInt & 0xFFFF;
          color = (color & 0xFF) + gouraud;
          POLYGOUR_PtrLog4[offset] = (color >> 8) & 0xFF;
        }
      }
      else
      {
        // @@Pas_Add1:
        for ( ; pixelCount != 0; --pixelCount )
        {
          // @@Each_X2:
          color = (color & 0xFF) + gouraud;
          gouraud += POLYGOUR_AddGour2;

          if (POLYGOUR_PtrZBuffer2[offset] >= (curZBufMinInt & 0xFFFF))
          {
            POLYGOUR_PtrZBuffer3[offset] = curZBufMinInt & 0xFFFF;
            POLYGOUR_PtrLog2[offset] = (color >> 8) & 0xFF;
          }

          U8 carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf2) > 0xFFFFFFFF;
          curZBufMinDec += POLYGOUR_DecZBuf2;
          curZBufMinInt += POLYGOUR_IntZBuf2 + carry;

          color = (color & 0xFF) + gouraud;

          if (POLYGOUR_PtrZBuffer2_1[offset] >= (curZBufMinInt & 0xFFFF))
          {
            POLYGOUR_PtrZBuffer3_1[offset] = curZBufMinInt & 0xFFFF;
            POLYGOUR_PtrLog2_1[offset] = (color >> 8) & 0xFF;
          }

          gouraud += POLYGOUR_AddGour3;

          carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf3) > 0xFFFFFFFF;
          curZBufMinDec += POLYGOUR_DecZBuf3;
          curZBufMinInt += POLYGOUR_IntZBuf3 + carry;

          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
          offset += 2;
        }
      }
      // @@End_X:
    }
    // @@EndScanLine:
    if ( !--count )
      break;

    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_GouraudTableZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  Fill_CurY += count;
  U8 *curOffLine = Fill_CurOffLine;

  U32 patch = Fill_Patch - 1;
  U32 curGouraud = Fill_CurGouraudMin;

  if (patch == 0) {
    Fill_Patch = 0;
    // Variables are kept for consistency with assembly but not used in this simplified version
  }

  for (U32 lineNum = 0; lineNum < nbLines; lineNum++) {
    if (patch != 0) {
      curOffLine += ScreenPitch;
      fillCurXMin += Fill_LeftSlope;
      fillCurXMax += Fill_RightSlope;
      curGouraud += Fill_Gouraud_LeftSlope;
      Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
    }

    U32 startX = fillCurXMin >> 16;
    U32 endX = fillCurXMax >> 16;
    U8 *scanLine = curOffLine + startX;
    U32 pixelCount = endX - startX;

    if (pixelCount > 0) {
      // Subpixel correction for Gouraud
      U32 subPixel = fillCurXMin & 0xFFFF;
      subPixel ^= 0xFFFF;
      
      // Preload cache for large scanlines
      if (pixelCount > 16) {
        U32 gouraudCorrection = (subPixel * Fill_Gouraud_XSlope) >> 16;
        curGouraud += gouraudCorrection;
        
        // Preload destination cache lines
        U8 *preloadStart = (U8*)((uintptr_t)scanLine & ~31);
        U8 *preloadEnd = (U8*)((uintptr_t)(scanLine + pixelCount) & ~31);
        for (U8 *addr = preloadStart; addr <= preloadEnd; addr += 32) {
          volatile U8 temp = *addr; // Cache preload
        }
      }

      // Setup for pixel loop
      U8 *colorTable = Fill_Color.Ptr; // Color lookup table
      U32 zBuf = Fill_CurZBufMin;
      U32 zBufInt = zBuf >> 8;
      U32 zBufFrac = zBuf << (32 - 8);
      U8 *logPtr = (U8*)Log;
      U16 *zBufferPtr = (U16*)PtrZBuffer;
      
      U32 gouraudValue = curGouraud;
      U32 gouraudIndex = (gouraudValue >> 8) & 0xFF00;

      for (U32 x = 0; x < pixelCount; x++) {
        // Z-buffer test
        U32 pixelOffset = (scanLine - logPtr) + x;
        if (zBufferPtr[pixelOffset] > zBufInt) {
          // Update Z-buffer
          zBufferPtr[pixelOffset] = zBufInt;
          
          // Write color using lookup table
          U8 colorIndex = colorTable[gouraudIndex];
          scanLine[x] = colorIndex;
        }

        // Advance interpolants
        gouraudValue += Fill_Gouraud_XSlope;
        gouraudIndex = (gouraudValue >> 8) & 0xFF00;
        
        zBufFrac += Fill_ZBuf_XSlope << (32 - 8);
        zBufInt += (Fill_ZBuf_XSlope >> 8);
        if (zBufFrac < (Fill_ZBuf_XSlope << (32 - 8))) {
          zBufInt++;
        }
      }
    }

    patch = 1; // Subsequent lines don't need initialization
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_DitherTableZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  Fill_CurY += count;
  U8 *curOffLine = Fill_CurOffLine;

  U32 patch = Fill_Patch - 1;
  U32 curGouraud = Fill_CurGouraudMin;
  U32 gouraudSlope = Fill_Gouraud_XSlope >> 8;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYGOUR_AddGour = gouraudSlope;
    POLYGOUR_AddGour1 = gouraudSlope;
    POLYGOUR_AddGour2 = gouraudSlope;
    POLYGOUR_AddGour3 = gouraudSlope;
    POLYGOUR_PtrZBufferPatch = PtrZBuffer;
    POLYGOUR_PtrZBuffer1 = PtrZBuffer;
    POLYGOUR_PtrZBuffer2 = PtrZBuffer;
    POLYGOUR_PtrZBuffer3 = PtrZBuffer;
    POLYGOUR_PtrZBuffer4 = PtrZBuffer;
    POLYGOUR_PtrZBuffer5 = PtrZBuffer;
    POLYGOUR_PtrLog = (U8 *)Log;
    POLYGOUR_PtrLog2 = (U8 *)Log;
    POLYGOUR_PtrLog4 = (U8 *)Log;

    POLYGOUR_PtrZBuffer_1 = PtrZBuffer + 1;
    POLYGOUR_PtrZBuffer1_1 = PtrZBuffer + 1;
    POLYGOUR_PtrZBuffer2_1 = PtrZBuffer + 1;
    POLYGOUR_PtrZBuffer3_1 = PtrZBuffer + 1;

    POLYGOUR_PtrLog_1 = (U8 *)Log + 1;
    POLYGOUR_PtrLog2_1 = (U8 *)Log + 1;

    U32 zBufSlopeInt = Fill_ZBuf_XSlope >> 8;
    POLYGOUR_IntZBuf = zBufSlopeInt;
    POLYGOUR_IntZBuf1 = zBufSlopeInt;
    POLYGOUR_IntZBuf2 = zBufSlopeInt;
    POLYGOUR_IntZBuf3 = zBufSlopeInt;

    U32 zBufSlopeDec = Fill_ZBuf_XSlope << 24;
    POLYGOUR_DecZBuf = zBufSlopeDec;
    POLYGOUR_DecZBuf1 = zBufSlopeDec;
    POLYGOUR_DecZBuf2 = zBufSlopeDec;
    POLYGOUR_DecZBuf3 = zBufSlopeDec;

    POLYGOUR_PtrColor = Fill_Color.Ptr;
    POLYGOUR_PtrColor1 = Fill_Color.Ptr;
    POLYGOUR_PtrColor2 = Fill_Color.Ptr;
    POLYGOUR_PtrColor3 = Fill_Color.Ptr;
    POLYGOUR_PtrColor4 = Fill_Color.Ptr;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while ( 1 )
  {
    // @@EnterLoop:
    U8 *screen = curOffLine + (fillCurXMin >> 16);
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      // *** Gouraud dither loop ***

      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 curZBufMinDec = Fill_CurZBufMin << 24; // ebp
      U32 curZBufMinInt = Fill_CurZBufMin >> 8; // ebx

      U32 offset = screen - (U8 *)Log; // edi
      U32 gouraud = curGouraud >> 8; // edx
      U32 color = (gouraud & 0xFF) + gouraud; // eax
      color += Fill_CurZBufMin >> 8;
      U32 colorIndex = color & 0xFF00; // esi
      U8 isOdd = diffX & 1;
      U32 pixelCount = diffX >> 1;
      if (isOdd)
      {
        if (pixelCount != 0)
        {
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
          do
          {
            // @@Each_X:
            if (POLYGOUR_PtrZBufferPatch[offset] >= (curZBufMinInt & 0xFFFF))
            {
              POLYGOUR_PtrZBuffer1[offset] = curZBufMinInt & 0xFFFF;
              color = (color & 0xFFFF00FF) | (POLYGOUR_PtrColor[colorIndex] << 8);
              POLYGOUR_PtrLog[offset] = (color >> 8) & 0xFF;
            }

            gouraud += POLYGOUR_AddGour;
            color = (color & 0xFF) + gouraud;

            U8 carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf) > 0xFFFFFFFF;
            curZBufMinDec += POLYGOUR_DecZBuf;
            curZBufMinInt += POLYGOUR_IntZBuf + carry;

            colorIndex = color & 0xFF00;

            if (POLYGOUR_PtrZBuffer_1[offset] >= (curZBufMinInt & 0xFFFF))
            {
              POLYGOUR_PtrZBuffer1_1[offset] = curZBufMinInt & 0xFFFF;
              color = (color & 0xFFFF00FF) | (POLYGOUR_PtrColor1[colorIndex] << 8);
              POLYGOUR_PtrLog_1[offset] = (color >> 8) & 0xFF;
            }

            gouraud += POLYGOUR_AddGour1;
            color = (color & 0xFF) + gouraud;

            carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf1) > 0xFFFFFFFF;
            curZBufMinDec += POLYGOUR_DecZBuf1;
            curZBufMinInt += POLYGOUR_IntZBuf1 + carry;

            colorIndex = color & 0xFF00;

            color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
            pixelCount--;
            offset += 2;
          }
          while ( pixelCount );
        }
        // @@Add_One:
        if (POLYGOUR_PtrZBuffer4[offset] >= (curZBufMinInt & 0xFFFF))
        {
          POLYGOUR_PtrZBuffer5[offset] = curZBufMinInt & 0xFFFF;
          color = (color & 0xFFFF00FF) | (POLYGOUR_PtrColor4[colorIndex] << 8);
          POLYGOUR_PtrLog4[offset] = (color >> 8) & 0xFF;
        }
      }
      else
      {
        // @@Pas_Add1:
        for ( ; pixelCount != 0; --pixelCount )
        {
          // @@Each_X2:
          if (POLYGOUR_PtrZBuffer2[offset] >= (curZBufMinInt & 0xFFFF))
          {
            POLYGOUR_PtrZBuffer3[offset] = curZBufMinInt & 0xFFFF;
            color = (color & 0xFFFF00FF) | (POLYGOUR_PtrColor2[colorIndex] << 8);
            POLYGOUR_PtrLog2[offset] = (color >> 8) & 0xFF;
          }

          gouraud += POLYGOUR_AddGour2;
          color = (color & 0xFF) + gouraud;

          U8 carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf2) > 0xFFFFFFFF;
          curZBufMinDec += POLYGOUR_DecZBuf2;
          curZBufMinInt += POLYGOUR_IntZBuf2 + carry;

          colorIndex = color & 0xFF00;

          if (POLYGOUR_PtrZBuffer2_1[offset] >= (curZBufMinInt & 0xFFFF))
          {
            POLYGOUR_PtrZBuffer3_1[offset] = curZBufMinInt & 0xFFFF;
            color = (color & 0xFFFF00FF) | (POLYGOUR_PtrColor3[colorIndex] << 8);
            POLYGOUR_PtrLog2_1[offset] = (color >> 8) & 0xFF;
          }

          gouraud += POLYGOUR_AddGour3;
          color = (color & 0xFF) + gouraud;

          carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf3) > 0xFFFFFFFF;
          curZBufMinDec += POLYGOUR_DecZBuf3;
          curZBufMinInt += POLYGOUR_IntZBuf3 + carry;

          colorIndex = color & 0xFF00;

          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
          offset += 2;
        }
      }
      // @@End_X:
    }
    // @@EndScanLine:
    if ( !--count )
      break;

    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_GouraudFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 curGouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYGOUR_IntGour = (Fill_Gouraud_XSlope >> 16) & 0xFF;
    POLYGOUR_DecGour = Fill_Gouraud_XSlope << 16;
    POLYGOUR_DecZBuf = Fill_ZBuf_XSlope << 24;
    POLYGOUR_IntZBuf = Fill_ZBuf_XSlope >> 8;
    POLYGOUR_PtrZBufferPatch = PtrZBuffer;
    POLYGOUR_PtrZBuffer1 = PtrZBuffer;
    POLYGOUR_PtrLog = (U8 *)Log;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while ( 1 )
  {
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);

    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      U32 lineGouraud = curGouraud;

      if (diffX > 16)
      {
        U64 addGouraud = ((U64)factor * (U64)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        lineGouraud += addGouraudDec;
      }

      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 curZBufMinDec = Fill_CurZBufMin << 24; // ebp
      U32 curZBufMinInt = Fill_CurZBufMin >> 8; // ebx

      U32 color = (lineGouraud >> 16) & 0xFF; // eax
      lineGouraud <<= 16;
      U32 offset = screen - (U8 *)Log; // edi

      do
      {
        if (POLYGOUR_PtrZBufferPatch[offset] >= (curZBufMinInt & 0xFFFF))
        {
          POLYGOUR_PtrZBuffer1[offset] = curZBufMinInt & 0xFFFF;
          POLYGOUR_PtrLog[offset] = Fill_Logical_Palette[color];
        }

        ++offset;

        U8 carry = ((U64)lineGouraud + (U64)POLYGOUR_DecGour) > 0xFFFFFFFF;
        lineGouraud += POLYGOUR_DecGour;
        color = (color & 0xFFFFFF00) | (((POLYGOUR_IntGour & 0xFF) + carry + (color & 0xFF)) & 0xFF);

        carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf) > 0xFFFFFFFF;
        curZBufMinDec += POLYGOUR_DecZBuf;
        curZBufMinInt += POLYGOUR_IntZBuf + carry;

        --diffX;
      }
      while (diffX);
    }

    if ( !--count )
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_DitherFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  Fill_CurY += count;
  U8 *curOffLine = Fill_CurOffLine;
  U32 curGouraud = Fill_CurGouraudMin;

  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    U32 decZBuf_addGour = Fill_Gouraud_XSlope >> 8;
    decZBuf_addGour &= 0xFFFF;
    decZBuf_addGour |= Fill_ZBuf_XSlope << 24;

    POLYGOUR_DecZBuf_AddGour = decZBuf_addGour;
    POLYGOUR_DecZBuf_AddGour1 = decZBuf_addGour;
    POLYGOUR_DecZBuf_AddGour2 = decZBuf_addGour;
    POLYGOUR_DecZBuf_AddGour3 = decZBuf_addGour;

    POLYGOUR_PtrLog = (U8 *)Log;
    POLYGOUR_PtrLog2 = (U8 *)Log;
    POLYGOUR_PtrLog4 = (U8 *)Log;

    POLYGOUR_PtrLog_1 = (U8 *)Log + 1;
    POLYGOUR_PtrLog2_1 = (U8 *)Log + 1;

    U32 zBufSlopeInt = Fill_ZBuf_XSlope >> 8;
    POLYGOUR_IntZBuf = zBufSlopeInt;
    POLYGOUR_IntZBuf1 = zBufSlopeInt;
    POLYGOUR_IntZBuf2 = zBufSlopeInt;
    POLYGOUR_IntZBuf3 = zBufSlopeInt;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while ( 1 )
  {
    // @@EnterLoop:
    U8 *screen = curOffLine + (fillCurXMin >> 16);
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      // *** Gouraud dither loop ***

      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 curZBufMinDec = Fill_CurZBufMin << 24;
      U32 curZBufMinInt = Fill_CurZBufMin >> 8; // ebx

      U32 offset = screen - (U8 *)Log; // edi
      U32 gouraud = (curGouraud >> 8) & 0xFFFF; // esi
      U32 color = gouraud & 0xFF; // eax
      gouraud |= curZBufMinDec;
      color += gouraud;
      U8 colorIndex = (color >> 8) & 0xFF; // edx

      U8 isOdd = diffX & 1;
      U32 pixelCount = diffX >> 1;
      if (isOdd)
      {
        if (pixelCount != 0)
        {
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
          do
          {
            // @@Each_X:
            if (PtrZBuffer[offset] >= (curZBufMinInt & 0xFFFF))
            {
              PtrZBuffer[offset] = curZBufMinInt & 0xFFFF;
              POLYGOUR_PtrLog[offset] = Fill_Logical_Palette[colorIndex];
            }

            U8 carry = ((U64)gouraud + (U64)POLYGOUR_DecZBuf_AddGour) > 0xFFFFFFFF;
            gouraud += POLYGOUR_DecZBuf_AddGour;
            curZBufMinInt += POLYGOUR_IntZBuf + carry;

            color = (color & 0xFF) + gouraud;
            colorIndex = (colorIndex & 0xFFFFFF00) | (color >> 8) & 0xFF;

            if (PtrZBuffer[offset + 1] >= (curZBufMinInt & 0xFFFF))
            {
              PtrZBuffer[offset + 1] = curZBufMinInt & 0xFFFF;
              POLYGOUR_PtrLog_1[offset] = Fill_Logical_Palette[colorIndex];
            }

            // @@NoDisp1:

            carry = ((U64)gouraud + (U64)POLYGOUR_DecZBuf_AddGour1) > 0xFFFFFFFF;
            gouraud += POLYGOUR_DecZBuf_AddGour1;
            curZBufMinInt += POLYGOUR_IntZBuf1 + carry;

            color = (color & 0xFF) + gouraud;
            colorIndex = (colorIndex & 0xFFFFFF00) | (color >> 8) & 0xFF;

            color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
            pixelCount--;
            offset += 2;
          }
          while ( pixelCount );
        }
        // @@Add_One:
        if (PtrZBuffer[offset] >= (curZBufMinInt & 0xFFFF))
        {
          PtrZBuffer[offset] = curZBufMinInt & 0xFFFF;
          POLYGOUR_PtrLog4[offset] = Fill_Logical_Palette[colorIndex];
        }
      }
      else
      {
        // @@Pas_Add1:
        for ( ; pixelCount != 0; --pixelCount )
        {
          // @@Each_X2:
          if (PtrZBuffer[offset] >= (curZBufMinInt & 0xFFFF))
          {
            PtrZBuffer[offset] = curZBufMinInt & 0xFFFF;
            POLYGOUR_PtrLog2[offset] = Fill_Logical_Palette[colorIndex];
          }

          U8 carry = ((U64)gouraud + (U64)POLYGOUR_DecZBuf_AddGour2) > 0xFFFFFFFF;
          gouraud += POLYGOUR_DecZBuf_AddGour2;
          curZBufMinInt += POLYGOUR_IntZBuf2 + carry;

          color = (color & 0xFF) + gouraud;
          colorIndex = (colorIndex & 0xFFFFFF00) | (color >> 8) & 0xFF;

          if (PtrZBuffer[offset + 1] >= (curZBufMinInt & 0xFFFF))
          {
            PtrZBuffer[offset + 1] = curZBufMinInt & 0xFFFF;
            POLYGOUR_PtrLog2_1[offset] = Fill_Logical_Palette[colorIndex];
          }

          carry = ((U64)gouraud + (U64)POLYGOUR_DecZBuf_AddGour3) > 0xFFFFFFFF;
          gouraud += POLYGOUR_DecZBuf_AddGour3;
          curZBufMinInt += POLYGOUR_IntZBuf3 + carry;

          color = (color & 0xFF) + gouraud;
          colorIndex = (colorIndex & 0xFFFFFF00) | (color >> 8) & 0xFF;

          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
          offset += 2;
        }
      }
      // @@End_X:
    }
    // @@EndScanLine:
    if ( !--count )
      break;

    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_GouraudNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  Fill_CurY += count;
  U8 *curOffLine = Fill_CurOffLine;

  U32 patch = Fill_Patch - 1;
  U32 curGouraud = Fill_CurGouraudMin;

  if (patch == 0) {
    Fill_Patch = 0;
    // Variables are kept for consistency with assembly but not used in this simplified version
  }

  for (U32 lineNum = 0; lineNum < nbLines; lineNum++) {
    if (patch != 0) {
      curOffLine += ScreenPitch;
      fillCurXMin += Fill_LeftSlope;
      fillCurXMax += Fill_RightSlope;
      curGouraud += Fill_Gouraud_LeftSlope;
      Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
    }

    U32 startX = fillCurXMin >> 16;
    U32 endX = fillCurXMax >> 16;
    U8 *scanLine = curOffLine + startX;
    U32 pixelCount = endX - startX;

    if (pixelCount > 0) {
      // Subpixel correction for Gouraud
      U32 subPixel = fillCurXMin & 0xFFFF;
      subPixel ^= 0xFFFF;
      
      // Preload cache for large scanlines
      if (pixelCount > 16) {
        U32 gouraudCorrection = (subPixel * Fill_Gouraud_XSlope) >> 16;
        curGouraud += gouraudCorrection;
        
        // Preload destination cache lines
        U8 *preloadStart = (U8*)((uintptr_t)scanLine & ~31);
        U8 *preloadEnd = (U8*)((uintptr_t)(scanLine + pixelCount) & ~31);
        for (U8 *addr = preloadStart; addr <= preloadEnd; addr += 32) {
          volatile U8 temp = *addr; // Cache preload
        }
      }

      // Setup for pixel loop
      U32 zBuf = Fill_CurZBufMin;
      U32 zBufInt = zBuf >> 8;
      U32 zBufFrac = zBuf << (32 - 8);
      U8 *logPtr = (U8*)Log;
      U16 *zBufferPtr = (U16*)PtrZBuffer;
      
      U32 gouraudValue = curGouraud;
      U32 gouraudIntPart = Fill_Gouraud_XSlope >> 16;
      U32 gouraudFracPart = Fill_Gouraud_XSlope << 16;
      U8 gouraudPixel = gouraudValue >> 16;

      for (U32 x = 0; x < pixelCount; x++) {
        // Z-buffer test (NZW - test only, no write)
        U32 pixelOffset = (scanLine - logPtr) + x;
        if (zBufferPtr[pixelOffset] > zBufInt) {
          // Write Gouraud shaded pixel (no Z-buffer write for NZW)
          scanLine[x] = gouraudPixel;
        }

        // Advance interpolants
        gouraudValue += gouraudFracPart;
        gouraudPixel += gouraudIntPart;
        if (gouraudValue < gouraudFracPart) {
          gouraudPixel++; // Handle carry from fractional part
        }
        
        zBufFrac += Fill_ZBuf_XSlope << (32 - 8);
        zBufInt += (Fill_ZBuf_XSlope >> 8);
        if (zBufFrac < (Fill_ZBuf_XSlope << (32 - 8))) {
          zBufInt++;
        }
      }
    }

    patch = 1; // Subsequent lines don't need initialization
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_DitherNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  Fill_CurY += count;
  U8 *curOffLine = Fill_CurOffLine;

  U32 patch = Fill_Patch - 1;
  U32 curGouraud = Fill_CurGouraudMin;

  if (patch == 0) {
    Fill_Patch = 0;
    // Variables are kept for consistency with assembly but not used in this simplified version
  }

  for (U32 lineNum = 0; lineNum < nbLines; lineNum++) {
    if (patch != 0) {
      curOffLine += ScreenPitch;
      fillCurXMin += Fill_LeftSlope;
      fillCurXMax += Fill_RightSlope;
      curGouraud += Fill_Gouraud_LeftSlope;
      Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
    }

    U32 startX = fillCurXMin >> 16;
    U32 endX = fillCurXMax >> 16;
    U8 *scanLine = curOffLine + startX;
    U32 pixelCount = endX - startX;

    if (pixelCount > 0) {
      // Preload cache for large scanlines
      if (pixelCount >= 32) {
        U8 *preloadStart = (U8*)((uintptr_t)scanLine & ~31);
        U8 *preloadEnd = (U8*)((uintptr_t)(scanLine + pixelCount) & ~31);
        for (U8 *addr = preloadStart; addr <= preloadEnd; addr += 32) {
          volatile U8 temp = *addr; // Cache preload
        }
      }

      // Setup for pixel loop
      U32 zBuf = Fill_CurZBufMin;
      U32 zBufInt = zBuf >> 8;
      U32 zBufFrac = zBuf << (32 - 8);
      U8 *logPtr = (U8*)Log;
      U16 *zBufferPtr = (U16*)PtrZBuffer;
      
      U32 gouraudValue = curGouraud >> 8;
      U32 gouraudAddValue = (Fill_Gouraud_XSlope >> 8) & 0xFFFF;
      U8 ditherValue = gouraudValue & 0xFF;

      // Handle odd pixel count
      U32 remainingPixels = pixelCount;
      U32 pixelOffset = scanLine - logPtr;
      
      if (remainingPixels & 1) {
        remainingPixels--;
        if (remainingPixels == 0) {
          // Single pixel case
          if (zBufferPtr[pixelOffset] > zBufInt) {
            U8 finalColor = (ditherValue + gouraudValue) & 0xFF;
            scanLine[0] = finalColor; // NZW: no Z-buffer write
          }
          goto line_complete;
        }
        
        // Process single pixel first
        U8 rotatedValue = ditherValue;
        for (U32 i = 0; i < (remainingPixels & 7); i++) {
          rotatedValue = (rotatedValue << 1) | (rotatedValue >> 7);
        }
        
        U32 x = 0;
        while (remainingPixels > 0) {
          U8 finalColor1 = (rotatedValue + gouraudValue) & 0xFF;
          gouraudValue += gouraudAddValue;
          
          if (zBufferPtr[pixelOffset + x] > zBufInt) {
            scanLine[x] = finalColor1; // NZW: no Z-buffer write
          }
          
          zBufFrac += Fill_ZBuf_XSlope << (32 - 8);
          zBufInt += (Fill_ZBuf_XSlope >> 8);
          if (zBufFrac < (Fill_ZBuf_XSlope << (32 - 8))) {
            zBufInt++;
          }
          
          if (remainingPixels > 1) {
            U8 finalColor2 = (rotatedValue + gouraudValue) & 0xFF;
            gouraudValue += gouraudAddValue;
            
            if (zBufferPtr[pixelOffset + x + 1] > zBufInt) {
              scanLine[x + 1] = finalColor2; // NZW: no Z-buffer write
            }
            
            zBufFrac += Fill_ZBuf_XSlope << (32 - 8);
            zBufInt += (Fill_ZBuf_XSlope >> 8);
            if (zBufFrac < (Fill_ZBuf_XSlope << (32 - 8))) {
              zBufInt++;
            }
            
            x += 2;
            remainingPixels -= 2;
          } else {
            x++;
            remainingPixels--;
          }
          
          // Rotate dither value
          rotatedValue = (rotatedValue << 1) | (rotatedValue >> 7);
        }
      } else {
        // Even pixel count - process pairs
        U8 rotatedValue = ditherValue;
        for (U32 i = 0; i < (pixelCount & 7); i++) {
          rotatedValue = (rotatedValue << 1) | (rotatedValue >> 7);
        }
        
        for (U32 x = 0; x < pixelCount; x += 2) {
          U8 finalColor1 = (rotatedValue + gouraudValue) & 0xFF;
          gouraudValue += gouraudAddValue;
          
          if (zBufferPtr[pixelOffset + x] > zBufInt) {
            scanLine[x] = finalColor1; // NZW: no Z-buffer write
          }
          
          zBufFrac += Fill_ZBuf_XSlope << (32 - 8);
          zBufInt += (Fill_ZBuf_XSlope >> 8);
          if (zBufFrac < (Fill_ZBuf_XSlope << (32 - 8))) {
            zBufInt++;
          }
          
          U8 finalColor2 = (rotatedValue + gouraudValue) & 0xFF;
          gouraudValue += gouraudAddValue;
          
          if (zBufferPtr[pixelOffset + x + 1] > zBufInt) {
            scanLine[x + 1] = finalColor2; // NZW: no Z-buffer write
          }
          
          zBufFrac += Fill_ZBuf_XSlope << (32 - 8);
          zBufInt += (Fill_ZBuf_XSlope >> 8);
          if (zBufFrac < (Fill_ZBuf_XSlope << (32 - 8))) {
            zBufInt++;
          }
          
          // Rotate dither value
          rotatedValue = (rotatedValue << 1) | (rotatedValue >> 7);
        }
      }
    }

line_complete:
    patch = 1; // Subsequent lines don't need initialization
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_GouraudTableNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  Fill_CurY += count;
  U8 *curOffLine = Fill_CurOffLine;

  U32 patch = Fill_Patch - 1;
  U32 curGouraud = Fill_CurGouraudMin;

  if (patch == 0) {
    Fill_Patch = 0;
    // Variables are kept for consistency with assembly but not used in this simplified version
  }

  for (U32 lineNum = 0; lineNum < nbLines; lineNum++) {
    if (patch != 0) {
      curOffLine += ScreenPitch;
      fillCurXMin += Fill_LeftSlope;
      fillCurXMax += Fill_RightSlope;
      curGouraud += Fill_Gouraud_LeftSlope;
      Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
    }

    U32 startX = fillCurXMin >> 16;
    U32 endX = fillCurXMax >> 16;
    U8 *scanLine = curOffLine + startX;
    U32 pixelCount = endX - startX;

    if (pixelCount > 0) {
      // Subpixel correction for Gouraud
      U32 subPixel = fillCurXMin & 0xFFFF;
      subPixel ^= 0xFFFF;
      
      // Preload cache for large scanlines
      if (pixelCount > 16) {
        U32 gouraudCorrection = (subPixel * Fill_Gouraud_XSlope) >> 16;
        curGouraud += gouraudCorrection;
        
        // Preload destination cache lines
        U8 *preloadStart = (U8*)((uintptr_t)scanLine & ~31);
        U8 *preloadEnd = (U8*)((uintptr_t)(scanLine + pixelCount) & ~31);
        for (U8 *addr = preloadStart; addr <= preloadEnd; addr += 32) {
          volatile U8 temp = *addr; // Cache preload
        }
      }

      // Setup for pixel loop
      U8 *colorTable = Fill_Color.Ptr; // Color lookup table
      U32 zBuf = Fill_CurZBufMin;
      U32 zBufInt = zBuf >> 8;
      U32 zBufFrac = zBuf << (32 - 8);
      U8 *logPtr = (U8*)Log;
      U16 *zBufferPtr = (U16*)PtrZBuffer;
      
      U32 gouraudValue = curGouraud;
      U32 gouraudIndex = (gouraudValue >> 8) & 0xFF00;

      for (U32 x = 0; x < pixelCount; x++) {
        // Z-buffer test (NZW - test only, no write)
        U32 pixelOffset = (scanLine - logPtr) + x;
        if (zBufferPtr[pixelOffset] > zBufInt) {
          // Write color using lookup table (no Z-buffer write for NZW)
          U8 colorIndex = colorTable[gouraudIndex];
          scanLine[x] = colorIndex;
        }

        // Advance interpolants
        gouraudValue += Fill_Gouraud_XSlope;
        gouraudIndex = (gouraudValue >> 8) & 0xFF00;
        
        zBufFrac += Fill_ZBuf_XSlope << (32 - 8);
        zBufInt += (Fill_ZBuf_XSlope >> 8);
        if (zBufFrac < (Fill_ZBuf_XSlope << (32 - 8))) {
          zBufInt++;
        }
      }
    }

    patch = 1; // Subsequent lines don't need initialization
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_DitherTableNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 gouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    // Setup Z-buffer pointers for pixel pairs and individual pixels
    POLYGOUR_PtrZBufferPatch = PtrZBuffer;
    POLYGOUR_PtrZBuffer1 = PtrZBuffer;
    POLYGOUR_PtrZBuffer2 = PtrZBuffer;
    POLYGOUR_PtrZBuffer3 = PtrZBuffer;
    POLYGOUR_PtrZBuffer4 = PtrZBuffer;

    // Setup screen buffer pointers with offset adjustments for pixel pairs
    POLYGOUR_PtrLog = (PTR_U8)Log;
    POLYGOUR_PtrLog2 = (PTR_U8)Log;
    POLYGOUR_PtrLog4 = (PTR_U8)Log;
    POLYGOUR_PtrLog_1 = (PTR_U8)Log + 1;
    POLYGOUR_PtrLog2_1 = (PTR_U8)Log + 1;

    // Setup Gouraud increments (integer part for dithering)
    S32 gouraudXSlope = Fill_Gouraud_XSlope >> 8;
    POLYGOUR_AddGour = gouraudXSlope;
    POLYGOUR_AddGour1 = gouraudXSlope;
    POLYGOUR_AddGour2 = gouraudXSlope;
    POLYGOUR_AddGour3 = gouraudXSlope;

    // Setup Z-buffer increments
    POLYGOUR_IntZBuf = (U32)Fill_ZBuf_XSlope >> 8;
    POLYGOUR_IntZBuf1 = (U32)Fill_ZBuf_XSlope >> 8;
    POLYGOUR_IntZBuf2 = (U32)Fill_ZBuf_XSlope >> 8;
    POLYGOUR_IntZBuf3 = (U32)Fill_ZBuf_XSlope >> 8;
    POLYGOUR_DecZBuf = (U32)Fill_ZBuf_XSlope << 24;
    POLYGOUR_DecZBuf1 = (U32)Fill_ZBuf_XSlope << 24;
    POLYGOUR_DecZBuf2 = (U32)Fill_ZBuf_XSlope << 24;
    POLYGOUR_DecZBuf3 = (U32)Fill_ZBuf_XSlope << 24;

    // Setup color lookup table pointers for all pixel positions
    POLYGOUR_PtrColor = Fill_Color.Ptr;
    POLYGOUR_PtrColor1 = Fill_Color.Ptr;
    POLYGOUR_PtrColor2 = Fill_Color.Ptr;
    POLYGOUR_PtrColor3 = Fill_Color.Ptr;
    POLYGOUR_PtrColor4 = Fill_Color.Ptr;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      PTR_U8 screenPtr = (PTR_U8)Fill_CurOffLine + (fillCurXMin >> 16);
      
      // Cache preloading for larger scanlines
      if (fillWidth >= 32)
      {
        PTR_U8 cachePtr = (PTR_U8)(((uintptr_t)screenPtr) & ~31);
        PTR_U8 cacheEnd = (PTR_U8)(((uintptr_t)(screenPtr + fillWidth)) & ~31);
        while (cachePtr <= cacheEnd)
        {
          volatile U8 dummy = *cachePtr;
          cachePtr += 32;
        }
      }

      // Extract Gouraud intensity and setup for dithering
      S32 gouraudShifted = gouraud >> 8;
      U8 ditherBase = gouraudShifted & 0xFF;
      U32 colorIndex = (gouraudShifted & 0xFF) + gouraudShifted;
      colorIndex &= 0xFF00;

      // Setup Z-buffer values
      U32 zBufInt = Fill_CurZBufMin >> 8;
      U32 zBufFrac = Fill_CurZBufMin << 24;

      // Calculate screen offset relative to Log base
      S32 screenOffset = screenPtr - (PTR_U8)Log;

      // Handle pixel pairs (dithering processes 2 pixels at a time)
      U32 pixelPairs = fillWidth >> 1;
      
      if (fillWidth & 1) // Odd number of pixels
      {
        if (pixelPairs > 0)
        {
          // Setup bit rotation for dithering pattern
          U8 rotatedDither = ditherBase;
          for (U32 i = 0; i < pixelPairs; i++)
          {
            rotatedDither = (rotatedDither << 1) | (rotatedDither >> 7);
          }
          
          // Process pixel pairs
          while (pixelPairs > 0)
          {
            // First pixel of pair
            if (POLYGOUR_PtrZBufferPatch[screenOffset] >= zBufInt)
            {
              U8 colorValue = POLYGOUR_PtrColor[colorIndex];
              POLYGOUR_PtrLog[screenOffset] = colorValue;
            }

            // Update for next pixel
            ditherBase &= 0xFF;
            gouraudShifted += POLYGOUR_AddGour;
            ditherBase += gouraudShifted;
            zBufFrac += POLYGOUR_DecZBuf;
            zBufInt += POLYGOUR_IntZBuf + (zBufFrac < POLYGOUR_DecZBuf ? 1 : 0);
            colorIndex = (ditherBase + gouraudShifted) & 0xFF00;

            // Second pixel of pair
            if (POLYGOUR_PtrZBuffer1[screenOffset] >= zBufInt)
            {
              U8 colorValue = POLYGOUR_PtrColor1[colorIndex];
              POLYGOUR_PtrLog_1[screenOffset] = colorValue;
            }

            // Update for next pair
            gouraudShifted += POLYGOUR_AddGour1;
            ditherBase &= 0xFF;
            ditherBase += gouraudShifted;
            zBufFrac += POLYGOUR_DecZBuf1;
            zBufInt += POLYGOUR_IntZBuf1 + (zBufFrac < POLYGOUR_DecZBuf1 ? 1 : 0);
            colorIndex = (ditherBase + gouraudShifted) & 0xFF00;

            screenOffset += 2;
            rotatedDither = (rotatedDither << 1) | (rotatedDither >> 7);
            pixelPairs--;
          }
        }

        // Handle single remaining pixel
        if (POLYGOUR_PtrZBuffer4[screenOffset] >= zBufInt)
        {
          U8 colorValue = POLYGOUR_PtrColor4[colorIndex];
          POLYGOUR_PtrLog4[screenOffset] = colorValue;
        }
      }
      else if (pixelPairs > 0) // Even number of pixels
      {
        // Setup bit rotation for dithering pattern
        U8 rotatedDither = ditherBase;
        for (U32 i = 0; i < pixelPairs; i++)
        {
          rotatedDither = (rotatedDither << 1) | (rotatedDither >> 7);
        }
        
        // Process pixel pairs
        while (pixelPairs > 0)
        {
          // First pixel of pair
          if (POLYGOUR_PtrZBuffer2[screenOffset] >= zBufInt)
          {
            U8 colorValue = POLYGOUR_PtrColor2[colorIndex];
            POLYGOUR_PtrLog2[screenOffset] = colorValue;
          }

          // Update for next pixel
          ditherBase &= 0xFF;
          gouraudShifted += POLYGOUR_AddGour2;
          ditherBase += gouraudShifted;
          zBufFrac += POLYGOUR_DecZBuf2;
          zBufInt += POLYGOUR_IntZBuf2 + (zBufFrac < POLYGOUR_DecZBuf2 ? 1 : 0);
          colorIndex = (ditherBase + gouraudShifted) & 0xFF00;

          // Second pixel of pair
          if (POLYGOUR_PtrZBuffer3[screenOffset] >= zBufInt)
          {
            U8 colorValue = POLYGOUR_PtrColor3[colorIndex];
            POLYGOUR_PtrLog2_1[screenOffset] = colorValue;
          }

          // Update for next pair
          gouraudShifted += POLYGOUR_AddGour3;
          ditherBase &= 0xFF;
          ditherBase += gouraudShifted;
          zBufFrac += POLYGOUR_DecZBuf3;
          zBufInt += POLYGOUR_IntZBuf3 + (zBufFrac < POLYGOUR_DecZBuf3 ? 1 : 0);
          colorIndex = (ditherBase + gouraudShifted) & 0xFF00;

          screenOffset += 2;
          rotatedDither = (rotatedDither << 1) | (rotatedDither >> 7);
          pixelPairs--;
        }
      }
    }

    diffY--;
    if (diffY == 0)
      break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurGouraudMin = gouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_GouraudFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 curGouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYGOUR_IntGour = (Fill_Gouraud_XSlope >> 16) & 0xFF;
    POLYGOUR_DecGour = Fill_Gouraud_XSlope << 16;
    POLYGOUR_DecZBuf = Fill_ZBuf_XSlope << 24;
    POLYGOUR_IntZBuf = Fill_ZBuf_XSlope >> 8;
    POLYGOUR_PtrZBufferPatch = PtrZBuffer;
    POLYGOUR_PtrLog = (U8 *)Log;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while ( 1 )
  {
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);

    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      U32 lineGouraud = curGouraud;

      if (diffX > 16)
      {
        U64 addGouraud = ((U64)factor * (U64)Fill_Gouraud_XSlope);
        U32 addGouraudDec = (U32)(addGouraud >> 32);
        U32 addGouraudInt = (U32)(addGouraud & 0xFFFFFFFF);
        addGouraudDec = addGouraudDec << 16;
        addGouraudInt = addGouraudInt >> 16;
        addGouraudDec |= addGouraudInt;
        lineGouraud += addGouraudDec;
      }

      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 curZBufMinDec = Fill_CurZBufMin << 24; // ebp
      U32 curZBufMinInt = Fill_CurZBufMin >> 8; // ebx

      U32 color = (lineGouraud >> 16) & 0xFF; // eax
      lineGouraud <<= 16;
      U32 offset = screen - (U8 *)Log; // edi

      do
      {
        if (POLYGOUR_PtrZBufferPatch[offset] >= (curZBufMinInt & 0xFFFF))
        {
          POLYGOUR_PtrLog[offset] = Fill_Logical_Palette[color];
        }

        ++offset;

        U8 carry = ((U64)lineGouraud + (U64)POLYGOUR_DecGour) > 0xFFFFFFFF;
        lineGouraud += POLYGOUR_DecGour;
        color = (color & 0xFFFFFF00) | (((POLYGOUR_IntGour & 0xFF) + carry + (color & 0xFF)) & 0xFF);

        carry = ((U64)curZBufMinDec + (U64)POLYGOUR_DecZBuf) > 0xFFFFFFFF;
        curZBufMinDec += POLYGOUR_DecZBuf;
        curZBufMinInt += POLYGOUR_IntZBuf + carry;

        --diffX;
      }
      while (diffX);
    }

    if ( !--count )
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_DitherFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  Fill_CurY += count;
  U8 *curOffLine = Fill_CurOffLine;
  U32 curGouraud = Fill_CurGouraudMin;

  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    U32 decZBuf_addGour = Fill_Gouraud_XSlope >> 8;
    decZBuf_addGour &= 0xFFFF;
    decZBuf_addGour |= Fill_ZBuf_XSlope << 24;

    POLYGOUR_DecZBuf_AddGour = decZBuf_addGour;
    POLYGOUR_DecZBuf_AddGour1 = decZBuf_addGour;
    POLYGOUR_DecZBuf_AddGour2 = decZBuf_addGour;
    POLYGOUR_DecZBuf_AddGour3 = decZBuf_addGour;

    POLYGOUR_PtrLog = (U8 *)Log;
    POLYGOUR_PtrLog2 = (U8 *)Log;
    POLYGOUR_PtrLog4 = (U8 *)Log;

    POLYGOUR_PtrLog_1 = (U8 *)Log + 1;
    POLYGOUR_PtrLog2_1 = (U8 *)Log + 1;

    U32 zBufSlopeInt = Fill_ZBuf_XSlope >> 8;
    POLYGOUR_IntZBuf = zBufSlopeInt;
    POLYGOUR_IntZBuf1 = zBufSlopeInt;
    POLYGOUR_IntZBuf2 = zBufSlopeInt;
    POLYGOUR_IntZBuf3 = zBufSlopeInt;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while ( 1 )
  {
    // @@EnterLoop:
    U8 *screen = curOffLine + (fillCurXMin >> 16);
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      // *** Gouraud dither loop ***

      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 curZBufMinDec = Fill_CurZBufMin << 24;
      U32 curZBufMinInt = Fill_CurZBufMin >> 8; // ebx

      U32 offset = screen - (U8 *)Log; // edi
      U32 gouraud = (curGouraud >> 8) & 0xFFFF; // esi
      U32 color = gouraud & 0xFF; // eax
      gouraud |= curZBufMinDec;
      color += gouraud;
      U8 colorIndex = (color >> 8) & 0xFF; // edx

      U8 isOdd = diffX & 1;
      U32 pixelCount = diffX >> 1;
      if (isOdd)
      {
        if (pixelCount != 0)
        {
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
          do
          {
            // @@Each_X:
            if (PtrZBuffer[offset] >= (curZBufMinInt & 0xFFFF))
            {
              POLYGOUR_PtrLog[offset] = Fill_Logical_Palette[colorIndex];
            }

            U8 carry = ((U64)gouraud + (U64)POLYGOUR_DecZBuf_AddGour) > 0xFFFFFFFF;
            gouraud += POLYGOUR_DecZBuf_AddGour;
            curZBufMinInt += POLYGOUR_IntZBuf + carry;

            color = (color & 0xFF) + gouraud;
            colorIndex = (colorIndex & 0xFFFFFF00) | (color >> 8) & 0xFF;

            if (PtrZBuffer[offset + 1] >= (curZBufMinInt & 0xFFFF))
            {
              POLYGOUR_PtrLog_1[offset] = Fill_Logical_Palette[colorIndex];
            }

            // @@NoDisp1:

            carry = ((U64)gouraud + (U64)POLYGOUR_DecZBuf_AddGour1) > 0xFFFFFFFF;
            gouraud += POLYGOUR_DecZBuf_AddGour1;
            curZBufMinInt += POLYGOUR_IntZBuf1 + carry;

            color = (color & 0xFF) + gouraud;
            colorIndex = (colorIndex & 0xFFFFFF00) | (color >> 8) & 0xFF;

            color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
            pixelCount--;
            offset += 2;
          }
          while ( pixelCount );
        }
        // @@Add_One:
        if (PtrZBuffer[offset] >= (curZBufMinInt & 0xFFFF))
        {
          POLYGOUR_PtrLog4[offset] = Fill_Logical_Palette[colorIndex];
        }
      }
      else
      {
        // @@Pas_Add1:
        for ( ; pixelCount != 0; --pixelCount )
        {
          // @@Each_X2:
          if (PtrZBuffer[offset] >= (curZBufMinInt & 0xFFFF))
          {
            POLYGOUR_PtrLog2[offset] = Fill_Logical_Palette[colorIndex];
          }

          U8 carry = ((U64)gouraud + (U64)POLYGOUR_DecZBuf_AddGour2) > 0xFFFFFFFF;
          gouraud += POLYGOUR_DecZBuf_AddGour2;
          curZBufMinInt += POLYGOUR_IntZBuf2 + carry;

          color = (color & 0xFF) + gouraud;
          colorIndex = (colorIndex & 0xFFFFFF00) | (color >> 8) & 0xFF;

          if (PtrZBuffer[offset + 1] >= (curZBufMinInt & 0xFFFF))
          {
            POLYGOUR_PtrLog2_1[offset] = Fill_Logical_Palette[colorIndex];
          }

          carry = ((U64)gouraud + (U64)POLYGOUR_DecZBuf_AddGour3) > 0xFFFFFFFF;
          gouraud += POLYGOUR_DecZBuf_AddGour3;
          curZBufMinInt += POLYGOUR_IntZBuf3 + carry;

          color = (color & 0xFF) + gouraud;
          colorIndex = (colorIndex & 0xFFFFFF00) | (color >> 8) & 0xFF;

          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount & 0xFF);
          offset += 2;
        }
      }
      // @@End_X:
    }
    // @@EndScanLine:
    if ( !--count )
      break;

    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curGouraud += Fill_Gouraud_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}
