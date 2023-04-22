#include <SVGA/SCALEBOX.H>

#include <SVGA/SCREEN.H>

void ScaleBox(S32 xs0, S32 ys0, S32 xs1, S32 ys1, void *ptrs, S32 xd0, S32 yd0,
              S32 xd1, S32 yd1, void *ptrd)
{
  U32 deltaX = xd1 - xd0 + 1;
  U32 deltaY = yd1 - yd0 + 1;
  U32 offsetLine = ModeDesiredX - deltaX;
  U32 srcXStride = ((xs1 - xs0 + 1) << 16) / deltaX;
  U32 srcXStrideH = srcXStride << 16;
  U32 poidFort = srcXStride >> 16;
  U8 *dst = (U8 *)ptrd + TabOffLine[yd0] + xd0;
  U8 *src = (U8 *)ptrs + TabOffLine[ys0] + xs0;
  U32 srcYStride = ((ys1 - ys0 + 1) << 16) / (yd1 - yd0 + 1);
  U32 virguleH = srcYStride << 16;
  U32 poidFortH = srcYStride >> 16;
  U32 accuVirguleH = 0;
  U8 *baseSrc = src;
  do
  {
    U32 x = deltaX;
    U32 strideAcc = 0;
    do
    {
      *dst = *src;
      U8 carry = (U64)strideAcc + (U64)srcXStrideH > 0xFFFFFFFF;
      strideAcc += srcXStrideH;
      src += poidFort + carry;
      ++dst;
      --x;
    }
    while (x);
    U8 carry = (U64)accuVirguleH + (U64)virguleH > 0xFFFFFFFF;
    accuVirguleH += virguleH;
    U32 nextY = poidFortH + carry;
    dst += offsetLine;
    src = baseSrc;
    baseSrc += TabOffLine[nextY];
    --deltaY;
  }
  while (deltaY);
}
