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
#include <math.h>

#include <FILLER.H>
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/UTILS.H>
#include <SYSTEM/DEBUG_TRACES.H>

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

// FIFO queue for perspective W values — the ASM fills the hardware stack
// ascending then pops via `pop` (FIFO: first stored = first consumed).
#define MAX_PERSPECTIVE_STACK_SIZE 128
typedef struct {
    S32 data[MAX_PERSPECTIVE_STACK_SIZE];
    U32 write_index;
    U32 read_index;
} PerspectiveWStack_t;

static PerspectiveWStack_t PerspectiveWStack = {{0}, 0, 0};
/* Perspective correction: replicate ASM x87 FPU behavior.
 *
 * The ASM keeps 256.0f/W on the FPU stack in 80-bit extended precision,
 * then multiplies each MapOverW value by it and stores via fistp (which
 * uses FPU rounding = round-to-nearest-even).
 *
 * In C we use volatile long double to force 80-bit stores, and lrintl()
 * to match fistp rounding.
 *
 * The ASM loads F_256 as a 32-bit float (DWORD), so we must use float
 * for the constant to ensure the same representation.
 */

static volatile long double PerspectiveInvW;

/* Queue operations — FIFO to match ASM hardware stack behavior */
static void PerspectiveWStack_clear(void) {
    PerspectiveWStack.write_index = 0;
    PerspectiveWStack.read_index = 0;
}

static void PerspectiveWStack_push(S32 value) {
    if (PerspectiveWStack.write_index < MAX_PERSPECTIVE_STACK_SIZE) {
        PerspectiveWStack.data[PerspectiveWStack.write_index] = value;
        PerspectiveWStack.write_index++;
    }
}

static S32 PerspectiveWStack_top(void) {
    if (PerspectiveWStack.read_index < PerspectiveWStack.write_index) {
        return PerspectiveWStack.data[PerspectiveWStack.read_index];
    }
    return 0;
}

static void PerspectiveWStack_pop(void) {
    if (PerspectiveWStack.read_index < PerspectiveWStack.write_index) {
        PerspectiveWStack.read_index++;
    }
}

static U32 PerspectiveWStack_size(void) {
    return PerspectiveWStack.write_index - PerspectiveWStack.read_index;
}

static S32 persp_eval(S32 val)
{
    volatile long double product = (long double)val * PerspectiveInvW;
    return (S32)lrintl(product);
}

// ╔═════════════════════════════════════════════════════════════╗
// ║ Initialize the gradiants of a scanline                      ║
// ╚═════════════════════════════════════════════════════════════╝
static void Fill_Init_Perspective_Impl(S32 lineU, S32 lineV, U32 fillWidth)
{
  S32 w = Fill_Cur_W;

  if (w == 0 || w == -1 || w == 1)
  {
    w = 2;
    Fill_Cur_W = 2;
  }

  Fill_Cur_MapUOverW = lineU;
  Fill_Cur_MapVOverW = lineV;
  S32 wXSlope = Fill_W_XSlope << PERSP_SHIFT;
  PerspectiveInvW = (long double)256.0f / (long double)w;

  PerspectiveWStack_clear();
  U32 stackLength = fillWidth + PERSP_VALUE - 1;
  stackLength >>= PERSP_SHIFT;

  do
  {
    w += wXSlope;
    if (w == 0 || w == -1 || w == 1)
    {
      w = 2;
    }

    PerspectiveWStack_push(w);
  } while (PerspectiveWStack_size() < stackLength);

  Fill_Cur_W = PerspectiveWStack_top();
  PerspectiveWStack_pop();

  Fill_Cur_MapU = persp_eval(Fill_Cur_MapUOverW);
  Fill_Cur_MapV = persp_eval(Fill_Cur_MapVOverW);
  PerspectiveInvW = (long double)256.0f / (long double)Fill_Cur_W;
}

// ╔═════════════════════════════════════════════════════════════╗
// ║ Initialize the gradiants of a section of PERSP_VALUE pixels ║
// ╚═════════════════════════════════════════════════════════════╝
static void Fill_Loop_Perspective_Impl(U32 fillWidth)
{
  Fill_Next_MapUOverW = (Fill_MapU_XSlope << PERSP_SHIFT) + Fill_Cur_MapUOverW;
  Fill_Next_MapVOverW = (Fill_MapV_XSlope << PERSP_SHIFT) + Fill_Cur_MapVOverW;

  Fill_Next_MapU = persp_eval(Fill_Next_MapUOverW);
  Fill_Next_MapV = persp_eval(Fill_Next_MapVOverW);

  if (fillWidth > PERSP_VALUE)
  {
    Fill_Next_W = PerspectiveWStack_top();
    PerspectiveWStack_pop();
    PerspectiveInvW = (long double)256.0f / (long double)Fill_Next_W;
  }
}

/* Global function pointers matching the ASM convention:
 * The ASM files (POLYTZG.ASM etc.) reference these as Extrn DWORD and
 * call them via `call [Fill_Init_Perspective]`.  By defining them as
 * global function pointers (instead of plain functions), both CPP
 * callers and ASM callers work correctly. */
void (*Fill_Init_Perspective)(S32, S32, U32) = Fill_Init_Perspective_Impl;
void (*Fill_Loop_Perspective)(U32) = Fill_Loop_Perspective_Impl;

S32 Filler_TextureZ(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_TextureZ", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TextureZ", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureZ", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureZ", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureZ", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureZ", "1", "entry");
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
    POLYTEXZ_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          mapIndex = (mapIndex & 0xFFFFFF00) | POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;
          mapIndex &= 0xFF;

          mapIndex = (mapIndex & 0xFFFFFF00) | POLYTEXZ_PtrCLUT[mapIndex];
          POLYTEXZ_PtrLog[offset] = mapIndex & 0xFF;

          mapIndex = mapV_mapU >> 16;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          offset++;
          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        Fill_Cur_MapU = Fill_Next_MapU;
        Fill_Cur_MapV = Fill_Next_MapV;
        Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
        Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
        Fill_Cur_W = Fill_Next_W;

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        fillWidth = lineWidth - PERSP_VALUE;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZFlat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_TextureZFlat", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TextureZFlat", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureZFlat", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureZFlat", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureZFlat", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureZFlat", "1", "entry");
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrCLUT = Fill_Color.Ptr;
    POLYTEXZ_RepMaskPatch = RepMask;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          mapIndex = (mapIndex & 0xFFFFFF00) | POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;
          mapIndex &= 0xFF;

          mapIndex = (mapIndex & 0xFFFFFF00) | POLYTEXZ_PtrCLUT[mapIndex];
          POLYTEXZ_PtrLog[offset] = mapIndex & 0xFF;

          mapIndex = mapV_mapU >> 16;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          offset++;
          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        Fill_Cur_MapU = Fill_Next_MapU;
        Fill_Cur_MapV = Fill_Next_MapV;
        Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
        Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
        Fill_Cur_W = Fill_Next_W;

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        fillWidth = lineWidth - PERSP_VALUE;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_TextureZChromaKey", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TextureZChromaKey", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureZChromaKey", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureZChromaKey", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureZChromaKey", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureZChromaKey", "1", "entry");
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_RepMaskPatch = RepMask;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          U8 texel = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;

          if (texel != 0)
          {
            POLYTEXZ_PtrLog[offset] = texel;
          }

          mapIndex = mapV_mapU >> 16;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          offset++;
          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        Fill_Cur_MapU = Fill_Next_MapU;
        Fill_Cur_MapV = Fill_Next_MapV;
        Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
        Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
        Fill_Cur_W = Fill_Next_W;

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        fillWidth = lineWidth - PERSP_VALUE;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZFlatChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
    LIB386_TRACE_CPP("Filler_TextureZFlatChromaKey", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, fillCurXMin, fillCurXMax);
    LIB386_TRACE_CPP("Filler_TextureZFlatChromaKey", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("Filler_TextureZFlatChromaKey", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("Filler_TextureZFlatChromaKey", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("Filler_TextureZFlatChromaKey", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);
    LIB386_TRACE_CPP("Filler_TextureZFlatChromaKey", "1", "entry");
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrCLUT = Fill_Color.Ptr;
    POLYTEXZ_RepMaskPatch = RepMask;
    POLYTEXZ_RepMask2 = RepMask;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          U8 texel = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;

          if (texel != 0)
          {
            U8 color = POLYTEXZ_PtrCLUT[texel];
            POLYTEXZ_PtrLog[offset] = color;
          }

          mapIndex = mapV_mapU >> 16;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          offset++;
          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        Fill_Cur_MapU = Fill_Next_MapU;
        Fill_Cur_MapV = Fill_Next_MapV;
        Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
        Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
        Fill_Cur_W = Fill_Next_W;

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        fillWidth = lineWidth - PERSP_VALUE;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_RepMaskPatch = RepMask;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          mapIndex = (mapIndex & 0xFFFFFF00) | POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;
          mapIndex &= 0xFF;

          mapIndex = (mapIndex & 0xFFFFFF00) | Fill_Logical_Palette[mapIndex];
          POLYTEXZ_PtrLog[offset] = mapIndex & 0xFF;

          mapIndex = mapV_mapU >> 16;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          offset++;
          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        Fill_Cur_MapU = Fill_Next_MapU;
        Fill_Cur_MapV = Fill_Next_MapV;
        Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
        Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
        Fill_Cur_W = Fill_Next_W;

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        fillWidth = lineWidth - PERSP_VALUE;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZChromaKeyFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_RepMaskPatch = RepMask;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          U8 texel = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;

          if (texel != 0)
          {
            U8 fogColor = Fill_Logical_Palette[texel];
            POLYTEXZ_PtrLog[offset] = fogColor;
          }

          mapIndex = mapV_mapU >> 16;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          offset++;
          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        Fill_Cur_MapU = Fill_Next_MapU;
        Fill_Cur_MapV = Fill_Next_MapV;
        Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
        Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
        Fill_Cur_W = Fill_Next_W;

        if (lineWidth <= PERSP_VALUE)
        {
          break;
        }

        fillWidth = lineWidth - PERSP_VALUE;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrZBufferPatch = PtrZBuffer;
    POLYTEXZ_PtrZBuffer1 = PtrZBuffer;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_IntZBuf = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_DecZBuf = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXZ_RepMaskPatch = RepMask;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 decZ = Fill_CurZBuf << 24;
        U32 intZ = Fill_CurZBuf >> 8;
        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          // @@LoopX:
          if (POLYTEXZ_PtrZBufferPatch[offset] >= (intZ & 0xFFFF))
          {
            POLYTEXZ_PtrZBuffer1[offset] = intZ & 0xFFFF;
            U8 texel = POLYTEXZ_PtrMapPatch[mapIndex];
            POLYTEXZ_PtrLog[offset] = texel;
          }

          // @@NoDisp:
          mapV_mapU += incV_incU;
          mapIndex = mapV_mapU >> 16;
          offset++;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          U8 carry = ((U64)decZ + (U64)POLYTEXZ_DecZBuf) > 0xFFFFFFFF;
          decZ += POLYTEXZ_DecZBuf;
          intZ += POLYTEXZ_IntZBuf + carry;

          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

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

S32 Filler_TextureZChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrZBufferPatch = PtrZBuffer;
    POLYTEXZ_PtrZBuffer1 = PtrZBuffer;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_IntZBuf = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_DecZBuf = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXZ_RepMaskPatch = RepMask;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 decZ = Fill_CurZBuf << 24;
        U32 intZ = Fill_CurZBuf >> 8;
        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          // @@LoopX:
          U8 textureColor = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;

          if (textureColor != 0 && POLYTEXZ_PtrZBufferPatch[offset] >= (intZ & 0xFFFF))
          {
            POLYTEXZ_PtrZBuffer1[offset] = intZ & 0xFFFF;
            POLYTEXZ_PtrLog[offset] = textureColor;
          }

          // @@NoDisp:
          mapIndex = mapV_mapU >> 16;
          offset++;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          U8 carry = ((U64)decZ + (U64)POLYTEXZ_DecZBuf) > 0xFFFFFFFF;
          decZ += POLYTEXZ_DecZBuf;
          intZ += POLYTEXZ_IntZBuf + carry;

          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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
          U8 textureColor = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;
          textureColor &= 0xFF;

          POLYTEXZ_PtrZBuffer1[offset] = intZ & 0xFFFF;
          U8 foggedColor = Fill_Logical_Palette[textureColor];
          POLYTEXZ_PtrLog[offset] = foggedColor;

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

S32 Filler_TextureZChromaKeyFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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
          U8 textureColor = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;

          // Chroma key test - skip transparent pixels (color 0)
          if (textureColor != 0 && POLYTEXZ_PtrZBufferPatch[offset] >= (intZ & 0xFFFF))
          {
            POLYTEXZ_PtrZBuffer1[offset] = intZ & 0xFFFF;
            U8 foggedColor = Fill_Logical_Palette[textureColor];
            POLYTEXZ_PtrLog[offset] = foggedColor;
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

S32 Filler_TextureZNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrZBufferPatch = PtrZBuffer;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_IntZBuf = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_DecZBuf = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXZ_RepMaskPatch = RepMask;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 decZ = Fill_CurZBuf << 24;
        U32 intZ = Fill_CurZBuf >> 8;
        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          // @@LoopX:
          if (POLYTEXZ_PtrZBufferPatch[offset] >= (intZ & 0xFFFF))
          {
            U8 texel = POLYTEXZ_PtrMapPatch[mapIndex];
            POLYTEXZ_PtrLog[offset] = texel;
          }

          // @@NoDisp:
          mapV_mapU += incV_incU;
          mapIndex = mapV_mapU >> 16;
          offset++;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          U8 carry = ((U64)decZ + (U64)POLYTEXZ_DecZBuf) > 0xFFFFFFFF;
          decZ += POLYTEXZ_DecZBuf;
          intZ += POLYTEXZ_IntZBuf + carry;

          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

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

S32 Filler_TextureZChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrZBufferPatch = PtrZBuffer;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_IntZBuf = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_DecZBuf = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXZ_RepMaskPatch = RepMask;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 decZ = Fill_CurZBuf << 24;
        U32 intZ = Fill_CurZBuf >> 8;
        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          // @@LoopX:
          U8 textureColor = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;

          if (textureColor != 0 && POLYTEXZ_PtrZBufferPatch[offset] >= (intZ & 0xFFFF))
          {
            POLYTEXZ_PtrLog[offset] = textureColor;
          }

          // @@NoDisp:
          mapIndex = mapV_mapU >> 16;
          offset++;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          U8 carry = ((U64)decZ + (U64)POLYTEXZ_DecZBuf) > 0xFFFFFFFF;
          decZ += POLYTEXZ_DecZBuf;
          intZ += POLYTEXZ_IntZBuf + carry;

          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

// Converted from ASM - Flat texture rendering with chroma key, Z-buffer testing (no Z-write)
S32 Filler_TextureZFlatChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
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
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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
          U8 textureColor = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;

          // Chroma key test - skip transparent pixels (color 0)
          if (textureColor != 0 && POLYTEXZ_PtrZBufferPatch[offset] >= (intZ & 0xFFFF))
          {
            // NZW: No Z-buffer write, only read for testing
            U8 flatColor = POLYTEXZ_PtrCLUT[textureColor];
            POLYTEXZ_PtrLog[offset] = flatColor;
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

S32 Filler_TextureZFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrZBufferPatch = PtrZBuffer;
    POLYTEXZ_PtrZBuffer2 = PtrZBuffer;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_IntZBuf = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_IntZBuf2 = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_DecZBuf = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXZ_DecZBuf2 = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXZ_RepMaskPatch = RepMask;
    POLYTEXZ_RepMask2 = RepMask;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 decZ = Fill_CurZBuf << 24;
        U32 intZ = Fill_CurZBuf >> 8;
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
          U8 textureColor = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;
          textureColor &= 0xFF;

          U8 foggedColor = Fill_Logical_Palette[textureColor];
          POLYTEXZ_PtrLog[offset] = foggedColor;

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

S32 Filler_TextureZChromaKeyFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;

    POLYTEXZ_PtrZBufferPatch = PtrZBuffer;
    POLYTEXZ_PtrLog = (PTR_U8)Log;
    POLYTEXZ_PtrMapPatch = PtrMap;
    POLYTEXZ_IntZBuf = (U32)Fill_ZBuf_XSlope >> 8;
    POLYTEXZ_DecZBuf = (U32)Fill_ZBuf_XSlope << 24;
    POLYTEXZ_RepMaskPatch = RepMask;
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

      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16)
      {
        U64 addW = ((S64)(S32)factor * (S64)(S32)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = addWDec << 16;
        addWInt = addWInt >> 16;
        addWDec |= addWInt;
        Fill_Cur_W = Fill_CurWMin + addWDec;

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

      U32 offset = line - (U8 *)Log;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      while (1) {
        Fill_Loop_Perspective(fillWidth);

        U32 incMapU = Fill_Next_MapU - Fill_Cur_MapU;
        incMapU = ((S32)incMapU) >> PERSP_SHIFT;

        U32 incMapV = Fill_Next_MapV - Fill_Cur_MapV;
        incMapV = ((S32)incMapV) >> PERSP_SHIFT;

        U32 incV_incU = (incMapV << 16) | (incMapU & 0xFFFF);
        U32 mapV_mapU = (Fill_Cur_MapV << 16) | (Fill_Cur_MapU & 0xFFFF);

        U32 mapIndex = mapV_mapU >> 16;
        mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);
        mapIndex &= RepMask;

        U32 decZ = Fill_CurZBuf << 24;
        U32 intZ = Fill_CurZBuf >> 8;
        U32 lineWidth = fillWidth;

        if (fillWidth > PERSP_VALUE)
        {
          fillWidth = PERSP_VALUE;
        }

        do
        {
          // @@LoopX:
          U8 textureColor = POLYTEXZ_PtrMapPatch[mapIndex];
          mapV_mapU += incV_incU;
          textureColor &= 0xFF;

          if (textureColor != 0 && POLYTEXZ_PtrZBufferPatch[offset] >= (intZ & 0xFFFF))
          {
            U8 foggedColor = Fill_Logical_Palette[textureColor];
            POLYTEXZ_PtrLog[offset] = foggedColor;
          }

          // @@NoDisp:
          mapIndex = mapV_mapU >> 16;
          offset++;
          mapIndex = (mapIndex & 0xFFFFFF00) | ((mapV_mapU >> 8) & 0xFF);

          U8 carry = ((U64)decZ + (U64)POLYTEXZ_DecZBuf) > 0xFFFFFFFF;
          decZ += POLYTEXZ_DecZBuf;
          intZ += POLYTEXZ_IntZBuf + carry;

          mapIndex &= POLYTEXZ_RepMaskPatch;
          fillWidth--;
        }
        while (fillWidth);

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

