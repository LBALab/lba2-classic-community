#include <POLYGON/POLYTZG.H>

#include <stdio.h>
#include <FILLER.H>
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/UTILS.H>

// Runtime patch variables for optimized rendering
PTR_U8 POLYTZG_PtrCLUT = NULL;
PTR_U8 POLYTZG_PtrMapPatch = NULL;
U32 POLYTZG_RepMaskPatch = 0;
U32 POLYTZG_DecGour = 0;
U8 POLYTZG_IntGour = 0;
S32 Cur_Gouraud = 0;

// External perspective functions
extern void Fill_Init_Perspective(S32 lineU, S32 lineV, U32 fillWidth);
extern void Fill_Loop_Perspective(U32 fillWidth);

S32 Filler_TextureZGouraud(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax) {
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    // Setup runtime patches for optimized rendering
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
      S32 lineGouraud = gouraud;

      // Handle sub-pixel precision offset
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16) {
        // Apply sub-pixel offset for texture coordinates
        U64 addU = ((U64)factor * (U64)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = (addUDec << 16) | (addUInt >> 16);
        lineU += addUDec;

        U64 addV = ((U64)factor * (U64)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = (addVDec << 16) | (addVInt >> 16);
        lineV += addVDec;

        U64 addW = ((U64)factor * (U64)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = (addWDec << 16) | (addWInt >> 16);
        Fill_CurWMin += addWDec;
      }

      Cur_Gouraud = lineGouraud;
      Fill_Cur_W = Fill_CurWMin;

      // Use perspective-correct texture mapping
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
          // Get texture coordinates with perspective correction
          U32 texU = (curMapU >> 16) & (RepMask & 0xFF);
          U32 texV = (curMapV >> 16) & ((RepMask >> 8) & 0xFF);
          
          // Get texture pixel
          U8 texel = PtrMap[texV * 256 + texU];
          
          // Apply Gouraud shading
          U32 gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel;
          U8 shadedColor = PtrCLUTGouraud[gouraudIndex];
          
          *dst++ = shadedColor;
          
          curMapU += incMapU;
          curMapV += incMapV;
          gouraudValue += gouraudInc;
        }
        
        remaining -= blockSize;
        
        if (remaining > 0) {
          // Update perspective values for next block
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
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    // Setup runtime patches for optimized rendering
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
      S32 lineGouraud = gouraud;

      // Handle sub-pixel precision offset
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16) {
        // Apply sub-pixel offset for texture coordinates
        U64 addU = ((U64)factor * (U64)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = (addUDec << 16) | (addUInt >> 16);
        lineU += addUDec;

        U64 addV = ((U64)factor * (U64)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = (addVDec << 16) | (addVInt >> 16);
        lineV += addVDec;

        U64 addW = ((U64)factor * (U64)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = (addWDec << 16) | (addWInt >> 16);
        Fill_CurWMin += addWDec;
      }

      Cur_Gouraud = lineGouraud;
      Fill_Cur_W = Fill_CurWMin;

      // Use perspective-correct texture mapping
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
          // Get texture coordinates with perspective correction
          U32 texU = (curMapU >> 16) & (RepMask & 0xFF);
          U32 texV = (curMapV >> 16) & ((RepMask >> 8) & 0xFF);
          
          // Get texture pixel
          U8 texel = PtrMap[texV * 256 + texU];
          
          // Apply chroma key test (skip transparent pixels)
          if (texel != 0) {
            // Apply Gouraud shading
            U32 gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel;
            U8 shadedColor = PtrCLUTGouraud[gouraudIndex];
            *dst = shadedColor;
          }
          
          dst++;
          curMapU += incMapU;
          curMapV += incMapV;
          gouraudValue += gouraudInc;
        }
        
        remaining -= blockSize;
        
        if (remaining > 0) {
          // Update perspective values for next block
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
  U32 z = Fill_CurZMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    // Setup runtime patches for optimized rendering
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
      S32 lineGouraud = gouraud;
      U32 lineZ = z;

      // Handle sub-pixel precision offset
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16) {
        // Apply sub-pixel offset for texture coordinates
        U64 addU = ((U64)factor * (U64)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = (addUDec << 16) | (addUInt >> 16);
        lineU += addUDec;

        U64 addV = ((U64)factor * (U64)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = (addVDec << 16) | (addVInt >> 16);
        lineV += addVDec;

        U64 addW = ((U64)factor * (U64)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = (addWDec << 16) | (addWInt >> 16);
        Fill_CurWMin += addWDec;

        U64 addZ = ((U64)factor * (U64)Fill_Z_XSlope);
        U32 addZDec = (U32)(addZ >> 32);
        U32 addZInt = (U32)(addZ & 0xFFFFFFFF);
        addZDec = (addZDec << 16) | (addZInt >> 16);
        lineZ += addZDec;
      }

      Cur_Gouraud = lineGouraud;
      Fill_Cur_W = Fill_CurWMin;

      // Use perspective-correct texture mapping
      Fill_Init_Perspective(lineU, lineV, fillWidth);
      
      U32 remaining = fillWidth;
      U8 *dst = line;
      U16 *zBuf = zbuffer;
      U32 currentZ = lineZ;
      
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
        U32 zValue = currentZ;
        U32 zInc = Fill_Z_XSlope;
        
        for (U32 x = 0; x < blockSize; x++) {
          U16 depthValue = zValue >> 16;
          
          // Z-buffer test
          if (depthValue < *zBuf) {
            // Get texture coordinates with perspective correction
            U32 texU = (curMapU >> 16) & (RepMask & 0xFF);
            U32 texV = (curMapV >> 16) & ((RepMask >> 8) & 0xFF);
            
            // Get texture pixel
            U8 texel = PtrMap[texV * 256 + texU];
            
            // Apply Gouraud shading
            U32 gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel;
            U8 shadedColor = PtrCLUTGouraud[gouraudIndex];
            
            *dst = shadedColor;
            *zBuf = depthValue; // Write Z-buffer
          }
          
          dst++;
          zBuf++;
          curMapU += incMapU;
          curMapV += incMapV;
          gouraudValue += gouraudInc;
          zValue += zInc;
        }
        
        remaining -= blockSize;
        
        if (remaining > 0) {
          // Update perspective values for next block
          Fill_Cur_MapU = Fill_Next_MapU;
          Fill_Cur_MapV = Fill_Next_MapV;
          Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
          Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
          Fill_Cur_W = Fill_Next_W;
          Cur_Gouraud = gouraudValue;
          currentZ = zValue;
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
    z += Fill_Z_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurGouraudMin = gouraud;
  Fill_CurZMin = z;
  
  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZGouraudChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax) {
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
  U32 z = Fill_CurZMin;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
    // Setup runtime patches for optimized rendering
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
      S32 lineGouraud = gouraud;
      U32 lineZ = z;

      // Handle sub-pixel precision offset
      U32 factor = 0x10000 - (fillCurXMin & 0xFFFF);
      if (fillWidth > 16) {
        // Apply sub-pixel offset for texture coordinates
        U64 addU = ((U64)factor * (U64)Fill_MapU_XSlope);
        U32 addUDec = (U32)(addU >> 32);
        U32 addUInt = (U32)(addU & 0xFFFFFFFF);
        addUDec = (addUDec << 16) | (addUInt >> 16);
        lineU += addUDec;

        U64 addV = ((U64)factor * (U64)Fill_MapV_XSlope);
        U32 addVDec = (U32)(addV >> 32);
        U32 addVInt = (U32)(addV & 0xFFFFFFFF);
        addVDec = (addVDec << 16) | (addVInt >> 16);
        lineV += addVDec;

        U64 addW = ((U64)factor * (U64)Fill_W_XSlope);
        U32 addWDec = (U32)(addW >> 32);
        U32 addWInt = (U32)(addW & 0xFFFFFFFF);
        addWDec = (addWDec << 16) | (addWInt >> 16);
        Fill_CurWMin += addWDec;

        U64 addZ = ((U64)factor * (U64)Fill_Z_XSlope);
        U32 addZDec = (U32)(addZ >> 32);
        U32 addZInt = (U32)(addZ & 0xFFFFFFFF);
        addZDec = (addZDec << 16) | (addZInt >> 16);
        lineZ += addZDec;
      }

      Cur_Gouraud = lineGouraud;
      Fill_Cur_W = Fill_CurWMin;

      // Use perspective-correct texture mapping
      Fill_Init_Perspective(lineU, lineV, fillWidth);
      
      U32 remaining = fillWidth;
      U8 *dst = line;
      U16 *zBuf = zbuffer;
      U32 currentZ = lineZ;
      
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
        U32 zValue = currentZ;
        U32 zInc = Fill_Z_XSlope;
        
        for (U32 x = 0; x < blockSize; x++) {
          U16 depthValue = zValue >> 16;
          
          // Z-buffer test
          if (depthValue < *zBuf) {
            // Get texture coordinates with perspective correction
            U32 texU = (curMapU >> 16) & (RepMask & 0xFF);
            U32 texV = (curMapV >> 16) & ((RepMask >> 8) & 0xFF);
            
            // Get texture pixel
            U8 texel = PtrMap[texV * 256 + texU];
            
            // Apply chroma key test (skip transparent pixels)
            if (texel != 0) {
              // Apply Gouraud shading
              U32 gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel;
              U8 shadedColor = PtrCLUTGouraud[gouraudIndex];
              
              *dst = shadedColor;
              *zBuf = depthValue; // Write Z-buffer
            }
          }
          
          dst++;
          zBuf++;
          curMapU += incMapU;
          curMapV += incMapV;
          gouraudValue += gouraudInc;
          zValue += zInc;
        }
        
        remaining -= blockSize;
        
        if (remaining > 0) {
          // Update perspective values for next block
          Fill_Cur_MapU = Fill_Next_MapU;
          Fill_Cur_MapV = Fill_Next_MapV;
          Fill_Cur_MapUOverW = Fill_Next_MapUOverW;
          Fill_Cur_MapVOverW = Fill_Next_MapVOverW;
          Fill_Cur_W = Fill_Next_W;
          Cur_Gouraud = gouraudValue;
          currentZ = zValue;
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
    z += Fill_Z_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurGouraudMin = gouraud;
  Fill_CurZMin = z;
  
  return Triangle_ReadNextEdge(NULL, NULL);
}

S32 Filler_TextureZGouraudNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax) {
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
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
    gouraud += Fill_Gouraud_LeftSlope;
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
      S32 lineGouraud = gouraud;
      U32 lineZ = z;

      // Texture mapping with Gouraud shading and Z-buffer (No Z Write)
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
          U8 texel = PtrMap[texV * 256 + texU];
          
          // Apply Gouraud shading
          U32 gouraudIntensity = (lineGouraud >> 16) & 0xFF;
          U8 shadedColor = PtrCLUTGouraud[(gouraudIntensity << 8) + texel];
          
          *line = shadedColor;
          // Note: No Z-buffer write for NZW variant
        }
        
        line++;
        zbuffer++;
        lineU += Fill_MapU_XSlope;
        lineV += Fill_MapV_XSlope;
        lineGouraud += Fill_Gouraud_XSlope;
        lineZ += Fill_Z_XSlope;
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    z += Fill_Z_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurGouraudMin = gouraud;
  Fill_CurZMin = z;
  
  return 0;
}

S32 Filler_TextureZGouraudChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax) {
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 gouraud = Fill_CurGouraudMin;
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
    gouraud += Fill_Gouraud_LeftSlope;
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
      S32 lineGouraud = gouraud;
      U32 lineZ = z;

      // Texture mapping with Gouraud shading, chroma key, and Z-buffer (No Z Write)
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
          U8 texel = PtrMap[texV * 256 + texU];
          
          // Chroma key test (skip transparent pixels)
          if (texel != 0)
          {
            // Apply Gouraud shading
            U32 gouraudIntensity = (lineGouraud >> 16) & 0xFF;
            U8 shadedColor = PtrCLUTGouraud[(gouraudIntensity << 8) + texel];
            
            *line = shadedColor;
            // Note: No Z-buffer write for NZW variant
          }
        }
        
        line++;
        zbuffer++;
        lineU += Fill_MapU_XSlope;
        lineV += Fill_MapV_XSlope;
        lineGouraud += Fill_Gouraud_XSlope;
        lineZ += Fill_Z_XSlope;
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    gouraud += Fill_Gouraud_LeftSlope;
    z += Fill_Z_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurGouraudMin = gouraud;
  Fill_CurZMin = z;
  
  return 0;
}
