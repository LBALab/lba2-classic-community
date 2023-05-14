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

// Verified
S32 Filler_Flat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
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
      do
      {
        *screen = Fill_Color.Num;
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

// Verified
S32 Filler_FlatZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 curZ = Fill_CurZBufMin;
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
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U32 zDec = curZ << 24; // esi
      U32 zDecSlope = Fill_ZBuf_XSlope << 24; // ebp
      U32 zIntSlope = Fill_ZBuf_XSlope >> 8; // ebx
      U32 offset = screen - (U8 *)Log; // edi
      U32 zInt = curZ >> 8; // edx
      do
      {
        if (POLYFLAT_PtrZBufferPatch[offset] >= (zInt & 0xFFFF))
        {
          POLYFLAT_PtrZBuffer1[offset] = zInt & 0xFFFF;
          POLYFLAT_PtrLog[offset] = Fill_Color.Num & 0xFF;
        }

        ++offset;
        U8 carry = (U64)zDec + (U64)zDecSlope > 0xFFFFFFFF;
        zDec += zDecSlope;
        zInt += zIntSlope + carry;
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
    curZ += Fill_ZBuf_LeftSlope;
  }

  Fill_CurZBufMin = curZ;
  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;

  return Triangle_ReadNextEdge(NULL, NULL);
}

// TODO: Verify
S32 Filler_TransparentZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 curZ = Fill_CurZBufMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYFLAT_PtrZBufferPatch = PtrZBuffer;
    POLYFLAT_PtrZBuffer1 = PtrZBuffer;
    POLYFLAT_PtrLog = (U8 *)Log;
    POLYFLAT_PtrLog1 = (U8 *)Log;
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
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U32 zDec = curZ << 24; // esi
      U32 zDecSlope = Fill_ZBuf_XSlope << 24; // ebp
      U32 zIntSlope = Fill_ZBuf_XSlope >> 8; // ebx
      U32 offset = screen - (U8 *)Log; // edi
      U32 zInt = curZ >> 8; // edx
      U8 color = Fill_Color.Num & 0xFF;

      do
      {
        if (POLYFLAT_PtrZBufferPatch[offset] >= (zInt & 0xFFFF))
        {
          POLYFLAT_PtrZBuffer1[offset] = zInt & 0xFFFF;
          U8 logColor = POLYFLAT_PtrLog[offset] & 0x0F;
          POLYFLAT_PtrLog1[offset] = color | logColor;
        }

        ++offset;
        U8 carry = (U64)zDec + (U64)zDecSlope > 0xFFFFFFFF;
        zDec += zDecSlope;
        zInt += zIntSlope + carry;
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
    curZ += Fill_ZBuf_LeftSlope;
  }

  Fill_CurZBufMin = curZ;
  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TrameZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

// Verified
S32 Filler_FlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 curZ = Fill_CurZBufMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYFLAT_PtrZBufferPatch = PtrZBuffer;
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
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U32 zDec = curZ << 24; // esi
      U32 zDecSlope = Fill_ZBuf_XSlope << 24; // ebp
      U32 zIntSlope = Fill_ZBuf_XSlope >> 8; // ebx
      U32 offset = screen - (U8 *)Log; // edi
      U32 zInt = curZ >> 8; // edx
      do
      {
        if (POLYFLAT_PtrZBufferPatch[offset] >= (zInt & 0xFFFF))
        {
          POLYFLAT_PtrLog[offset] = Fill_Color.Num & 0xFF;
        }

        ++offset;
        U8 carry = (U64)zDec + (U64)zDecSlope > 0xFFFFFFFF;
        zDec += zDecSlope;
        zInt += zIntSlope + carry;
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
    curZ += Fill_ZBuf_LeftSlope;
  }

  Fill_CurZBufMin = curZ;
  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TransparentNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TrameNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

// Verified
S32 Filler_FlagZBuffer(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 curZ = Fill_CurZBufMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYFLAT_PtrZBufferPatch = PtrZBuffer;
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
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U32 zDec = curZ << 24; // esi
      U32 zDecSlope = Fill_ZBuf_XSlope << 24; // ebp
      U32 zIntSlope = Fill_ZBuf_XSlope >> 8; // ebx
      U32 offset = screen - (U8 *)Log; // edi
      U32 zInt = curZ >> 8; // edx
      do
      {
        if (POLYFLAT_PtrZBufferPatch[offset] >= (zInt & 0xFFFF))
        {
          IsPolygonHidden = 0;
        }

        ++offset;
        U8 carry = (U64)zDec + (U64)zDecSlope > 0xFFFFFFFF;
        zDec += zDecSlope;
        zInt += zIntSlope + carry;
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
    curZ += Fill_ZBuf_LeftSlope;
  }

  Fill_CurZBufMin = curZ;
  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;

  return Triangle_ReadNextEdge(NULL, NULL);
}
