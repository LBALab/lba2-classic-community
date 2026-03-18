#include <POLYGON/POLYFLAT.H>

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>

#include <stdio.h>
#include <SYSTEM/DEBUG_TRACES.H>

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
    LIB386_TRACE_CPP("Filler_Flat", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_Flat", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_Flat", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_Flat", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_Flat", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_Flat", "1", "entry");
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

S32 Filler_Trame(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_Trame", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_Trame", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_Trame", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_Trame", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_Trame", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_Trame", "1", "entry");
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 parity = Fill_Trame_Parity;
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
      // Trame (dithering) pattern implementation
      diffX >>= 1; // shr ecx,1
      if (diffX == 0)
        goto EndScanLine;

      parity ^= 1; // xor esi,1 - toggle parity each scanline
      
      // Check alignment with current parity
      U32 alignment = ((uintptr_t)screen) & 1; // and eax,1
      diffX++; // inc ecx
      alignment ^= parity; // xor eax,esi

      U8 color = Fill_Color.Num & 0xFF;
      
      if (alignment != 0) // je @@Pair
      {
        screen++; // inc edi - skip first pixel if not aligned with parity
      }

      // @@Pair:
      {
        U32 carry = diffX & 1;
        diffX >>= 1; // shr ecx,1
        if (carry == 0) // jnc @@Odd
        {
          // @@Odd: write first pixel
          *screen = color;
          screen += 2;
          diffX--;
        }
        // @@Each_X:
        while (diffX > 0)
        {
          *screen = color; // mov [edi],al
          screen += 2; // add edi,2
          // @@Odd:
          *screen = color; // mov [edi],al
          screen += 2; // add edi,2
          diffX--; // dec ecx
        }
      }
    }

EndScanLine:
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
  Fill_Trame_Parity = parity ^ 1; // xor esi,1 - final parity toggle

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_Transparent(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_Transparent", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_Transparent", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_Transparent", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_Transparent", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_Transparent", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_Transparent", "1", "entry");
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
      U8 fillColorByte = Fill_Color.Num & 0xFF;
      
      // @@LoopX:
      do
      {
        U8 existingPixel = *screen;
        U8 blendedPixel = (existingPixel & 0x0F) | fillColorByte;
        *screen = blendedPixel;
        screen++;
        --diffX;
      } while (diffX);
    }

    // @@EndScanLine:
    --count;
    if (count == 0)
    {
      break;
    }

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
    LIB386_TRACE_CPP("Filler_FlatZBuf", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_FlatZBuf", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_FlatZBuf", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_FlatZBuf", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_FlatZBuf", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_FlatZBuf", "1", "entry");
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
      U32 zIntSlope = (U32)Fill_ZBuf_XSlope >> 8; // ebx
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
    LIB386_TRACE_CPP("Filler_TransparentZBuf", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TransparentZBuf", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TransparentZBuf", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TransparentZBuf", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TransparentZBuf", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TransparentZBuf", "1", "entry");
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
      U32 zIntSlope = (U32)Fill_ZBuf_XSlope >> 8; // ebx
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
    LIB386_TRACE_CPP("Filler_TrameZBuf", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TrameZBuf", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TrameZBuf", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TrameZBuf", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TrameZBuf", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TrameZBuf", "1", "entry");
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 parity = Fill_Trame_Parity;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYFLAT_PtrZBufferPatch = PtrZBuffer;
    POLYFLAT_PtrZBuffer1 = PtrZBuffer;
    POLYFLAT_PtrZBuffer2 = PtrZBuffer;
    POLYFLAT_PtrZBuffer3 = PtrZBuffer;
    POLYFLAT_PtrLog = (U8 *)Log;
    POLYFLAT_PtrLog2 = (U8 *)Log;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    // @@EnterLoop:
    U32 startX = fillCurXMin >> 16;
    U32 endX = fillCurXMax >> 16;
    U32 diffX = endX - startX;
    
    if (endX > startX)
    {
      U8 *screen = startX + curOffLine;

      diffX >>= 1; // shr ecx,1
      if (diffX == 0)
        goto EndScanLine;

      parity ^= 1; // xor esi,1 - toggle parity each scanline
      
      // Check alignment with current parity
      U32 alignment = ((uintptr_t)screen) & 1; // and eax,1
      diffX++; // inc ecx
      alignment ^= parity; // xor eax,esi

      if (alignment != 0) // je @@Pair
      {
        screen++; // inc edi - skip first pixel if not aligned with parity
      }

      // @@Pair: Z-buffer calculations
      U32 zDec = Fill_CurZBufMin << 24; // shl esi,32-8
      U32 zDecSlope = Fill_ZBuf_XSlope << 1; // add ebx,ebx (double for pairs)
      U32 zDecSlopeStep = zDecSlope << 24; // shl ebp,32-8
      U32 zInt = Fill_CurZBufMin >> 8; // shr edx,8
      U32 zIntSlope = zDecSlope >> 8; // shr ebx,8
      U32 offset = screen - (U8 *)Log; // sub edi,eax
      U8 color = Fill_Color.Num & 0xFF;

      {
        U32 carry = diffX & 1;
        diffX >>= 1; // shr ecx,1
        if (carry == 0) // jnc @@Odd
        {
          // @@Odd: Test and write first pixel
          if (POLYFLAT_PtrZBuffer2[offset] >= (zInt & 0xFFFF))
          {
            POLYFLAT_PtrZBuffer3[offset] = zInt & 0xFFFF;
            POLYFLAT_PtrLog2[offset] = color;
          }
          offset += 2;
          diffX--;
        }
        // @@Each_X:
        while (diffX > 0)
        {
          // Test and write pixel (Each_X)
          if (POLYFLAT_PtrZBufferPatch[offset] >= (zInt & 0xFFFF))
          {
            POLYFLAT_PtrZBuffer1[offset] = zInt & 0xFFFF;
            POLYFLAT_PtrLog[offset] = color;
          }
          offset += 2;
          U8 zCarry = (U64)zDec + (U64)zDecSlopeStep > 0xFFFFFFFF;
          zDec += zDecSlopeStep;
          zInt += zIntSlope + zCarry;

          // @@Odd: Test and write pixel
          if (POLYFLAT_PtrZBuffer2[offset] >= (zInt & 0xFFFF))
          {
            POLYFLAT_PtrZBuffer3[offset] = zInt & 0xFFFF;
            POLYFLAT_PtrLog2[offset] = color;
          }
          offset += 2;
          diffX--;
        }
      }
    }

EndScanLine:
    // @@EndScanLine:
    if (!--count)
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_Trame_Parity = parity ^ 1; // Final parity toggle

  return Triangle_ReadNextEdge(NULL, NULL);
}

// Verified
S32 Filler_FlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_FlatNZW", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_FlatNZW", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_FlatNZW", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_FlatNZW", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_FlatNZW", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_FlatNZW", "1", "entry");
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
      U32 zIntSlope = (U32)Fill_ZBuf_XSlope >> 8; // ebx
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
    LIB386_TRACE_CPP("Filler_TransparentNZW", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TransparentNZW", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TransparentNZW", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TransparentNZW", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TransparentNZW", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TransparentNZW", "1", "entry");
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
      U32 zIntSlope = (U32)Fill_ZBuf_XSlope >> 8; // ebx
      U32 offset = screen - (U8 *)Log; // edi
      U32 zInt = curZ >> 8; // edx
      U8 color = Fill_Color.Num & 0xFF;

      do
      {
        if (POLYFLAT_PtrZBufferPatch[offset] >= (zInt & 0xFFFF))
        {
          // No Z-buffer write for NZW variant
          U8 logColor = POLYFLAT_PtrLog[offset] & 0x0F;
          POLYFLAT_PtrLog[offset] = color | logColor;
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

S32 Filler_TrameNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_TrameNZW", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TrameNZW", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TrameNZW", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TrameNZW", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TrameNZW", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TrameNZW", "1", "entry");
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  Fill_CurY += count;
  U32 parity = Fill_Trame_Parity;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYFLAT_PtrZBufferPatch = PtrZBuffer;
    POLYFLAT_PtrZBuffer2 = PtrZBuffer;
    POLYFLAT_PtrLog = (U8 *)Log;
    POLYFLAT_PtrLog2 = (U8 *)Log;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    // @@EnterLoop:
    U32 startX = fillCurXMin >> 16;
    U32 endX = fillCurXMax >> 16;
    U32 diffX = endX - startX;
    
    if (endX > startX)
    {
      U8 *screen = startX + curOffLine;

      diffX >>= 1; // shr ecx,1
      if (diffX == 0)
        goto EndScanLine;

      parity ^= 1; // xor esi,1 - toggle parity each scanline
      
      // Check alignment with current parity
      U32 alignment = ((uintptr_t)screen) & 1; // and eax,1
      diffX++; // inc ecx
      alignment ^= parity; // xor eax,esi

      if (alignment != 0) // je @@Pair
      {
        screen++; // inc edi - skip first pixel if not aligned with parity
      }

      // @@Pair: Z-buffer calculations
      U32 zDec = Fill_CurZBufMin << 24; // shl esi,32-8
      U32 zDecSlope = Fill_ZBuf_XSlope << 1; // add ebx,ebx (double for pairs)
      U32 zDecSlopeStep = zDecSlope << 24; // shl ebp,32-8
      U32 zInt = Fill_CurZBufMin >> 8; // shr edx,8
      U32 zIntSlope = zDecSlope >> 8; // shr ebx,8
      U32 offset = screen - (U8 *)Log; // sub edi,eax
      U8 color = Fill_Color.Num & 0xFF;

      {
        U32 carry = diffX & 1;
        diffX >>= 1; // shr ecx,1
        if (carry == 0) // jnc @@Odd
        {
          // @@Odd: Test and write first pixel (NZW: no Z write)
          if (POLYFLAT_PtrZBuffer2[offset] >= (zInt & 0xFFFF))
          {
            POLYFLAT_PtrLog2[offset] = color;
          }
          offset += 2;
          diffX--;
        }
        // @@Each_X:
        while (diffX > 0)
        {
          // Test and write pixel (Each_X, NZW: no Z write)
          if (POLYFLAT_PtrZBufferPatch[offset] >= (zInt & 0xFFFF))
          {
            POLYFLAT_PtrLog[offset] = color;
          }
          offset += 2;
          U8 zCarry = (U64)zDec + (U64)zDecSlopeStep > 0xFFFFFFFF;
          zDec += zDecSlopeStep;
          zInt += zIntSlope + zCarry;

          // @@Odd: Test and write pixel (NZW: no Z write)
          if (POLYFLAT_PtrZBuffer2[offset] >= (zInt & 0xFFFF))
          {
            POLYFLAT_PtrLog2[offset] = color;
          }
          offset += 2;
          diffX--;
        }
      }
    }

EndScanLine:
    // @@EndScanLine:
    if (!--count)
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_Trame_Parity = parity ^ 1; // Final parity toggle

  return Triangle_ReadNextEdge(NULL, NULL);
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
      U32 zIntSlope = (U32)Fill_ZBuf_XSlope >> 8; // ebx
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
