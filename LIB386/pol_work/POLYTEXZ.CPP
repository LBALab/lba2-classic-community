/*
; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                       (Adeline Software Intl.)                     ***
; ***                           1995-96-97                               ***
; ***                                                                    ***
; **************************************************************************
; **************************************************************************
; **************************************************************************
*/

#include <POLYGON/POLYTEXZ.H>

#include <stdio.h>
#include <stack>

#include <FILLER.H>
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/UTILS.H>

PTR_U16 POLYTEXZ_PtrZBuffer1 = NULL;
PTR_U16 POLYTEXZ_PtrZBuffer2 = NULL;
PTR_U16 POLYTEXZ_PtrZBufferPatch = NULL;
PTR_U8 POLYTEXZ_PtrMapPatch = NULL;
PTR_U8 POLYTEXZ_PtrCLUT = NULL;
PTR_U8 POLYTEXZ_PtrLog = NULL;
U32 POLYTEXZ_RepMaskPatch = 0x12345678;
U32 POLYTEXZ_RepMask2 = 0x12345678;
U32 POLYTEXZ_DecZBuf = 0x12345678;
U32 POLYTEXZ_DecZBuf2 = 0x12345678;
U32 POLYTEXZ_IntZBuf = 0x12345678;
U32 POLYTEXZ_IntZBuf2 = 0x12345678;

std::stack<S32> PerspectiveWStack = std::stack<S32>();
double PerspectiveInvW = 0.0;

// ╔═════════════════════════════════════════════════════════════╗
// ║ Initialize the gradiants of a scanline                      ║
// ╚═════════════════════════════════════════════════════════════╝
void Fill_Init_Perspective(S32 lineU, S32 lineV, U32 fillWidth)
{
  S32 w = Fill_Cur_W;

  if (w == 0 || w == -1 || w == 1)
  {
    // @@W_Nul1:
    w = 2;
    Fill_Cur_W = 2;
  }

  // @@Return_Error1:
  Fill_Cur_MapUOverW = lineU;
  Fill_Cur_MapVOverW = lineV;
  S32 wXSlope = Fill_W_XSlope << PERSP_SHIFT;
  PerspectiveInvW = 256.0 / (double)w;

  // Here, we precalc an array in the stack. This array
  // contains valid Cur_W values
  PerspectiveWStack = std::stack<S32>();
  U32 stackLength = fillWidth + PERSP_VALUE - 1;
  stackLength >>= PERSP_SHIFT;

  do
  {
    // @@CreateStack:
    w += wXSlope;
    if (w == 0 || w == -1 || w == 1)
    {
      // @@W_Nul:
      w = 2;
    }

    // @@Return_Error:
    PerspectiveWStack.push(w);
  } while (PerspectiveWStack.size() < stackLength);

  Fill_Cur_W = PerspectiveWStack.top();
  PerspectiveWStack.pop();

  Fill_Cur_MapU = ROUND((double)Fill_Cur_MapUOverW * PerspectiveInvW);
  Fill_Cur_MapV = ROUND((double)Fill_Cur_MapVOverW * PerspectiveInvW);
  PerspectiveInvW = 256.0 / (double)Fill_Cur_W; // Will be used in Fill_Loop_Perspective
}

// ╔═════════════════════════════════════════════════════════════╗
// ║ Initialize the gradiants of a section of PERSP_VALUE pixels ║
// ╚═════════════════════════════════════════════════════════════╝
void Fill_Loop_Perspective(U32 fillWidth)
{
  Fill_Next_MapUOverW = (Fill_MapU_XSlope << PERSP_SHIFT) + Fill_Cur_MapUOverW;
  Fill_Next_MapVOverW = (Fill_MapV_XSlope << PERSP_SHIFT) + Fill_Cur_MapVOverW;

  Fill_Next_MapU = ROUND((double)Fill_Next_MapUOverW * PerspectiveInvW);
  Fill_Next_MapV = ROUND((double)Fill_Next_MapVOverW * PerspectiveInvW);

  if (fillWidth > PERSP_VALUE)
  {
    Fill_Next_W = PerspectiveWStack.top();
    PerspectiveWStack.pop();
    PerspectiveInvW = 256.0 / (double)Fill_Next_W;
  }
}

S32 Filler_TextureZ(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZFlat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZFlatChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZChromaKeyFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

// Verified against ASM (not properly tested)
S32 Filler_TextureZFlatZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrCLUT = Fill_Color.Ptr;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_RepMaskPatch = RepMask;
    POLYTEXZ_RepMask2 = RepMask;
    POLYTEXZ_PtrZBufferPatch = PtrZBuffer;
    POLYTEXZ_PtrZBuffer1 = PtrZBuffer;
    POLYTEXZ_PtrZBuffer2 = PtrZBuffer;
    POLYTEXZ_IntZBuf = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_IntZBuf2 = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_DecZBuf = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXZ_DecZBuf2 = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
    Fill_CurZBuf = Fill_CurZBufMin;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
    Fill_CurZBuf = Fill_CurZBufMin;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      Fill_Cur_W = Fill_CurWMin;

      S32 lineU = u;
      S32 lineV = v;

      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        U64 addW = ((U64)factor * (U64)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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
      // @@FastInit:
      U32 offset = line - (U8 *)Log; // edi
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        // @@FastFill:
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF); // ebp
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF); // ebx

        U32 mapIndex = mapV_mapU >> 16; // eax
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 decZ = Fill_CurZBuf << 24; // esi
        U32 intZ = Fill_CurZBuf >> 8; // edx
        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          // @@LoopX:
          if (POLYTEXZ_PtrZBufferPatch[offset] < (intZ & 0xFFFF))
          {
            while (1)
            {
              // @@NoDisp:
              mapIndex = mapV_mapU + incV_incU;
              mapV_mapU += incV_incU;

              mapIndex >>= 16;
              offset++;

              mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

              U8 carry = ((U64)decZ + (U64)POLYTEXZ_DecZBuf2) > 0xFFFFFFFF;
              decZ += POLYTEXZ_DecZBuf2;
              intZ += POLYTEXZ_IntZBuf2 + carry;

              mapIndex &= POLYTEXZ_RepMask2;

              fillWidth--;

              if (fillWidth == 0)
              {
                break;
              }

              if (POLYTEXZ_PtrZBuffer2[offset] >= (intZ & 0xFFFF))
              {
                break;
              }
            }

            if (fillWidth == 0)
            {
              break;
            }
          }

          // @@Disp:
          mapIndex = (mapIndex & 0xFFFFFF00) | POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;
          mapIndex &= 0xFF;

          POLYTEXZ_PtrZBuffer1[offset] = intZ & 0xFFFF;
          mapIndex = (mapIndex & 0xFFFFFF00) | POLYTEXZ_PtrCLUT[mapIndex];
          POLYTEXZ_PtrLog[offset] = mapIndex & 0xFF;

          mapIndex = mapV_mapU >> 16;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          offset++;

          U8 carry = ((U64)decZ + (U64)POLYTEXZ_DecZBuf) > 0xFFFFFFFF;
          decZ += POLYTEXZ_DecZBuf;
          intZ += POLYTEXZ_IntZBuf + carry;

          mapIndex &= POLYTEXZ_RepMaskPatch;

          fillWidth--;
        }
        while (fillWidth);

        // @@EndPERSP_VALUE:

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        Fill_Cur_MapU = Fill_Next_MapU;
        Fill_Cur_MapV = Fill_Next_MapV;
        Fill_CurZBuf = (intZ << 8) | (decZ >> 24);
        Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
        Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
        Fill_Cur_W = Fill_Next_W;

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        fillWidth = lineWidth - PERSP_VALUE;
      }
      // @@End_X:
    }
    // @@EndScanLine:
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
    Fill_CurWMin += Fill_W_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
    Fill_CurZBuf = Fill_CurZBufMin;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZFlatChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrCLUT = Fill_Color.Ptr;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_RepMaskPatch = RepMask;
    POLYTEXZ_PtrZBufferPatch = PtrZBuffer;
    POLYTEXZ_PtrZBuffer1 = PtrZBuffer;
    POLYTEXZ_IntZBuf = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_DecZBuf = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
    Fill_CurZBuf = Fill_CurZBufMin;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
    Fill_CurZBuf = Fill_CurZBufMin;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      Fill_Cur_W = Fill_CurWMin;

      S32 lineU = u;
      S32 lineV = v;

      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        U64 addW = ((U64)factor * (U64)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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
      // @@FastInit:
      U32 offset = line - (U8 *)Log; // edi
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        // @@FastFill:
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF); // ebp
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF); // ebx

        U32 mapIndex = mapV_mapU >> 16; // eax
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 decZ = Fill_CurZBuf << 24; // esi
        U32 intZ = Fill_CurZBuf >> 8; // edx
        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          // @@LoopX:
          mapIndex = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;

          if (mapIndex != 0 && POLYTEXZ_PtrZBufferPatch[offset] >= (intZ & 0xFFFF))
          {
            POLYTEXZ_PtrZBuffer1[offset] = intZ & 0xFFFF;
            mapIndex = (mapIndex & 0xFFFFFF00) | POLYTEXZ_PtrCLUT[mapIndex];
            POLYTEXZ_PtrLog[offset] = mapIndex & 0xFF;
          }

          // @@NoDisp:
          mapIndex = mapV_mapU;

          mapIndex >>= 16;
          offset++;

          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          U8 carry = ((U64)decZ + (U64)POLYTEXZ_DecZBuf) > 0xFFFFFFFF;
          decZ += POLYTEXZ_DecZBuf;
          intZ += POLYTEXZ_IntZBuf + carry;

          mapIndex &= POLYTEXZ_RepMaskPatch;

          fillWidth--;
        }
        while (fillWidth);

        // @@EndPERSP_VALUE:

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        Fill_Cur_MapU = Fill_Next_MapU;
        Fill_Cur_MapV = Fill_Next_MapV;
        Fill_CurZBuf = (intZ << 8) | (decZ >> 24);
        Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
        Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
        Fill_Cur_W = Fill_Next_W;

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        fillWidth = lineWidth - PERSP_VALUE;
      }
      // @@End_X:
    }
    // @@EndScanLine:
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
    Fill_CurWMin += Fill_W_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
    Fill_CurZBuf = Fill_CurZBufMin;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZChromaKeyFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZFlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrCLUT = Fill_Color.Ptr;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_RepMaskPatch = RepMask;
    POLYTEXZ_RepMask2 = RepMask;
    POLYTEXZ_PtrZBufferPatch = PtrZBuffer;
    POLYTEXZ_PtrZBuffer1 = PtrZBuffer;
    POLYTEXZ_PtrZBuffer2 = PtrZBuffer;
    POLYTEXZ_IntZBuf = Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_IntZBuf2 = Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_DecZBuf = Fill_ZBuf_XSlope << 24;
    POLYTEXZ_DecZBuf2 = Fill_ZBuf_XSlope << 24;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
    Fill_CurZBuf = Fill_CurZBufMin;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
    Fill_CurZBuf = Fill_CurZBufMin;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      Fill_Cur_W = Fill_CurWMin;

      U32 lineU = u;
      U32 lineV = v;

      // Load 1st dst cache line
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        U64 addW = ((U64)factor * (U64)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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
      // @@FastInit:
      U32 offset = line - (U8 *)Log; // edi
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        // @@FastFill:
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF); // ebp
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF); // ebx

        U32 mapIndex = mapV_mapU >> 16; // eax
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 decZ = Fill_CurZBuf << 24; // esi
        U32 intZ = Fill_CurZBuf >> 8; // edx
        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          // @@LoopX:
          if (POLYTEXZ_PtrZBufferPatch[offset] < (intZ & 0xFFFF))
          {
            while (1)
            {
              // @@NoDisp:
              mapIndex = mapV_mapU + incV_incU;
              mapV_mapU += incV_incU;

              mapIndex >>= 16;
              offset++;

              mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

              U8 carry = ((U64)decZ + (U64)POLYTEXZ_DecZBuf2) > 0xFFFFFFFF;
              decZ += POLYTEXZ_DecZBuf2;
              intZ += POLYTEXZ_IntZBuf2 + carry;

              mapIndex &= POLYTEXZ_RepMask2;

              fillWidth--;

              if (fillWidth == 0)
              {
                break;
              }

              if (POLYTEXZ_PtrZBuffer2[offset] >= (intZ & 0xFFFF))
              {
                break;
              }
            }

            if (fillWidth == 0)
            {
              break;
            }
          }

          // @@Disp:
          mapIndex = (mapIndex & 0xFFFFFF00) | POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU + incV_incU;
          mapIndex &= 0xFF;

          mapIndex = (mapIndex & 0xFFFFFF00) | POLYTEXZ_PtrCLUT[mapIndex];
          POLYTEXZ_PtrLog[offset] = mapIndex & 0xFF;

          mapIndex = mapV_mapU >> 16;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          offset++;

          U8 carry = ((U64)decZ + (U64)POLYTEXZ_DecZBuf) > 0xFFFFFFFF;
          decZ += POLYTEXZ_DecZBuf;
          intZ += POLYTEXZ_IntZBuf + carry;

          mapIndex &= POLYTEXZ_RepMaskPatch;

          fillWidth--;
        }
        while (fillWidth);

        // @@EndPERSP_VALUE:

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        Fill_Cur_MapU = Fill_Next_MapU;
        Fill_Cur_MapV = Fill_Next_MapV;
        Fill_CurZBuf = (intZ << 8) | (decZ >> 24);
        Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
        Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
        Fill_Cur_W = Fill_Next_W;

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        fillWidth = lineWidth - PERSP_VALUE;
      }
      // @@End_X:
    }
    // @@EndScanLine:
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
    Fill_CurWMin += Fill_W_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
    Fill_CurZBuf = Fill_CurZBufMin;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZFlatChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 Filler_TextureZChromaKeyFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

