#include <POLYGON/POLYTZF.H>

#include <stdio.h>
#include <math.h>
#include <FILLER.H>
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/UTILS.H>

// External variables from POLY.CPP
extern U32 Fill_Fog_Factor;
extern U32 Fill_ScaledFogNear;

/* Local perspective state for Z-buffer fog smooth filler.
 * POLYTZF uses PERSP_VALUE=32 (shift=5) unlike POLYTEXZ's 16 (shift=4).
 * The FPU stack carries ZBuf/Z alongside U/Z and V/Z. */
static S32 POLYTZF_Cur_ZBufOverW;
static S32 POLYTZF_Next_ZBufOverW;
static S32 Fill_Next_ZBuf;

#define POLYTZF_PERSP_VALUE 32
#define POLYTZF_PERSP_SHIFT 5

/* Perspective inverse W — stored as volatile long double to match
 * x87 80-bit extended precision used by the ASM filler. */
static volatile long double POLYTZF_PerspInvW;

/* W stack for POLYTZF */
#define POLYTZF_MAX_STACK 64
static struct {
    S32 data[POLYTZF_MAX_STACK];
    U32 top_index;
} POLYTZF_WStack;

static void POLYTZF_WStack_clear(void) { POLYTZF_WStack.top_index = 0; }
static void POLYTZF_WStack_push(S32 v) { if (POLYTZF_WStack.top_index < POLYTZF_MAX_STACK) POLYTZF_WStack.data[POLYTZF_WStack.top_index++] = v; }
static S32  POLYTZF_WStack_top(void) { return POLYTZF_WStack.top_index > 0 ? POLYTZF_WStack.data[POLYTZF_WStack.top_index - 1] : 0; }
static void POLYTZF_WStack_pop(void) { if (POLYTZF_WStack.top_index > 0) POLYTZF_WStack.top_index--; }
static U32  POLYTZF_WStack_size(void) { return POLYTZF_WStack.top_index; }

static S32 tzf_persp_eval(S32 val)
{
    volatile long double product = (long double)val * POLYTZF_PerspInvW;
    return (S32)lrintl(product);
}

static S32 tzf_persp_eval_zbuf(S32 val)
{
    volatile long double product = (long double)val * POLYTZF_PerspInvW * 256.0f;
    return (S32)lrintl(product);
}

static void POLYTZF_Init_Perspective(S32 lineU, S32 lineV, U32 fillWidth)
{
    S32 w = Fill_Cur_W;
    if (w == 0 || w == -1 || w == 1) { w = 2; Fill_Cur_W = 2; }

    Fill_Cur_MapUOverW = lineU;
    Fill_Cur_MapVOverW = lineV;
    S32 wXSlope = Fill_W_XSlope << POLYTZF_PERSP_SHIFT;

    POLYTZF_PerspInvW = (long double)256.0f / (long double)w;

    POLYTZF_WStack_clear();
    U32 stackLength = fillWidth + POLYTZF_PERSP_VALUE - 1;
    stackLength >>= POLYTZF_PERSP_SHIFT;
    do {
        w += wXSlope;
        if (w == 0 || w == -1 || w == 1) w = 2;
        POLYTZF_WStack_push(w);
    } while (POLYTZF_WStack_size() < stackLength);

    Fill_Cur_W = POLYTZF_WStack_top();
    POLYTZF_WStack_pop();
    POLYTZF_Cur_ZBufOverW = Fill_CurZBufMin;

    Fill_Cur_MapU = tzf_persp_eval(Fill_Cur_MapUOverW);
    Fill_Cur_MapV = tzf_persp_eval(Fill_Cur_MapVOverW);
    Fill_CurZBuf = tzf_persp_eval_zbuf(POLYTZF_Cur_ZBufOverW);

    POLYTZF_PerspInvW = (long double)256.0f / (long double)Fill_Cur_W;
}

static void POLYTZF_Loop_Perspective(U32 fillWidth)
{
    Fill_Next_MapUOverW = (Fill_MapU_XSlope << POLYTZF_PERSP_SHIFT) + Fill_Cur_MapUOverW;
    Fill_Next_MapVOverW = (Fill_MapV_XSlope << POLYTZF_PERSP_SHIFT) + Fill_Cur_MapVOverW;
    POLYTZF_Next_ZBufOverW = (Fill_ZBuf_XSlope << POLYTZF_PERSP_SHIFT) + POLYTZF_Cur_ZBufOverW;

    Fill_Next_MapU = tzf_persp_eval(Fill_Next_MapUOverW);
    Fill_Next_MapV = tzf_persp_eval(Fill_Next_MapVOverW);
    Fill_Next_ZBuf = tzf_persp_eval_zbuf(POLYTZF_Next_ZBufOverW);

    if (fillWidth > POLYTZF_PERSP_VALUE)
    {
        Fill_Next_W = POLYTZF_WStack_top();
        POLYTZF_WStack_pop();
        POLYTZF_PerspInvW = (long double)256.0f / (long double)Fill_Next_W;
    }
}

S32 Filler_TextureZFogSmooth(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 w = Fill_CurWMin;
  U32 patch = Fill_Patch - 1;

  static U32 POLYTZF_RepMaskPatch;
  static PTR_U8 POLYTZF_PtrMapPatch;
  static PTR_U8 POLYTZF_PtrCLUT;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYTZF_RepMaskPatch = RepMask;
    POLYTZF_PtrMapPatch = PtrMap;
    POLYTZF_PtrCLUT = PtrCLUTFog + 12 * 256;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    w += Fill_W_LeftSlope;
  }

  while (1)
  {
    PTR_U8 line = Fill_CurOffLine;
    U32 xStart = fillCurXMin >> 16;
    U32 xEnd = fillCurXMax >> 16;
    if (xEnd > xStart)
    {
      U32 fillWidth = xEnd - xStart;
      line += xStart;

      /* Local copies for sub-pixel correction — do NOT modify outer u,v,w */
      S32 lineU = u;
      S32 lineV = v;

      Fill_CurWMin = w;
      Fill_Cur_W = w;

      /* Sub-pixel correction for wide scanlines.
       * Must match ASM's "imul / shl edx,16 / shr eax,16 / or" pattern. */
      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 32)
      {
        {
          S64 prod = (S64)(S32)factor * (S64)(S32)Fill_W_XSlope;
          U32 lo = (U32)(prod & 0xFFFFFFFFu);
          S32 hi = (S32)(prod >> 32);
          S32 addW = (S32)(((U32)hi << 16) | (lo >> 16));
          Fill_Cur_W = w + addW;
        }
        {
          S64 prod = (S64)(S32)factor * (S64)(S32)Fill_MapU_XSlope;
          U32 lo = (U32)(prod & 0xFFFFFFFFu);
          S32 hi = (S32)(prod >> 32);
          S32 addU = (S32)(((U32)hi << 16) | (lo >> 16));
          lineU += addU;
        }
        {
          S64 prod = (S64)(S32)factor * (S64)(S32)Fill_MapV_XSlope;
          U32 lo = (U32)(prod & 0xFFFFFFFFu);
          S32 hi = (S32)(prod >> 32);
          S32 addV = (S32)(((U32)hi << 16) | (lo >> 16));
          lineV += addV;
        }
      }

      POLYTZF_Init_Perspective(lineU, lineV, fillWidth);

      U32 remaining = fillWidth;
      while (1)
      {
        POLYTZF_Loop_Perspective(remaining);

        S32 curMapU = Fill_Cur_MapU;
        S32 nextMapU = Fill_Next_MapU;
        S32 curMapV = Fill_Cur_MapV;
        S32 nextMapV = Fill_Next_MapV;

        S32 incU = (nextMapU - curMapU) >> 5;    /* SAR — signed shift */
        S32 incV = (nextMapV - curMapV) >> 5;    /* SAR — signed shift */
        incU &= 0xFFFF;
        curMapU &= 0xFFFF;
        incV = (incV << 16) & (S32)0xFFFF0000u;
        curMapV = (curMapV << 16) & (S32)0xFFFF0000u;
        U32 uvInc = (U32)incV | (U32)incU;
        U32 uvCur = (U32)curMapV | (U32)curMapU;

        /* Fog computation from Z buffer value */
        S32 zBuf = Fill_CurZBuf >> 8;
        S32 fogDist = zBuf - (S32)Fill_ScaledFogNear;
        U32 fogRow;
        if (fogDist < 0)
        {
          fogRow = 0;
        }
        else
        {
          fogRow = ((U32)fogDist * Fill_Fog_Factor) >> 8;
          fogRow &= 0xF000;
        }

        U32 spanLen = (remaining > 32) ? 32 : remaining;

        for (U32 px = 0; px < spanLen; px++)
        {
          U32 U_int = (uvCur >> 8) & 0xFF;
          U32 V_int = (uvCur >> 16) & 0xFF;
          U32 mapIndex = (V_int << 8) | U_int;
          mapIndex &= POLYTZF_RepMaskPatch;
          U8 texel = POLYTZF_PtrMapPatch[mapIndex];
          *line++ = POLYTZF_PtrCLUT[fogRow + texel];
          uvCur += uvInc;
        }

        if (remaining <= 32) break;

        Fill_Cur_MapU = Fill_Next_MapU;
        Fill_Cur_MapV = Fill_Next_MapV;
        Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
        Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
        Fill_Cur_W = Fill_Next_W;
        Fill_CurZBuf = Fill_Next_ZBuf;
        POLYTZF_Cur_ZBufOverW = POLYTZF_Next_ZBufOverW;
        remaining -= 32;
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    w += Fill_W_LeftSlope;
    Fill_CurZBufMin += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = (PTR_U8)Fill_CurOffLine;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurWMin = w;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZFogSmoothZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 w = Fill_CurWMin;
  U32 z = Fill_CurZMin;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    w += Fill_W_LeftSlope;
    z += Fill_Z_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U16 *zbuffer = (fillCurXMin >> 16) + PtrZBuffer;
      
      S32 lineU = u;
      S32 lineV = v;
      S32 lineW = w;
      U32 lineZ = z;

      // Texture mapping with fog and Z-buffer
      for (U32 x = 0; x < fillWidth; x++)
      {
        U16 currentZ = lineZ >> 16;
        
        // Z-buffer test
        if (currentZ < *zbuffer)
        {
          // Get texture coordinates
          U32 texU = (lineU >> 16) & (RepMask & 0xFF);
          U32 texV = (lineV >> 16) & ((RepMask >> 8) & 0xFF);
          
          // Get texture pixel
          U8 texel = PtrMap[texV * 256 + texU]; // Assuming 256x256 texture
          
          // Calculate fog factor based on W (depth)
          U32 fogFactor;
          if (lineW < (S32)Fill_ScaledFogNear) {
            fogFactor = 0; // No fog
          } else {
            U32 fogDist = lineW - Fill_ScaledFogNear;
            fogFactor = (fogDist * Fill_Fog_Factor) >> 16;
            if (fogFactor > 255) fogFactor = 255;
          }
          
          // Apply fog using fog color lookup table
          U8 finalColor = PtrCLUTFog[(fogFactor << 8) + texel];
          
          *line = finalColor;
          *zbuffer = currentZ; // Write Z-buffer
        }
        
        line++;
        zbuffer++;
        lineU += Fill_MapU_XSlope;
        lineV += Fill_MapV_XSlope;
        lineW += Fill_W_XSlope;
        lineZ += Fill_Z_XSlope;
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    w += Fill_W_LeftSlope;
    z += Fill_Z_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurWMin = w;
  Fill_CurZMin = z;
  
  return 0;
}

S32 Filler_TextureZFogSmoothNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 w = Fill_CurWMin;
  U32 z = Fill_CurZMin;
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
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    w += Fill_W_LeftSlope;
    z += Fill_Z_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U16 *zbuffer = (fillCurXMin >> 16) + PtrZBuffer;
      
      S32 lineU = u;
      S32 lineV = v;
      S32 lineW = w;
      U32 lineZ = z;

      // Texture mapping with fog and Z-buffer (No Z Write)
      for (U32 x = 0; x < fillWidth; x++)
      {
        U16 currentZ = lineZ >> 16;
        
        // Z-buffer test (no write - NZW = No Z Write)
        if (currentZ < *zbuffer)
        {
          // Get texture coordinates
          U32 texU = (lineU >> 16) & (RepMask & 0xFF);
          U32 texV = (lineV >> 16) & ((RepMask >> 8) & 0xFF);
          
          // Get texture pixel
          U8 texel = PtrMap[texV * 256 + texU]; // Assuming 256x256 texture
          
          // Calculate fog factor based on W (depth)
          U32 fogFactor;
          if (lineW < (S32)Fill_ScaledFogNear) {
            fogFactor = 0; // No fog
          } else {
            U32 fogDist = lineW - Fill_ScaledFogNear;
            fogFactor = (fogDist * Fill_Fog_Factor) >> 16;
            if (fogFactor > 255) fogFactor = 255;
          }
          
          // Apply fog using fog color lookup table
          U8 finalColor = PtrCLUTFog[(fogFactor << 8) + texel];
          
          *line = finalColor;
          // Note: No Z-buffer write for NZW variant
        }
        
        line++;
        zbuffer++;
        lineU += Fill_MapU_XSlope;
        lineV += Fill_MapV_XSlope;
        lineW += Fill_W_XSlope;
        lineZ += Fill_Z_XSlope;
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    w += Fill_W_LeftSlope;
    z += Fill_Z_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurWMin = w;
  Fill_CurZMin = z;
  
  return 0;
}
