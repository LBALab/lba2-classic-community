#include <POLYGON/POLYTZF.H>

#include <stdio.h>
#include <FILLER.H>
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/UTILS.H>

// External variables from POLY.CPP
extern U32 Fill_Fog_Factor;
extern U32 Fill_ScaledFogNear;

S32 Filler_TextureZFogSmooth(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  S32 u = Fill_CurMapUMin;
  S32 v = Fill_CurMapVMin;
  S32 w = Fill_CurWMin;
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
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      
      S32 lineU = u;
      S32 lineV = v;
      S32 lineW = w;

      // Simple texture mapping with fog
      for (U32 x = 0; x < fillWidth; x++)
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
        
        *line++ = finalColor;
        
        lineU += Fill_MapU_XSlope;
        lineV += Fill_MapV_XSlope;
        lineW += Fill_W_XSlope;
      }
    }

    if (--diffY == 0) break;

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
    w += Fill_W_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  Fill_CurWMin = w;
  
  return 0;
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
