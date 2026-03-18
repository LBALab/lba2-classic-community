#include <SVGA/RESBLOCK.H>

#include <string.h>

#include <SVGA/SCREEN.H>
#include <SYSTEM/DEBUG_TRACES.H>

void RestoreBlock(void *screen, void *buffer, S32 x0, S32 y0, S32 x1, S32 y1)
{
    LIB386_TRACE_CPP("RestoreBlock", "1", "scr=%p buf=%p x0=%d y0=%d x1=%d y1=%d", screen, buffer, x0, y0, x1, y1);
  U8 *src = (U8 *)buffer;
  U8 *dst = (U8 *)screen + TabOffLine[y0] + x0;
  U32 deltaY = y1 - y0 + 1;
  U32 deltaX = x1 - x0 + 1;
  U32 lineOffset = TabOffLine[1] - deltaX;

  do
  {
    memcpy(dst, src, deltaX);
    src += deltaX;
    dst += deltaX + lineOffset;
    --deltaY;
  }
  while (deltaY);
}
