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
S32 POLYGOUR_AddGour = 0x12345678;
S32 POLYGOUR_AddGour1 = 0x12345678;
S32 POLYGOUR_AddGour2 = 0x12345678;
S32 POLYGOUR_AddGour3 = 0x12345678;
S32 POLYGOUR_IntZBuf = 0x12345678;
S32 POLYGOUR_IntZBuf1 = 0x12345678;
S32 POLYGOUR_IntZBuf2 = 0x12345678;
S32 POLYGOUR_IntZBuf3 = 0x12345678;
S32 POLYGOUR_DecGour = 0x12345678;
S32 POLYGOUR_DecZBuf = 0x12345678;
S32 POLYGOUR_DecZBuf1 = 0x12345678;
S32 POLYGOUR_DecZBuf2 = 0x12345678;
S32 POLYGOUR_DecZBuf3 = 0x12345678;
S32 POLYGOUR_DecZBuf_AddGour = 0x12345678;
S32 POLYGOUR_DecZBuf_AddGour1 = 0x12345678;
S32 POLYGOUR_DecZBuf_AddGour2 = 0x12345678;
S32 POLYGOUR_DecZBuf_AddGour3 = 0x12345678;
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
      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 gouraudSlopeFrac = Fill_Gouraud_XSlope << 16;
      U32 gouraudSlopeInt = Fill_Gouraud_XSlope >> 16;
      U32 color = curGouraud >> 16;
      U32 gouraud = curGouraud << 16;
      do
      {
        *screen = color;
        U8 carryFlag = ((U64)gouraudSlopeFrac + (U64)gouraud) > 0xFFFFFFFF;
        gouraud += gouraudSlopeFrac;
        color = (color & 0xFFFFFF00) | ((gouraudSlopeInt + carryFlag + color) & 0xFF);
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

S32 Filler_Dither(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
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
      U32 gouraud = curGouraud >> 8;
      U32 color = gouraud;
      U8 isOdd = diffX & 1;
      U32 pixelCount = diffX >> 1;
      if (isOdd)
      {
        if (pixelCount != 0)
        {
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount);
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
            color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount);
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
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount);
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
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_DitherTable(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

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
      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 gouraudSlopeFrac = Fill_Gouraud_XSlope << 16;
      U32 gouraudSlopeInt = Fill_Gouraud_XSlope >> 16;
      U32 color = (curGouraud >> 16) & 0xFF;
      U32 gouraud = curGouraud << 16;
      do
      {
        *screen = Fill_Logical_Palette[color];
        U8 carryFlag = ((U64)gouraudSlopeFrac + (U64)gouraud) > 0xFFFFFFFF;
        gouraud += gouraudSlopeFrac;
        color = (color & 0xFFFFFF00) | ((gouraudSlopeInt + carryFlag + color) & 0xFF);
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

S32 Filler_DitherFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  // TODO: make sure this works as intended
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
      U32 gouraud = curGouraud >> 8;
      U32 color = gouraud;
      U8 isOdd = diffX & 1;
      U32 pixelCount = diffX >> 1;
      if (isOdd)
      {
        if (pixelCount != 0)
        {
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount);
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
            color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount);
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
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount);
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
      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 gouraudSlopeInt = Fill_Gouraud_XSlope >> 16;
      U32 curZBufMinFrac = Fill_CurZBufMin << 24;
      U32 curZBufMinInt = Fill_CurZBufMin >> 8;

      U32 offset = screen - (U8 *)Log;
      U32 color = curGouraud >> 16;
      U32 gouraud = curGouraud << 16;

      do
      {
        if (POLYGOUR_PtrZBufferPatch[offset] >= (curZBufMinInt & 0xFFFF))
        {
          POLYGOUR_PtrZBuffer1[offset] = curZBufMinInt & 0xFFFF;
          POLYGOUR_PtrLog[offset] = color & 0xFF;
        }

        U8 carryFlag = ((U64)gouraud + (U64)POLYGOUR_DecGour) > 0xFFFFFFFF;
        gouraud += POLYGOUR_DecGour;
        color = (color & 0xFFFFFF00) | ((gouraudSlopeInt + carryFlag + color) & 0xFF);

        carryFlag = ((U64)curZBufMinFrac + (U64)POLYGOUR_DecZBuf) > 0xFFFFFFFF;
        curZBufMinFrac += POLYGOUR_DecZBuf;
        curZBufMinInt = (curZBufMinInt & 0xFFFFFF00) | ((POLYGOUR_IntZBuf + carryFlag + curZBufMinInt) & 0xFF);

        ++screen;
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

S32 Filler_DitherZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  // TODO: Review this code carefully

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

    S32 zBufSlopeInt = Fill_ZBuf_XSlope >> 8;
    POLYGOUR_IntZBuf = zBufSlopeInt;
    POLYGOUR_IntZBuf1 = zBufSlopeInt;
    POLYGOUR_IntZBuf2 = zBufSlopeInt;
    POLYGOUR_IntZBuf3 = zBufSlopeInt;

    S32 zBufSlopeDec = Fill_ZBuf_XSlope << 24;
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
      U32 curZBufMinFrac = Fill_CurZBufMin << 24;
      U32 curZBufMinInt = Fill_CurZBufMin >> 8;

      U32 offset = screen - (U8 *)Log;
      U32 gouraud = curGouraud >> 8;
      U32 color = gouraud;
      U8 isOdd = diffX & 1;
      U32 pixelCount = diffX >> 1;
      if (isOdd)
      {
        if (pixelCount != 0)
        {
          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount);
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

            U8 carryFlag = ((U64)curZBufMinFrac + (U64)POLYGOUR_DecZBuf) > 0xFFFFFFFF;
            curZBufMinFrac += POLYGOUR_DecZBuf;
            curZBufMinInt = (curZBufMinInt & 0xFFFFFF00) | ((POLYGOUR_IntZBuf + carryFlag + curZBufMinInt) & 0xFF);

            if (POLYGOUR_PtrZBuffer1[offset] >= (curZBufMinInt & 0xFFFF))
            {
              POLYGOUR_PtrZBuffer1_1[offset] = curZBufMinInt & 0xFFFF;
              POLYGOUR_PtrLog_1[offset] = (color >> 8) & 0xFF;
            }

            gouraud += POLYGOUR_AddGour1;

            carryFlag = ((U64)curZBufMinFrac + (U64)POLYGOUR_DecZBuf1) > 0xFFFFFFFF;
            curZBufMinFrac += POLYGOUR_DecZBuf1;
            curZBufMinInt = (curZBufMinInt & 0xFFFFFF00) | ((POLYGOUR_IntZBuf1 + carryFlag + curZBufMinInt) & 0xFF);

            color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount);
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

          U8 carryFlag = ((U64)curZBufMinFrac + (U64)POLYGOUR_DecZBuf2) > 0xFFFFFFFF;
          curZBufMinFrac += POLYGOUR_DecZBuf2;
          curZBufMinInt = (curZBufMinInt & 0xFFFFFF00) | ((POLYGOUR_IntZBuf2 + carryFlag + curZBufMinInt) & 0xFF);

          if (POLYGOUR_PtrZBuffer2_1[offset] >= (curZBufMinInt & 0xFFFF))
          {
            POLYGOUR_PtrZBuffer3_1[offset] = curZBufMinInt & 0xFFFF;
            POLYGOUR_PtrLog2_1[offset] = (color >> 8) & 0xFF;
          }

          carryFlag = ((U64)curZBufMinFrac + (U64)POLYGOUR_DecZBuf3) > 0xFFFFFFFF;
          curZBufMinFrac += POLYGOUR_DecZBuf3;
          curZBufMinInt = (curZBufMinInt & 0xFFFFFF00) | ((POLYGOUR_IntZBuf3 + carryFlag + curZBufMinInt) & 0xFF);

          gouraud += POLYGOUR_AddGour3;

          color = (color & 0xFFFFFF00) | ROL8(color & 0xFF, pixelCount);
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
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_GouraudTableZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_DitherTableZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

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
      // There is a bunch of cache-related stuff here in the original ASM, but
      // it's not needed in the C version AFAICT.

      // @@NoPreload:
      U32 curZBufMinFrac = Fill_CurZBufMin << 24;
      U32 curZBufMinInt = Fill_CurZBufMin >> 8;

      U32 color = (curGouraud >> 16) & 0xFF;
      U32 gouraud = curGouraud << 16;
      U32 offset = screen - (U8 *)Log;

      do
      {
        if (POLYGOUR_PtrZBufferPatch[offset] >= (curZBufMinInt & 0xFFFF))
        {
          POLYGOUR_PtrZBuffer1[offset] = curZBufMinInt & 0xFFFF;
          POLYGOUR_PtrLog[offset] = Fill_Logical_Palette[color];
        }

        U8 carryFlag = ((U64)gouraud + (U64)POLYGOUR_DecGour) > 0xFFFFFFFF;
        gouraud += POLYGOUR_DecGour;
        color = (color & 0xFFFFFF00) | ((POLYGOUR_IntGour + carryFlag + color) & 0xFF);

        carryFlag = ((U64)curZBufMinFrac + (U64)POLYGOUR_DecZBuf) > 0xFFFFFFFF;
        curZBufMinFrac += POLYGOUR_DecZBuf;
        curZBufMinInt = (curZBufMinInt & 0xFFFFFF00) | ((POLYGOUR_IntZBuf + carryFlag + curZBufMinInt) & 0xFF);

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
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurGouraudMin = curGouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_DitherFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_GouraudNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_DitherNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_GouraudTableNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_DitherTableNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_GouraudFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_DitherFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}
