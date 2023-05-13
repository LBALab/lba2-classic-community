#include "BOXZBUF.H"

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>

#include <stdio.h>

S32 IncZ = 0;

// Verified?
S32	ZBufBoxOverWrite2(S32 z0, S32 z1,
		S32 xmin, S32 ymin,
		S32 xmax, S32 ymax
) {
  S32 deltaY = ymax - ymin + 1; // eax
  S32 deltaX = xmax - xmin + 1; // ebx
  S32 scaledZ0 = Fill_ZBuffer_Factor * z0; // esi
  S32 scaledZ1 = Fill_ZBuffer_Factor * z1; // edi
  S32 deltaZBuf = scaledZ1 - scaledZ0;
  S64 zBufDividend = (((S64)deltaZBuf >> 31) << 32) | (S64)deltaZBuf;
  IncZ = zBufDividend / deltaY;
  U32 startOffset = TabOffLine[ymin] + xmin + deltaX; // edx
  U8 *dstLog = (U8 *)Log + startOffset; // ebp
  U16 *dstZBuf = PtrZBuffer + startOffset; // edi
  U8 *dstScreen = (U8 *)Screen + startOffset; // edx
  S32 i = -deltaX;
  U8 flag = 1;

  do
  {
    U32 z0Int = scaledZ0 >> 16; // esi

    while (1)
    {
      // Read ZBuffer (no AGI)
      U16 z = dstZBuf[i];

      // Is our pixel hidden?
      if (z >= z0Int)
      {
        flag = 0;

        while (1)
        {
          i++;
          if (i > 0)
          {
            break;
          }

          // Read ZBuffer (no AGI)
          U16 z = dstZBuf[i];

          // Is our pixel hidden?
          if (z < z0Int)
          {
            dstLog[i] = dstScreen[i]; // No: Copy it
          }
        }

        break;
      }

      dstLog[i] = dstScreen[i]; // No: Copy it
      i++;
      if (i > 0)
      {
        break;
      }
    }

    i = -deltaX;
    dstScreen += ScreenPitch;
    dstLog += ScreenPitch;
    dstZBuf += ScreenPitch;
    scaledZ0 += IncZ;
    deltaY--;
  }
  while (deltaY != 0);

  return flag;
}
