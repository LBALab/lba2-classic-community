#include "FLOW.H"

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <POLYGON/POLY.H>
#include <stdio.h>

void BoxFlow(S32 x, S32 y, S32 coul)
{
  if (x >= ClipXMax || x < ClipXMin || y >= ClipYMax || y < ClipYMin)
  {
    return;
  }

  U8 *dst = (U8 *)Log + TabOffLine[y] + x;
  U32 screenWidth = TabOffLine[1];
  dst[0] = coul;
  dst[1] = coul;
  dst[screenWidth] = coul;
  dst[screenWidth + 1] = coul;
}

void ShadeBoxBlk(S32 x0, S32 y0, S32 x1, S32 y1, S32 deccoul)
{
  if (x0 > ClipXMax || x1 < ClipXMin || y0 > ClipYMax || y1 < ClipYMin)
  {
    return;
  }

  if (x0 < ClipXMin)
  {
    x0 = ClipXMin;
  }
  if (x1 > ClipXMax)
  {
    x1 = ClipXMax;
  }
  if (y0 < ClipYMin)
  {
    y0 = ClipYMin;
  }
  if (y1 > ClipYMax)
  {
    y1 = ClipYMax;
  }

  S32 deltaY = y1 - y0 + 1;
  S32 deltaX = x1 - x0 + 1;

  U8 *dst = (U8 *)Log + TabOffLine[y0] + x0;
  U32 screenWidth = TabOffLine[1];

  U32 clutOffset = 15 * 256;
  U8 clutOffsetHighByte = (clutOffset >> 8) & 0xFF;
  clutOffsetHighByte -= (U8)(deccoul & 0xFF);
  clutOffset = (clutOffset & 0xFFFF00FF) | (clutOffsetHighByte << 8);
  U8 *clut = PtrCLUTGouraud + clutOffset;

  for (S32 y = 0; y < deltaY; y++)
  {
    for (S32 x = 0; x < deltaX; x++)
    {
      dst[x] = clut[dst[x]];
    }
    dst += screenWidth;
  }
}

void	CopyBlockShade( S32 x0, S32 y0, S32 x1, S32 y1, U8 *src,
				S32 xd, S32 yd, U8 *dst, S32 shade )
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}
