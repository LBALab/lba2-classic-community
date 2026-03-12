#include <SVGA/CPYBLOCI.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <stdio.h>

void CopyBlockIncrust(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd,
                      void *dst)
{
  // Initial bounds check
  if (x0 > ClipXMax || y0 > ClipYMax) return;
  
  // Clip source coordinates to screen bounds
  S32 clipDeltaX = ClipXMin - x0;
  if (clipDeltaX > 0) {
    x0 = ClipXMin;
    xd += clipDeltaX;
  }
  
  S32 clipDeltaY = ClipYMin - y0;
  if (clipDeltaY > 0) {
    y0 = ClipYMin;
    yd += clipDeltaY;
  }
  
  if (x1 > ClipXMax) x1 = ClipXMax;
  if (y1 > ClipYMax) y1 = ClipYMax;
  
  // Calculate dimensions
  S32 width = x1 - x0;
  if (width < 0) return;
  
  S32 height = y1 - y0;
  if (height < 0) return;
  
  // Check destination bounds
  if (xd < ClipXMin || xd > ClipXMax) return;
  if (yd < ClipYMin || yd > ClipYMax) return;
  
  // Clip destination if it would go out of bounds
  S32 destClipX = (xd + width) - ClipXMax;
  if (destClipX > 0) {
    width -= destClipX;
    if (width <= 0) return;
  }
  
  S32 destClipY = (yd + height) - ClipYMax;
  if (destClipY > 0) {
    height -= destClipY;
    if (height <= 0) return;
  }
  
  // Calculate source and destination pointers
  U8 *srcPtr = (U8 *)src + TabOffLine[y0] + x0;
  U8 *dstPtr = (U8 *)dst + TabOffLine[yd] + xd;
  
  U32 scanLine = TabOffLine[1];  // Screen width
  
  // Copy with transparency (skip pixels with value 0)
  for (S32 y = 0; y <= height; y++) {
    for (S32 x = 0; x <= width; x++) {
      U8 pixel = srcPtr[x];
      if (pixel != 0) {  // Skip transparent pixels
        dstPtr[x] = pixel;
      }
    }
    srcPtr += scanLine;
    dstPtr += scanLine;
  }
}
