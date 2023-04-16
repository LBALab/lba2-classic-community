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
  // TODO: revisit and clean this up when characters are rendered
  // This function is supposed (I think) to make items in indoor levels to
  // be drawn on top of the character.
  Struc_Mask_Header *mask; // esi@1
  U8 *v4;                  // ebp@1
  U32 xMin;                // ebx@1
  U32 yMin;                // ecx@1
  U8 *maskData;            // esi@1
  U32 xMax;                // edx@1
  U32 yMax;                // eax@1
  int v12;                 // edx@5
  int v13;                 // ebx@5
  U8 *v14;                 // edi@5
  U8 *v15;                 // ebp@5
  char v16;                // bh@5
  int v17;                 // edx@5
  int v18;                 // ecx@5
  char v19;                // bl@6
  char v21;                // al@9
  U8 v22;                  // of@9
  U32 NbPix;               // edi@27
  U8 *pixSrc;              // esi@29
  U8 *v29;                 // ebp@29
  int offFirstPix;         // edi@29
  U8 *pixDst;              // edi@29
  int v33;                 // edx@29
  U8 v34;                  // bl@30
  U8 v36;                  // bh@30
  unsigned int v37;        // ecx@30
  U8 v38;                  // ah@30
  U8 *v39;                 // edi@30
  U8 *v40;                 // esi@30
  U8 v41;                  // al@31
  bool v42;                // cf@31
  U8 v43;                  // ah@31
  U8 v44;                  // al@34
  bool v45;                // cf@34

  U8 something1 = 0;
  U8 something2 = 0;
  U32 screenWidth = TabOffLine[1];

  mask = (Struc_Mask_Header *)(bankmask + ((U32 *)bankmask)[nummask]);
  v4 = (U8 *)src;
  xMin = mask->HotX + x;
  yMin = mask->HotY + y;
  maskData = (U8 *)mask + sizeof(Struc_Mask_Header); // Skip header

  // Test Clipping
  xMax = mask->DeltaX + xMin - 1;
  yMax = mask->DeltaY + yMin - 1;
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
      yMin = ClipYMin - yMin;
      do
      {
        // NextH:
        maskData += *maskData + 1;
        --yMin;
      } while (yMin != 0);
      yMin = ClipYMin;
    }
    // Clipping bottom
    // PasHaut:
    if (yMax > ClipYMax)
    {
      yMax = ClipYMax;
    }
    // Clipping left
    // PasBas:
    S32 OffsetBegin = 0;
    if (xMin < ClipXMin)
    {
      OffsetBegin = ClipXMin - xMin;
    }
    // Clipping right
    // PasGauche:
    NbPix = xMax - xMin - OffsetBegin + 1;
    if (xMax > ClipXMax)
    {
      NbPix -= xMax - ClipXMax;
      xMax = ClipXMax;
    }
    // PasDroit:
    v29 = maskData;
    offFirstPix = TabOffLine[yMin] + xMin;
    pixSrc = v4 + offFirstPix;
    pixDst = (U8 *)Log + offFirstPix;
    v33 = yMax - yMin + 1;
    do
    {
      // l1:
      v34 = *v29; // nb blocks
      v29++;
      v36 = NbPix;                // nb points to copy
      v37 = OffsetBegin;          // start x
      v38 = OffsetBegin & 0xFF;   // nb pix count
      v39 = pixDst + OffsetBegin; // new pos screen
      v40 = pixSrc + OffsetBegin;
      while (1)
      {
        // l0:
        v41 = *v29; // nb pix to jump
        v29++;
        --v34;
        v42 = v38 < v41;
        v43 = v38 - v41;
        if (v43 == 0)
        {
          // falls flat
          goto LABEL_OKDRAW;
        }

        if (v42)
        {
          // rptj:
          v43 = v43;
          v37 = v43; // nb pix to jump
          something1 = v36 <= v43;
          v36 -= v43; // remains to write
          if (something1)
          {
            v39 += v37;
            v40 += v37;
            goto LABEL_OKDRAW;
          }
          goto LABEL_APLUS;
        }
        if (v34 == 0)
        {
          goto LABEL_APLUS;
        }

        v44 = *v29; // nb copy point
        v29++;
        --v34;
        v45 = v43 < v44;
        v38 = v43 - v44;
        if (v38 == 0)
        {
          goto LABEL_OKJUMP;
        }
        if (v45)
        {
          break;
        }
        if (v34 == 0)
        {
          goto LABEL_APLUS;
        }
      }
      // rgtc:
      v38 = -v38; // nb pix to copy
      v37 = v38;
      something2 = v36 < v38;
      v36 -= v38;
      if (something2)
      {
        // don't copy everything
        v37 += v36;
        memcpy(v39, v40, v37);
        // No need to advance v39 and v40
        break;
      }
      memcpy(v39, v40, v37);
      v40 += v37;
      v39 += v37;
      v37 = 0;
      if (v36 == 0)
      {
        goto LABEL_APLUS;
      }
    LABEL_OKJUMP:
      while (2)
      {
        v37 = *v29; // nb pix to jump
        v29++;
        --v34;
        if (v34 == 0)
        {
          break;
        }
        v22 = v36 <= v37;
        v36 -= v37;
        if (v22)
        {
          break;
        }
        v39 += v37;
        v40 += v37;
      LABEL_OKDRAW:
        v37 = *v29; // nb pix to copy
        v29++;
        --v34;
        v42 = v36 < v37;
        v36 -= v37;
        if (v42)
        {
          v37 += v36;
          memcpy(v39, v40, v37);
          // No need to advance v39 and v40
          break;
        }
        memcpy(v39, v40, v37);
        v40 += v37;
        v39 += v37;
        v37 = 0;
        if (v36 != 0 && v34)
        {
          continue;
        }
        break;
      }
    LABEL_APLUS:
      yMax = v34;
      pixDst += screenWidth;
      v29 += v34;
      pixSrc += screenWidth;
      v33--;
    } while (v33);
  }
  else
  {
    // Calculate Offset Screen
    v12 = xMax - xMin + 1; // (deltaX)
    v13 = TabOffLine[yMin] + xMin;
    v14 = (U8 *)Log + v13;
    v15 = (U8 *)v4 + v13;
    v16 = yMax - yMin + 1; // NbLine (deltaY)
    v17 = screenWidth - v12;
    v18 = 0; // counter
    yMax = 0;
    do
    {
      // NextLine:
      v19 = *maskData; // Nb Block for this line
      maskData++;
      do
      {
        // SameLine:
        yMax = *maskData; // Nb Zero to Jump
        maskData++;
        v14 += yMax; // Incrust on Log
        v15 += yMax; // And on PtSrc
        v19--;
        if (v19 == 0)
        {
          break;
        }
        v18 = *maskData; // Nb Zero to Write
        maskData++;
        do
        {
          // loopb:
          *v14 = *v15;
          v15++;
          v14++;
          --v18;
        } while (v18);
        v19--;
      } while (v19);
      v14 += v17;
      v15 += v17;
      --v16;
    } while (v16);
  }
}
