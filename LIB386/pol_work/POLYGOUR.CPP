#include <POLYGON/POLYGOUR.H>

#include <POLYGON/POLY.H>
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

;				******************
;				*** Local data ***
;				******************
			ALIGN	4

; Runtime code patch refactor, uses memory instead of instruction substitution
AddGour             DWORD 12345678h
AddGour1            DWORD 12345678h
AddGour2            DWORD 12345678h
AddGour3            DWORD 12345678h
IntZBuf             DWORD 12345678h
IntZBuf1            DWORD 12345678h
IntZBuf2            DWORD 12345678h
IntZBuf3            DWORD 12345678h
DecGour             DWORD 12345678h
DecZBuf             DWORD 12345678h
DecZBuf1            DWORD 12345678h
DecZBuf2            DWORD 12345678h
DecZBuf3            DWORD 12345678h
DecZBuf_AddGour     DWORD 12345678h
DecZBuf_AddGour1    DWORD 12345678h
DecZBuf_AddGour2    DWORD 12345678h
DecZBuf_AddGour3    DWORD 12345678h
PtrLog              DWORD 12345678h
PtrLog_1            DWORD 12345678h
PtrLog2             DWORD 12345678h
PtrLog2_1           DWORD 12345678h
PtrLog4             DWORD 12345678h
PtrZBuffer_1        DWORD 12345678h
PtrZBuffer1         DWORD 12345678h
PtrZBuffer1_1       DWORD 12345678h
PtrZBuffer2         DWORD 12345678h
PtrZBuffer2_1       DWORD 12345678h
PtrZBuffer3         DWORD 12345678h
PtrZBuffer3_1       DWORD 12345678h
PtrZBuffer4         DWORD 12345678h
PtrZBuffer5         DWORD 12345678h
PtrZBufferPatch     DWORD 12345678h
PtrColor            DWORD 12345678h
PtrColor1           DWORD 12345678h
PtrColor2           DWORD 12345678h
PtrColor3           DWORD 12345678h
PtrColor4           DWORD 12345678h
IntGour             BYTE  12h

*/
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
      U32 gouraudSlope = Fill_Gouraud_XSlope << 16;
      U32 gouraudSlopeMinor = Fill_Gouraud_XSlope >> 16;
      U32 color = curGouraud >> 16;
      U32 gouraud = curGouraud << 16;
      do
      {
        *screen = color;
        U8 carryFlag = ((U64)gouraudSlope + (U64)gouraud) > 0xFFFFFFFF;
        gouraud += gouraudSlope;
        color = (color & 0xFFFFFF00) | ((gouraudSlopeMinor + carryFlag + color) & 0xFF);
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
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_DitherFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_GouraudZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_DitherZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
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
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
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
