#include <SVGA/CPYBLOCK.H>

#include <memory.h>

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/DEBUG_TRACES.H>

void CopyBlock(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd,
               void *dst)
{
    LIB386_TRACE_CPP("CopyBlock", "1", "x0=%d y0=%d x1=%d y1=%d src=%p xd=%d yd=%d dst=%p", x0, y0, x1, y1, src, xd, yd, dst);
  if (x0 > ClipXMax || y0 > ClipYMax)
  {
    return;
  }

  if (ClipXMin > x0)
  {
    x0 = ClipXMin;
    xd += ClipXMin - x0;
  }

  if (ClipYMin > y0)
  {
    y0 = ClipYMin;
    yd += ClipYMin - y0;
  }

  if (x1 > ClipXMax)
  {
    x1 = ClipXMax;
  }

  if (y1 > ClipYMax)
  {
    y1 = ClipYMax;
  }

  U32 width = x1 - x0;
  if (x0 > x1)
  {
    return;
  }
  U32 height = y1 - y0;
  if (y0 > y1)
  {
    return;
  }
  if (xd < ClipXMin)
  {
    return;
  }
  if (xd > ClipXMax)
  {
    return;
  }
  if (yd < ClipYMin)
  {
    return;
  }
  if (yd > ClipYMax)
  {
    return;
  }
  S32 distanceToXMax = width + xd - ClipXMax;
  if (width + xd > ClipXMax)
  {
    if (width <= distanceToXMax)
    {
      return;
    }
    width -= distanceToXMax;
  }
  S32 distanceToYMax = height + yd - ClipYMax;
  if (height + yd > ClipYMax)
  {
    if (height <= distanceToYMax)
    {
      return;
    }
    height -= distanceToYMax;
  }
  // Offset Src
  U8 *srcOffset = (U8 *)(TabOffLine[y0] + x0 + (U8 *)src);
  // Delta X
  U32 deltaX = width + 1;
  // Offset Dst
  U8 *dstOffset = (U8 *)(TabOffLine[yd] + xd + (U8 *)dst);
  // Delta Y
  U32 deltaY = height + 1;
  U32 screenOffset = TabOffLine[1] - (width + 1);

  do
  {
    memcpy(dstOffset, srcOffset, deltaX);
    srcOffset += screenOffset + deltaX;
    dstOffset += screenOffset + deltaX;
    --deltaY;
  }
  while (deltaY);
}
