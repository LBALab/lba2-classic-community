#include <POLYGON/POLY.H>

#include <stdio.h>

#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>

S32 Z_Value = 0;
S32 Sph_YMin = 0;
S32 Sph_YMax = 0;
S32 Sph_XMin = 0;
S32 Sph_XMax = 0;
U8 Sph_Coul = 0;

typedef void (*Sphere_Filler_Func)(U32 screenY);

void Sph_Line_Flat(U32 screenY)
{
  U8* dst = (U8 *)Log + TabOffLine[screenY] + Sph_XMin;
  U32 nbCols = Sph_XMax - Sph_XMin;
  for (int i = 0; i < nbCols; ++i)
  {
    dst[i] = Sph_Coul;
  }
}

void Sph_Line_FlatZBuf(U32 screenY)
{
  U32 lineOffset = TabOffLine[screenY];
  
  U8 *screen = (U8 *)Log + lineOffset;
  U16 *zBuffer = (U16 *)((U8 *)PtrZBuffer + lineOffset * 2);
  
  S32 xMin = Sph_XMin;
  S32 xMax = Sph_XMax;
  
  screen += xMin;
  zBuffer += xMin;
  
  S32 width = xMax - xMin;
  if (width <= 0)
  {
    return;
  }
  
  U8 color = Sph_Coul;
  U16 zValue = Z_Value;
  
  do
  {
    if (*zBuffer >= zValue)
    {
      *zBuffer = zValue;
      *screen = color;
    }
    zBuffer++;
    screen++;
    width--;
  }
  while (width > 0);
}

void Sph_Line_FlatNZW(U32 screenY)
{
  U32 lineOffset = TabOffLine[screenY];
  
  U8 *screen = (U8 *)Log + lineOffset;
  U16 *zBuffer = (U16 *)((U8 *)PtrZBuffer + lineOffset * 2);
  
  S32 xMin = Sph_XMin;
  S32 xMax = Sph_XMax;
  
  screen += xMin;
  zBuffer += xMin;
  
  S32 width = xMax - xMin;
  if (width <= 0)
  {
    return;
  }
  
  U8 color = Sph_Coul;
  U16 zValue = Z_Value;
  
  do
  {
    if (*zBuffer >= zValue)
    {
      *screen = color; // NZW = No Z-Write
    }
    zBuffer++;
    screen++;
    width--;
  }
  while (width > 0);
}

void Sph_Line_Transp(U32 screenY)
{
  U32 lineOffset = TabOffLine[screenY];
  U8 *screen = (U8 *)Log + lineOffset;
  
  S32 xMin = Sph_XMin;
  S32 xMax = Sph_XMax;
  
  screen += xMin;
  
  S32 width = xMax - xMin;
  if (width <= 0)
  {
    return;
  }
  
  U8 colorLow = Sph_Coul & 0xF0;
  U16 colorBoth = (colorLow << 8) | colorLow;
  
  // Handle alignment for odd addresses
  if ((uintptr_t)screen & 1)
  {
    U8 pixel = *screen;
    pixel = (pixel & 0x0F) + colorLow;
    *screen = pixel;
    screen++;
    width--;
  }
  
  U32 remainder = width & 1;
  U32 pairCount = width >> 1;
  
  // Process 16-bit aligned pixels
  while (pairCount > 0)
  {
    U16 pixels = *(U16*)screen;
    pixels = (pixels & 0x0F0F) + colorBoth;
    *(U16*)screen = pixels;
    
    screen += 2;
    pairCount--;
  }
  
  // Handle remaining single pixel
  if (remainder)
  {
    U8 pixel = *screen;
    pixel = (pixel & 0x0F) + colorLow;
    *screen = pixel;
  }
}

void Fill_Sphere(S32 Type_Sphere, S32 Color_Sphere, S32 Centre_X, S32 Centre_Y, S32 Rayon, S32 zBufferValue)
{
  Sphere_Filler_Func Fill_Filler;
  
  // Select appropriate line drawing function based on flags and type
  if (Fill_Flag_ZBuffer)
  {
    Z_Value = zBufferValue;
    Fill_Filler = Sph_Line_FlatZBuf;
    
    if (Fill_Flag_NZW)
    {
      Fill_Filler = Sph_Line_FlatNZW;
    }
    
    // Transparent spheres always use transparency function (no Z-buffer)
    if (Type_Sphere == 2)
    {
      Fill_Filler = Sph_Line_Transp;
    }
  }
  else
  {
    Fill_Filler = Sph_Line_Flat;
    if (Type_Sphere == 2)
    {
      Fill_Filler = Sph_Line_Transp;
    }
  }
  
  // Handle color conversion for fog mode
  Color_Sphere &= 0xFF;
  if (Fill_Flag_Fog)
  {
    Color_Sphere = Fill_Logical_Palette[Color_Sphere];
  }
  Sph_Coul = Color_Sphere;
  
  // Bresenham-like circle algorithm
  S32 sum = -Rayon;
  S32 px = Rayon;
  S32 py = 0;
  S32 xc = Centre_X;
  S32 yc = Centre_Y;
  S32 clippingNeeded = 0;
  
  // Calculate sphere bounds and check clipping
  S32 yMin = yc - px;
  if (yMin > ClipYMax) return;
  if (yMin < ClipYMin)
  {
    clippingNeeded++;
    yMin = ClipYMin;
  }
  Sph_YMin = yMin;
  
  S32 yMax = yc + px;
  if (yMax < ClipYMin) return;
  if (yMax > ClipYMax)
  {
    clippingNeeded++;
    yMax = ClipYMax;
  }
  Sph_YMax = yMax;
  
  S32 xMax = xc + px;
  if (xMax < ClipXMin) return;
  if (xMax > ClipXMax)
  {
    clippingNeeded++;
  }
  
  S32 xMin = xc - px;
  if (xMin > ClipXMax) return;
  if (xMin < ClipXMin)
  {
    clippingNeeded++;
  }
  
  // Main circle drawing loop
  if (!clippingNeeded)
  {
    while (py <= px)
    {
      S32 screenY1 = yc + py;
      S32 screenY2 = yc - py;
      
      Sph_XMin = xc - px;
      Sph_XMax = xc + px;
      Fill_Filler(screenY1);
      Fill_Filler(screenY2);
      
      sum += py;
      if (sum >= 0)
      {
        S32 screenY3 = yc + px;
        S32 screenY4 = yc - px;
        
        Sph_XMin = xc - py;
        Sph_XMax = xc + py;
        Fill_Filler(screenY3);
        Fill_Filler(screenY4);
        
        px--;
        sum -= px;
      }
      py++;
    }
  }
  else
  {
    while (py <= px)
    {
      S32 screenY = yc + py;
      if (screenY <= Sph_YMax && screenY >= Sph_YMin)
      {
        S32 lineXMin = xc - px;
        S32 lineXMax = xc + px;
        
        if (lineXMin < ClipXMin)
          lineXMin = ClipXMin;
        else if (lineXMin > ClipXMax)
        {
          Sph_YMax = screenY - 1;
          goto skip1;
        }
        
        if (lineXMax > ClipXMax)
          lineXMax = ClipXMax;
        else if (lineXMax < ClipXMin)
        {
          Sph_YMax = screenY - 1;
          goto skip1;
        }
        
        Sph_XMin = lineXMin;
        Sph_XMax = lineXMax;
        Fill_Filler(screenY);
      }
skip1:
      
      screenY = yc - py;
      if (screenY <= Sph_YMax && screenY >= Sph_YMin)
      {
        S32 lineXMin = xc - px;
        S32 lineXMax = xc + px;
        
        if (lineXMin < ClipXMin)
          lineXMin = ClipXMin;
        else if (lineXMin > ClipXMax)
        {
          Sph_YMin = screenY + 1;
          goto skip2;
        }
        
        if (lineXMax > ClipXMax)
          lineXMax = ClipXMax;
        else if (lineXMax < ClipXMin)
        {
          Sph_YMin = screenY + 1;
          goto skip2;
        }
        
        Sph_XMin = lineXMin;
        Sph_XMax = lineXMax;
        Fill_Filler(screenY);
      }
skip2:
      
      sum += py;
      if (sum >= 0)
      {
        screenY = yc + px;
        if (screenY <= Sph_YMax && screenY >= Sph_YMin)
        {
          S32 lineXMin = xc - py;
          S32 lineXMax = xc + py;
          
          if (lineXMin < ClipXMin)
            lineXMin = ClipXMin;
          else if (lineXMin > ClipXMax)
          {
            Sph_YMax = screenY - 1;
            goto skip3;
          }
          
          if (lineXMax > ClipXMax)
            lineXMax = ClipXMax;
          else if (lineXMax < ClipXMin)
          {
            Sph_YMax = screenY - 1;
            goto skip3;
          }
          
          Sph_XMin = lineXMin;
          Sph_XMax = lineXMax;
          Fill_Filler(screenY);
        }
skip3:
        
        screenY = yc - px;
        if (screenY <= Sph_YMax && screenY >= Sph_YMin)
        {
          S32 lineXMin = xc - py;
          S32 lineXMax = xc + py;
          
          if (lineXMin < ClipXMin)
            lineXMin = ClipXMin;
          else if (lineXMin > ClipXMax)
          {
            Sph_YMin = screenY + 1;
            goto skip4;
          }
          
          if (lineXMax > ClipXMax)
            lineXMax = ClipXMax;
          else if (lineXMax < ClipXMin)
          {
            Sph_YMin = screenY + 1;
            goto skip4;
          }
          
          Sph_XMin = lineXMin;
          Sph_XMax = lineXMax;
          Fill_Filler(screenY);
        }
skip4:
        
        px--;
        sum -= px;
      }
      py++;
    }
    
    if (Sph_YMin >= Sph_YMax)
      return;
  }
}
