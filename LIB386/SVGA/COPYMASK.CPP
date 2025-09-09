/*
    DWORD	TabOffset[]
      Brick:
          - BYTE Delta X
          - BYTE Delta Y
        Line(Delta Y):
        - BYTE NbBlock
           Block 0:	Nb Zero to Jump
           Block 1:	Nb Zero to Write
           Block 2:	Nb Zero to Jump
          etc...
*/

#include <SVGA/COPYMASK.H>

#include <string.h>

#include <SYSTEM/ADELINE.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

typedef struct {
	U8 DeltaX;
	U8 DeltaY;
	U8 HotX;
	U8 HotY;
} Struc_Mask_Header;

void CopyMask(S32 nummask, S32 x, S32 y, U8 *bankmask, void *src)
{
  U32 screenWidth = TabOffLine[1];

  if (y < 0)
    y = 0;

  Struc_Mask_Header *maskHeader = (Struc_Mask_Header *)(bankmask + ((U32 *)bankmask)[nummask]);
  U32 xMin = maskHeader->HotX + x;
  U32 yMin = maskHeader->HotY + y;
  U8 *maskData = (U8 *)maskHeader + sizeof(Struc_Mask_Header); // Skip header

  // Test Clipping
  U32 xMax = maskHeader->DeltaX + xMin - 1;
  U32 yMax = maskHeader->DeltaY + yMin - 1;

  U32 marginTop = 0;
  U32 marginLeft = 0;
  U32 marginRight = 0;
  U32 marginBottom = 0;

  if (xMin < ClipXMin || yMin < ClipYMin || xMax > ClipXMax || yMax > ClipYMax)
  {
    // ClippingMask:
    if (xMin > ClipXMax || yMin > ClipYMax || xMax < ClipXMin || yMax < ClipYMin)
    {
      return;
    }

    if (yMin < ClipYMin)
    {
      // Clipping top
      marginTop = ClipYMin - yMin;
    }
    // Clipping bottom
    // PasHaut:
    if (yMax > ClipYMax)
    {
      marginBottom = yMax - ClipYMax;
    }
    // Clipping left
    // PasBas:
    if (xMin < ClipXMin)
    {
      marginLeft = ClipXMin - xMin;
    }
    // Clipping right
    // PasGauche:
    if (xMax > ClipXMax)
    {
      marginRight = xMax - ClipXMax;
    }
  }

  // Calculate Offset Screen
  U32 deltaX = xMax - xMin + 1; // (deltaX)
  U32 initialOffset = TabOffLine[yMin] + xMin;
  U8 *screen = (U8 *)Log + initialOffset;
  U8 *source = (U8 *)src + initialOffset;
  U32 deltaY = yMax - yMin + 1; // NbLine (deltaY)
  U32 lineOffset = screenWidth - deltaX;
  for (U32 y = 0; y < deltaY; y++)
  {
    // NextLine:
    U8 numberOfBlocks = *maskData; // Nb Block for this line
    maskData++;
    U32 x = 0;

    do
    {
      // SameLine:
      U32 numberOfZeroToJump = *maskData; // Nb Zero to Jump
      maskData++;
      screen += numberOfZeroToJump; // Incrust on Log
      source += numberOfZeroToJump; // And on PtSrc
      numberOfBlocks--;
      if (numberOfBlocks == 0)
      {
        break;
      }

      x += numberOfZeroToJump;

      U32 numberOfPixelToCopy = *maskData; // Nb Zero to Write
      maskData++;
      do
      {
        // loopb:
        if (x >= marginLeft && x < deltaX - marginRight
            && y >= marginTop && y < deltaY - marginBottom)
        {
          *screen = *source;
        }
        x++;
        source++;
        screen++;
        --numberOfPixelToCopy;
      } while (numberOfPixelToCopy);
      numberOfBlocks--;
    } while (numberOfBlocks);

    screen += lineOffset;
    source += lineOffset;
  }
}
