#include <3D/LPROJ.H>
#include <3D/CAMERA.H>
#include <stdio.h>

S32 LongProjectPoint3D(S32 x, S32 y, S32 z)
{
  signed int result; // eax@2

  if (z > CameraZrClip)
  {
    result = 0;
    Xp = 2147483648;
    Yp = 2147483648;
  }
  else
  {
    Xp = XCentre + (signed int)((long double)(x - CameraXr) * (FRatioX / (long double)(CameraZr - z)));
    Yp = YCentre + y - CameraYr;
    result = 1;
  }

  return result;
}

Func_LongProjectPoint *LongProjectPoint = LongProjectPoint3D;
