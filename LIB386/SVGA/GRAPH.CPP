#include <SVGA/GRAPH.H>

#include <SVGA/SCREENXY.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

/*
;----------------------------------------------------------------------------
;               DWORD   TabOffset[]     ( Un de plus pointe fin de fichier )
;
;                       Brick:
;
;                           - BYTE Delta X
;                           - BYTE Delta Y
;                           - BYTE HotX (Signed)
;                           - BYTE HotY (Signed)
;
;                               Pour chaque Ligne ( Delta Y ):
;
;                               - BYTE NbBlock
;
;                                  Par Block:
;                                   - 00xxxxxxb  xxxxxx zeros to jump
;                                   - 01xxxxxxb  xxxxxx Copy Pix
;                                   - 10xxxxxxb  xxxxxx Repeat Pix
;                                   - BYTE datas[]

;----------------------------------------------------------------------------
*/
// OffsetBegin	dd	0
// NbPix		dd	0
U8 BufferClip[512];

// -----------------------------------------------------------------------------
S32 GetDxDyGraph(U8 numgraph, S32 *x, S32 *y, const void *bankgraph) {
  const U32 *graphMem = (const U32 *)bankgraph;
  const U8 *graphBytes = (const U8 *)bankgraph;

  // Don't know if the value is really a signed int, but works for now
  U32 offset = graphMem[numgraph];

  *x = (graphBytes[offset] & 0xFF);
  *y = (graphBytes[offset + 1] & 0xFF);

  return 0; // TODO: Remove, make void
}

#include <stdio.h>

S32 ClippingGraph(S32 x0, S32 y0, S32 x1, S32 y1, U8 *graphics);

S32 AffGraph(S32 numgraph, S32 x, S32 y, const void *bankgraph)
{
  U8 *data = (U8 *)bankgraph + ((U32 *)bankgraph)[numgraph];
  U8 deltaX = data[0];
  U8 nbLine = data[1]; // delta Y
  S8 hotX = data[2];
  S8 hotY = data[3];

  U8 *graphics = data + 4;

// ;-------------------- Test Clipping
  S32 x0 = ScreenXMin = x + hotX;
  S32 y0 = ScreenYMin = y + hotY;
  S32 x1 = ScreenXMax = (x + hotX) + deltaX - 1;
  S32 y1 = ScreenYMax = (y + hotY) + nbLine - 1;

  if (x0 < ClipXMin || y0 < ClipYMin ||
      x1 > ClipXMax || y1 > ClipYMax)
  {
    return ClippingGraph(x0, y0, x1, y1, graphics);
  }

  x1++;
  y1++;
  x1 -= x0;
  y1 -= y0;

// ;-------------------- Calcul Offset Ecran
	U8 *screen = (U8 *)Log + TabOffLine[y0] + x0;
	U32 offsetScreen = TabOffLine[1] - x1;

	for (int i = 0; i < nbLine; i++)
	{
// ;-------------------- Init NbBlock for this line
		// NextLine:
		U8 nbBlock = graphics[0];
		graphics++;

		// ;-------------------- Manage One Line
		for (int j = 0; j < nbBlock; j++)
		{
			// SameLine:
			U8 control = graphics[0];
			U8 opCode = (control & 0b11000000) >> 6;
			U32 counter = control & 0b00111111;
			if (opCode == 0) // Incrust
			{
				// JumpZero:
				graphics++;
				screen = screen + counter + 1;
			}
			else
			{
				counter++; // One More Please...
				if (opCode == 1)
				{
					// jmp	WriteDiffPix	; Pix Differents
					graphics++;
					while (counter > 0)
					{
						*(screen++) = *(graphics++);
						counter--;
					}
				}
				else
				{
					// ;-------------------- Repeat Same Color
					U8 color = graphics[1]; // Color to Repeat
					graphics += 2;
					while (counter > 0)
					{
						*(screen++) = color;
						counter--;
					}
				}
			}
		}
		screen += offsetScreen;
	}

  return 0;
}

S32 ClippingGraph(S32 x0, S32 y0, S32 x1, S32 y1, U8 *graphics)
{
	if (x0 > ClipXMax || y0 > ClipYMax ||
		x1 < ClipXMin || y1 < ClipYMin)
	{
		return 0;
	}

	if (y0 < ClipYMin)
	{
		// ---------------------- Clipping Top, Skip ClipYMin-ECX Line(s)

		S32 yDiff = ClipYMin - y0;
		for (int i = 0; i < yDiff; i++)
		{
			// NextH:
			U8 nbBlock = graphics[0];
			graphics++;
			for (int j = 0; j < nbBlock; j++)
			{
				// SameH:
				U8 control = graphics[0];
				graphics++;
				U8 opCode = (control & 0b11000000) >> 6;
				if (opCode != 0)
				{
					U32 jump = 0;
					if (opCode == 1)
					{
						jump = control & 0b00111111;
					}
					graphics += jump + 1;
				}
			}
		}

		y0 = ClipYMin;
	}

	// ;---------------------- Clipping Bottom
	// PasHaut:
	if (y1 > ClipYMax)
	{
		y1 = ClipYMax;
	}

	// ;---------------------- Clipping Left
	// PasBas:
	U32 offset = 0;
	if (x0 < ClipXMin)
	{
		offset = ClipXMin - x0;
	}

	// PasGauche:
	U32 OffsetBegin = offset;
	// ;---------------------- Clipping Right
	offset += x0 - 1;
	offset -= x1;
	offset = -offset;
	U32 NbPix = offset;
	x1 -= ClipXMax;

	if (x1 > 0)
	{
		NbPix -= x1;
	}

	// PasDroit:
	U8 *screen = (U8 *)Log + TabOffLine[y0] + x0;
	S32 deltaY = y1 - y0;
	for (int i = 0; i <= deltaY; i++)
	{
		U8 *buffer = BufferClip;
		U8 nbBlock = graphics[0];
		graphics++;

		for (int j = 0; j < nbBlock; j++)
		{
			U8 control = graphics[0];
			graphics++;
			U8 opCode = (control & 0b11000000) >> 6;
			U32 counter = control & 0b00111111;
			counter++;
			if (opCode == 0)
			{
				// jmp JumpL
				while (counter > 0)
				{
					*(buffer++) = 0;
					counter--;
				}
			}
			else if (opCode == 1)
			{
				// jne	WriteDiffL
				while (counter > 0)
				{
					*(buffer++) = *(graphics++);
					counter--;
				}
			}
			else
			{
				U8 color = graphics[0];
				graphics++;
				while (counter > 0)
				{
					*(buffer++) = color;
					counter--;
				}
			}
		}

		U8 *src = BufferClip + OffsetBegin;
		U8 *dst = screen + OffsetBegin;
		for (int j = 0; j < NbPix; j++)
		{
			U8 value = *(src++);
			if (value != 0)
			{
				*dst = value;
			}
			dst++;
		}

		screen += TabOffLine[1];
	}

	return 0; // TODO: Remove, make void
}

S32 GetBoxGraph(S32 numgraph, S32 *x0, S32 *y0, S32 *x1, S32 *y1,
                const void *bankgraph)
{
	U8 *bank = (U8 *)bankgraph + ((U32 *)bankgraph)[numgraph];

	U8 dx = bank[0];
	U8 dy = bank[1];
	U8 hotX = bank[2];
	U8 hotY = bank[3];

	*x0 = hotX;
	*y0 = hotY;
	*x1 = hotX + dx;
	*y1 = hotY + dy;

  return 0;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
