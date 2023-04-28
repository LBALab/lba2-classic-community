#include <POLYGON/POLYFLAT.H>

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>

#include <stdio.h>

U8 *POLYFLAT_PtrLog = NULL;
U8 *POLYFLAT_PtrLog1 = NULL;
U8 *POLYFLAT_PtrLog2 = NULL;
U16 *POLYFLAT_PtrZBuffer1 = NULL;
U16 *POLYFLAT_PtrZBuffer2 = NULL;
U16 *POLYFLAT_PtrZBuffer3 = NULL;
U16 *POLYFLAT_PtrZBufferPatch = NULL;

S32 Filler_Flat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = (U8 *)Fill_CurOffLine;
  Fill_CurY += count;
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
  }

  while (1)
  {
    // @@EnterLoop:
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      // Is the scanline big enough to load the dst lines in cache
      // @@SmallScanLine:
      U8 color = Fill_Color; // Color
      do
      {
        *screen = color;
        screen++;
        --diffX;
      } while (diffX);
    }
    // @@EndScanLine:
    if (!--count)
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;

  return Triangle_ReadNextEdge(NULL, NULL);
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_Transparent	PROC


; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_Trame		PROC
*/

// *** CALL: 	ECX = Nb lines to draw
//		EBX = Fill_CurXMin
//		EDX = Fill_CurXMax
S32 Filler_FlatZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  S32 curZ = Fill_CurZBufMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYFLAT_PtrZBufferPatch = PtrZBuffer;
    POLYFLAT_PtrZBuffer1 = PtrZBuffer;
    POLYFLAT_PtrLog = (U8 *)Log;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curZ += Fill_ZBuf_LeftSlope;
  }

  while ( 1 )
  {
    curOffLine = curOffLine;
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    S32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      S32 zHigh = curZ << 24;
      S32 zSlopeHigh = Fill_ZBuf_XSlope << 24;
      U32 zSlopeLow = (unsigned int)Fill_ZBuf_XSlope >> 8;
      U32 screenDiff = screen - (U8 *)Log;
      S32 zLow = curZ >> 8;
      U8 color = Fill_Color;
      do
      {
        if ( *(POLYFLAT_PtrZBufferPatch + screenDiff) >= (U16)zLow )
        {
          *(POLYFLAT_PtrZBuffer1 + screenDiff) = zLow;
          *(POLYFLAT_PtrLog + screenDiff) = color;
        }
        ++screenDiff;
        U8 carry = (U64)zSlopeHigh + (U64)zHigh > 0xFFFFFFFF;
        zHigh += zSlopeHigh;
        zLow += zSlopeLow + carry;
        --diffX;
      }
      while (diffX);
    }
    curOffLine = curOffLine;
    if (!--count)
    {
      break;
    }

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curZ += Fill_ZBuf_LeftSlope;
  }
  Fill_CurZBufMin = curZ;
  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;

  return Triangle_ReadNextEdge(NULL, NULL);
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TransparentZBuf	PROC



; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TrameZBuf	PROC
*/

// *** CALL: 	ECX = Nb lines to draw
//		EBX = Fill_CurXMin
//		EDX = Fill_CurXMax
S32 Filler_FlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TransparentNZW	PROC


; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TrameNZW		PROC

; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_FlagZBuffer	PROC
*/
