#include <POLYGON/POLYTZG.H>

#include <stdio.h>
#include <FILLER.H>
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/UTILS.H>
#include <SYSTEM/DEBUG_TRACES.H>

// Runtime patch variables for optimized rendering
PTR_U8 POLYTZG_PtrCLUT = NULL;
PTR_U8 POLYTZG_PtrMapPatch = NULL;
U32 POLYTZG_RepMaskPatch = 0;
U32 POLYTZG_DecGour = 0;
U8 POLYTZG_IntGour = 0;
S32 Cur_Gouraud = 0;

// External perspective functions (C linkage — function pointers in POLYTEXZ.CPP)
extern "C" {
extern void (*Fill_Init_Perspective)(S32 lineU, S32 lineV, U32 fillWidth);
extern void (*Fill_Loop_Perspective)(U32 fillWidth);
}

// Helper: signed 32×32→64 multiply, extract bits [16..47] (matches ASM imul + shl/shr/or)
static inline S32 imul_extract_16_47(S32 a, S32 b) {
  S64 prod = (S64)a * (S64)b;
  U32 lo = (U32)(prod & 0xFFFFFFFFu);
  S32 hi = (S32)(prod >> 32);
  return (S32)(((U32)hi << 16) | (lo >> 16));
}

S32 Filler_TextureZGouraud(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax) {
    LIB386_TRACE_CPP("Filler_TextureZGouraud", "1", "entry");
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYTZG_PtrCLUT = PtrCLUTGouraud;
    POLYTZG_RepMaskPatch = RepMask;
    POLYTZG_PtrMapPatch = PtrMap;
    POLYTZG_DecGour = Fill_Gouraud_XSlope << 16;
    POLYTZG_IntGour = (Fill_Gouraud_XSlope >> 16) & 0xFF;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;

      S32 lineU = u;
      S32 lineV = v;

      Fill_Cur_W = Fill_CurWMin;

      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16) {
        Fill_Cur_W = Fill_CurWMin + imul_extract_16_47(Fill_W_XSlope, factor);
        lineU += imul_extract_16_47(Fill_MapU_XSlope, factor);
        lineV += imul_extract_16_47(Fill_MapV_XSlope, factor);
      }

      Cur_Gouraud = gouraud;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      U32 remaining = fillWidth;
      U8 *dst = line;

      while (remaining > 0) {
        U32 blockSize = (remaining > PERSP_VALUE) ? PERSP_VALUE : remaining;

        Fill_Loop_Perspective(remaining);

        S32 curMapU = Fill_Cur_MapU;
        S32 nextMapU = Fill_Next_MapU;
        S32 curMapV = Fill_Cur_MapV;
        S32 nextMapV = Fill_Next_MapV;

        S32 incMapU = (nextMapU - curMapU) >> PERSP_SHIFT;
        S32 incMapV = (nextMapV - curMapV) >> PERSP_SHIFT;

        S32 gouraudValue = Cur_Gouraud;
        S32 gouraudInc = Fill_Gouraud_XSlope;

        for (U32 x = 0; x < blockSize; x++) {
          U32 texU = (curMapU >> 8) & (RepMask & 0xFF);
          U32 texV = (curMapV >> 8) & ((RepMask >> 8) & 0xFF);
          U8 texel = PtrMap[texV * 256 + texU];
          U32 gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel;
          *dst++ = PtrCLUTGouraud[gouraudIndex];

          curMapU += incMapU;
          curMapV += incMapV;
          gouraudValue += gouraudInc;
        }

        remaining -= blockSize;

        if (remaining > 0) {
          Fill_Cur_MapU = Fill_Next_MapU;
          Fill_Cur_MapV = Fill_Next_MapV;
          Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
          Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
          Fill_Cur_W = Fill_Next_W;
          Cur_Gouraud = gouraudValue;
        }
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurGouraudMin = gouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZGouraudChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax) {
    LIB386_TRACE_CPP("Filler_TextureZGouraudChromaKey", "1", "entry");
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYTZG_PtrCLUT = PtrCLUTGouraud;
    POLYTZG_RepMaskPatch = RepMask;
    POLYTZG_PtrMapPatch = PtrMap;
    POLYTZG_DecGour = Fill_Gouraud_XSlope << 16;
    POLYTZG_IntGour = (Fill_Gouraud_XSlope >> 16) & 0xFF;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;

      S32 lineU = u;
      S32 lineV = v;

      Fill_Cur_W = Fill_CurWMin;

      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16) {
        Fill_Cur_W = Fill_CurWMin + imul_extract_16_47(Fill_W_XSlope, factor);
        lineU += imul_extract_16_47(Fill_MapU_XSlope, factor);
        lineV += imul_extract_16_47(Fill_MapV_XSlope, factor);
      }

      Cur_Gouraud = gouraud;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      U32 remaining = fillWidth;
      U8 *dst = line;

      while (remaining > 0) {
        U32 blockSize = (remaining > PERSP_VALUE) ? PERSP_VALUE : remaining;

        Fill_Loop_Perspective(remaining);

        S32 curMapU = Fill_Cur_MapU;
        S32 nextMapU = Fill_Next_MapU;
        S32 curMapV = Fill_Cur_MapV;
        S32 nextMapV = Fill_Next_MapV;

        S32 incMapU = (nextMapU - curMapU) >> PERSP_SHIFT;
        S32 incMapV = (nextMapV - curMapV) >> PERSP_SHIFT;

        S32 gouraudValue = Cur_Gouraud;
        S32 gouraudInc = Fill_Gouraud_XSlope;

        for (U32 x = 0; x < blockSize; x++) {
          U32 texU = (curMapU >> 8) & (RepMask & 0xFF);
          U32 texV = (curMapV >> 8) & ((RepMask >> 8) & 0xFF);
          U8 texel = PtrMap[texV * 256 + texU];

          if (texel != 0) {
            U32 gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel;
            *dst = PtrCLUTGouraud[gouraudIndex];
          }

          dst++;
          curMapU += incMapU;
          curMapV += incMapV;
          gouraudValue += gouraudInc;
        }

        remaining -= blockSize;

        if (remaining > 0) {
          Fill_Cur_MapU = Fill_Next_MapU;
          Fill_Cur_MapV = Fill_Next_MapV;
          Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
          Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
          Fill_Cur_W = Fill_Next_W;
          Cur_Gouraud = gouraudValue;
        }
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurGouraudMin = gouraud;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZGouraudZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax) {
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
  U32 z = Fill_CurZBufMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYTZG_PtrCLUT = PtrCLUTGouraud;
    POLYTZG_RepMaskPatch = RepMask;
    POLYTZG_PtrMapPatch = PtrMap;
    POLYTZG_DecGour = Fill_Gouraud_XSlope << 16;
    POLYTZG_IntGour = (Fill_Gouraud_XSlope >> 16) & 0xFF;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    z += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 pixOfs = (U32)(Fill_CurOffLine - (PTR_U8)Log) + (fillCurXMin >> 16);

      S32 lineU = u;
      S32 lineV = v;
      U32 lineZ = z;

      Fill_Cur_W = Fill_CurWMin;

      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16) {
        Fill_Cur_W = Fill_CurWMin + imul_extract_16_47(Fill_W_XSlope, factor);
        lineU += imul_extract_16_47(Fill_MapU_XSlope, factor);
        lineV += imul_extract_16_47(Fill_MapV_XSlope, factor);
      }

      Fill_CurZBuf = lineZ;
      Cur_Gouraud = gouraud;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      U32 remaining = fillWidth;
      U8 *dst = line;

      while (remaining > 0) {
        U32 blockSize = (remaining > PERSP_VALUE) ? PERSP_VALUE : remaining;

        Fill_Loop_Perspective(remaining);

        S32 curMapU = Fill_Cur_MapU;
        S32 nextMapU = Fill_Next_MapU;
        S32 curMapV = Fill_Cur_MapV;
        S32 nextMapV = Fill_Next_MapV;

        S32 incMapU = (nextMapU - curMapU) >> PERSP_SHIFT;
        S32 incMapV = (nextMapV - curMapV) >> PERSP_SHIFT;

        S32 gouraudValue = Cur_Gouraud;
        S32 gouraudInc = Fill_Gouraud_XSlope;
        U32 zValue = Fill_CurZBuf;
        U32 zInc = Fill_ZBuf_XSlope;

        for (U32 x = 0; x < blockSize; x++) {
          U16 zComp = (U16)(zValue >> 8);

          if (PtrZBuffer[pixOfs] >= zComp) {
            U32 texU = (curMapU >> 8) & (RepMask & 0xFF);
            U32 texV = (curMapV >> 8) & ((RepMask >> 8) & 0xFF);
            U8 texel = PtrMap[texV * 256 + texU];
            U32 gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel;
            *dst = PtrCLUTGouraud[gouraudIndex];
            PtrZBuffer[pixOfs] = zComp;
          }

          dst++;
          pixOfs++;
          curMapU += incMapU;
          curMapV += incMapV;
          gouraudValue += gouraudInc;
          zValue += zInc;
        }

        remaining -= blockSize;

        if (remaining > 0) {
          Fill_Cur_MapU = Fill_Next_MapU;
          Fill_Cur_MapV = Fill_Next_MapV;
          Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
          Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
          Fill_Cur_W = Fill_Next_W;
          Cur_Gouraud = gouraudValue;
          Fill_CurZBuf = zValue;
        }
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    z += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurGouraudMin = gouraud;
  Fill_CurZBufMin = z;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZGouraudChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax) {
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
  U32 z = Fill_CurZBufMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYTZG_PtrCLUT = PtrCLUTGouraud;
    POLYTZG_RepMaskPatch = RepMask;
    POLYTZG_PtrMapPatch = PtrMap;
    POLYTZG_DecGour = Fill_Gouraud_XSlope << 16;
    POLYTZG_IntGour = (Fill_Gouraud_XSlope >> 16) & 0xFF;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    z += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 pixOfs = (U32)(Fill_CurOffLine - (PTR_U8)Log) + (fillCurXMin >> 16);

      S32 lineU = u;
      S32 lineV = v;
      U32 lineZ = z;

      Fill_Cur_W = Fill_CurWMin;

      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16) {
        Fill_Cur_W = Fill_CurWMin + imul_extract_16_47(Fill_W_XSlope, factor);
        lineU += imul_extract_16_47(Fill_MapU_XSlope, factor);
        lineV += imul_extract_16_47(Fill_MapV_XSlope, factor);
      }

      Fill_CurZBuf = lineZ;
      Cur_Gouraud = gouraud;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      U32 remaining = fillWidth;
      U8 *dst = line;

      while (remaining > 0) {
        U32 blockSize = (remaining > PERSP_VALUE) ? PERSP_VALUE : remaining;

        Fill_Loop_Perspective(remaining);

        S32 curMapU = Fill_Cur_MapU;
        S32 nextMapU = Fill_Next_MapU;
        S32 curMapV = Fill_Cur_MapV;
        S32 nextMapV = Fill_Next_MapV;

        S32 incMapU = (nextMapU - curMapU) >> PERSP_SHIFT;
        S32 incMapV = (nextMapV - curMapV) >> PERSP_SHIFT;

        S32 gouraudValue = Cur_Gouraud;
        S32 gouraudInc = Fill_Gouraud_XSlope;
        U32 zValue = Fill_CurZBuf;
        U32 zInc = Fill_ZBuf_XSlope;

        for (U32 x = 0; x < blockSize; x++) {
          U16 zComp = (U16)(zValue >> 8);

          if (PtrZBuffer[pixOfs] >= zComp) {
            U32 texU = (curMapU >> 8) & (RepMask & 0xFF);
            U32 texV = (curMapV >> 8) & ((RepMask >> 8) & 0xFF);
            U8 texel = PtrMap[texV * 256 + texU];

            if (texel != 0) {
              U32 gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel;
              *dst = PtrCLUTGouraud[gouraudIndex];
              PtrZBuffer[pixOfs] = zComp;
            }
          }

          dst++;
          pixOfs++;
          curMapU += incMapU;
          curMapV += incMapV;
          gouraudValue += gouraudInc;
          zValue += zInc;
        }

        remaining -= blockSize;

        if (remaining > 0) {
          Fill_Cur_MapU = Fill_Next_MapU;
          Fill_Cur_MapV = Fill_Next_MapV;
          Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
          Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
          Fill_Cur_W = Fill_Next_W;
          Cur_Gouraud = gouraudValue;
          Fill_CurZBuf = zValue;
        }
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    z += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurGouraudMin = gouraud;
  Fill_CurZBufMin = z;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZGouraudNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax) {
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
  U32 z = Fill_CurZBufMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYTZG_PtrCLUT = PtrCLUTGouraud;
    POLYTZG_RepMaskPatch = RepMask;
    POLYTZG_PtrMapPatch = PtrMap;
    POLYTZG_DecGour = Fill_Gouraud_XSlope << 16;
    POLYTZG_IntGour = (Fill_Gouraud_XSlope >> 16) & 0xFF;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    z += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 pixOfs = (U32)(Fill_CurOffLine - (PTR_U8)Log) + (fillCurXMin >> 16);

      S32 lineU = u;
      S32 lineV = v;
      U32 lineZ = z;

      Fill_Cur_W = Fill_CurWMin;

      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16) {
        Fill_Cur_W = Fill_CurWMin + imul_extract_16_47(Fill_W_XSlope, factor);
        lineU += imul_extract_16_47(Fill_MapU_XSlope, factor);
        lineV += imul_extract_16_47(Fill_MapV_XSlope, factor);
      }

      Fill_CurZBuf = lineZ;
      Cur_Gouraud = gouraud;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      U32 remaining = fillWidth;
      U8 *dst = line;

      while (remaining > 0) {
        U32 blockSize = (remaining > PERSP_VALUE) ? PERSP_VALUE : remaining;

        Fill_Loop_Perspective(remaining);

        S32 curMapU = Fill_Cur_MapU;
        S32 nextMapU = Fill_Next_MapU;
        S32 curMapV = Fill_Cur_MapV;
        S32 nextMapV = Fill_Next_MapV;

        S32 incMapU = (nextMapU - curMapU) >> PERSP_SHIFT;
        S32 incMapV = (nextMapV - curMapV) >> PERSP_SHIFT;

        S32 gouraudValue = Cur_Gouraud;
        S32 gouraudInc = Fill_Gouraud_XSlope;
        U32 zValue = Fill_CurZBuf;
        U32 zInc = Fill_ZBuf_XSlope;

        for (U32 x = 0; x < blockSize; x++) {
          U16 zComp = (U16)(zValue >> 8);

          if (PtrZBuffer[pixOfs] >= zComp) {
            U32 texU = (curMapU >> 8) & (RepMask & 0xFF);
            U32 texV = (curMapV >> 8) & ((RepMask >> 8) & 0xFF);
            U8 texel = PtrMap[texV * 256 + texU];
            U32 gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel;
            *dst = PtrCLUTGouraud[gouraudIndex];
            // NZW: no Z-buffer write
          }

          dst++;
          pixOfs++;
          curMapU += incMapU;
          curMapV += incMapV;
          gouraudValue += gouraudInc;
          zValue += zInc;
        }

        remaining -= blockSize;

        if (remaining > 0) {
          Fill_Cur_MapU = Fill_Next_MapU;
          Fill_Cur_MapV = Fill_Next_MapV;
          Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
          Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
          Fill_Cur_W = Fill_Next_W;
          Cur_Gouraud = gouraudValue;
          Fill_CurZBuf = zValue;
        }
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    z += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurGouraudMin = gouraud;
  Fill_CurZBufMin = z;

  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZGouraudChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax) {
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
  U32 z = Fill_CurZBufMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    POLYTZG_PtrCLUT = PtrCLUTGouraud;
    POLYTZG_RepMaskPatch = RepMask;
    POLYTZG_PtrMapPatch = PtrMap;
    POLYTZG_DecGour = Fill_Gouraud_XSlope << 16;
    POLYTZG_IntGour = (Fill_Gouraud_XSlope >> 16) & 0xFF;
  }
  else
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    z += Fill_ZBuf_LeftSlope;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 pixOfs = (U32)(Fill_CurOffLine - (PTR_U8)Log) + (fillCurXMin >> 16);

      S32 lineU = u;
      S32 lineV = v;
      U32 lineZ = z;

      Fill_Cur_W = Fill_CurWMin;

      U32 factor = (fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16) {
        Fill_Cur_W = Fill_CurWMin + imul_extract_16_47(Fill_W_XSlope, factor);
        lineU += imul_extract_16_47(Fill_MapU_XSlope, factor);
        lineV += imul_extract_16_47(Fill_MapV_XSlope, factor);
      }

      Fill_CurZBuf = lineZ;
      Cur_Gouraud = gouraud;
      Fill_Init_Perspective(lineU, lineV, fillWidth);

      U32 remaining = fillWidth;
      U8 *dst = line;

      while (remaining > 0) {
        U32 blockSize = (remaining > PERSP_VALUE) ? PERSP_VALUE : remaining;

        Fill_Loop_Perspective(remaining);

        S32 curMapU = Fill_Cur_MapU;
        S32 nextMapU = Fill_Next_MapU;
        S32 curMapV = Fill_Cur_MapV;
        S32 nextMapV = Fill_Next_MapV;

        S32 incMapU = (nextMapU - curMapU) >> PERSP_SHIFT;
        S32 incMapV = (nextMapV - curMapV) >> PERSP_SHIFT;

        S32 gouraudValue = Cur_Gouraud;
        S32 gouraudInc = Fill_Gouraud_XSlope;
        U32 zValue = Fill_CurZBuf;
        U32 zInc = Fill_ZBuf_XSlope;

        for (U32 x = 0; x < blockSize; x++) {
          U16 zComp = (U16)(zValue >> 8);

          if (PtrZBuffer[pixOfs] >= zComp) {
            U32 texU = (curMapU >> 8) & (RepMask & 0xFF);
            U32 texV = (curMapV >> 8) & ((RepMask >> 8) & 0xFF);
            U8 texel = PtrMap[texV * 256 + texU];

            if (texel != 0) {
              U32 gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel;
              *dst = PtrCLUTGouraud[gouraudIndex];
              // NZW: no Z-buffer write
            }
          }

          dst++;
          pixOfs++;
          curMapU += incMapU;
          curMapV += incMapV;
          gouraudValue += gouraudInc;
          zValue += zInc;
        }

        remaining -= blockSize;

        if (remaining > 0) {
          Fill_Cur_MapU = Fill_Next_MapU;
          Fill_Cur_MapV = Fill_Next_MapV;
          Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
          Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
          Fill_Cur_W = Fill_Next_W;
          Cur_Gouraud = gouraudValue;
          Fill_CurZBuf = zValue;
        }
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    Fill_CurWMin += Fill_W_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    z += Fill_ZBuf_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurGouraudMin = gouraud;
  Fill_CurZBufMin = z;

  return Triangle_ReadNextEdge(NULL, NULL);
}
