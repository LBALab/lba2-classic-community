#include <SVGA/SAVBLOCK.H>

#include <string.h>

#include <SVGA/SCREEN.H>

void SaveBlock(void *screen, void *buffer, S32 x0, S32 y0, S32 x1,
                      S32 y1)
{
  U8 *dst = (U8 *)buffer;
  U8 *src = (U8 *)screen + TabOffLine[y0] + x0;
  U32 deltaY = y1 - y0 + 1;
  U32 deltaX = x1 - x0 + 1;
  U32 lineOffset = TabOffLine[1] - deltaX;

  do
  {
    memcpy(dst, src, deltaX);
    dst += deltaX;
    src += deltaX + lineOffset;
    --deltaY;
  }
  while (deltaY);
}
