#include "BOXZBUF.H"

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>

S32 IncZ = 0;

S32	ZBufBoxOverWrite2(S32 z0, S32 z1,
		S32 xmin, S32 ymin,
		S32 xmax, S32 ymax
) {
  S32 deltaY = ymax - ymin + 1;
  S32 deltaX = xmax - xmin + 1;
  S32 scaledZ0 = Fill_ZBuffer_Factor * z0;
  S32 scaledZ1 = Fill_ZBuffer_Factor * z1;
  S32 deltaZBuf = scaledZ1 - scaledZ0;
  S64 zBufDivisor = (((S64)deltaZBuf >> 31) << 32) | (S64)deltaZBuf;
  IncZ = zBufDivisor / deltaY;
  U32 startOffset = TabOffLine[ymin] + xmin + deltaX;
  U8 *dstLog = (U8 *)Log + startOffset;
  U16 *dstZBuf = PtrZBuffer + startOffset;
  U8 *dstScreen = (U8 *)Screen + startOffset;
  S32 i = -deltaX;
  U8 flag = 1;

  do
  {
    U32 z0High = scaledZ0;
    U32 z0Low = scaledZ0 >> 16;

    while (1)
    {
      // Read ZBuffer (no AGI)
      U16 z = dstZBuf[i];
      // Is our pixel hidden?
      if (z >= z0Low)
      {
        break;
      }
      dstLog[i] = dstScreen[i]; // No: Copy it
      i++;
      if (i > 0)
      {
        break;
      }
    }

    if (i <= 0)
    {
      flag = 0;
      while (1)
      {
        i++;
        if (i > 0)
        {
          break;
        }
        U16 z = dstZBuf[i];
        if (z < z0Low)
        {
          dstLog[i] = dstScreen[i];
        }
      }
    }

    i = -deltaX;
    dstScreen += ScreenPitch;
    dstLog += ScreenPitch;
    dstZBuf += 2 * ScreenPitch;
    scaledZ0 = IncZ + z0High;
    deltaY--;
  }
  while (deltaY != 1);

  return flag;
}
