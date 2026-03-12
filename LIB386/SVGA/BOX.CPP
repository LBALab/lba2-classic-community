#include <SVGA/BOX.H>

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

void Box(S32 x0, S32 y0, S32 x1, S32 y1, S32 col)
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

	U32 screenWidth = TabOffLine[1];
	U32 deltaY = y1 - y0;
	U32 deltaX = x1 - x0;
	U32 deltaScreen = screenWidth - deltaX;
	U32 offset = TabOffLine[y0] + x0;

	for (int j = 0; j <= deltaY; j++)
	{
		U32 yOffset = offset + j * screenWidth;

		for (int i = 0; i <= deltaX; i++)
		{
			((U8 *)Log)[yOffset + i] = col;
		}
	}
}
