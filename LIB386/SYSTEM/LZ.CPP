#include <SYSTEM/LZ.H>
#include <stdio.h>
#include <stdlib.h>

void	ExpandLZ(void *Dst, void *Src, U32 DecompSize, U32 MinBloc)
{
	U8 *dst = (U8 *)Dst;
	U8 *src = (U8 *)Src;
	U32 srcOffset = 0;
	U32 dstOffset = 0;

	while (dstOffset < DecompSize)
	{
		// Check the next flag bit
		U8 flag = src[srcOffset++];
		for (int i = 0; i < 8; i++)
		{
			if (dstOffset >= DecompSize)
			{
				break;
			}
			if ((flag & 0x1) != 0)
			{
				// Literal byte
				dst[dstOffset++] = src[srcOffset++];
			}
			else
			{
				// Back-reference
				U32 blockLength = (src[srcOffset] & 0x0f) + MinBloc;
				U32 blockOffset = ((U32)(src[srcOffset + 1]) << 4) | (src[srcOffset] >> 4);

				for (U32 j = 0; j < blockLength; j++)
				{
					dst[dstOffset] = dst[dstOffset - blockOffset - 1];
					dstOffset++;

					if (dstOffset >= DecompSize)
					{
						break;
					}
				}
				srcOffset += 2;
			}
			flag >>= 1;
		}
	}
}
