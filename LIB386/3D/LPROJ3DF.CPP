#include <3D/LPROJ.H>

#include <3D/CAMERA.H>
#include <SYSTEM/UTILS.H>

S32 LongProjectPoint3D(S32 x, S32 y, S32 z)
{
  if (z > CameraZrClip)
  {
    Xp = -2147483648; // 0x80000000
    Yp = -2147483648;

    return 0;
  }

  long double factor = FRatioX / (long double)(CameraZr - z);
  Xp = XCentre + (S32)ROUND((long double)(x - CameraXr) * factor);
  Yp = YCentre + (S32)ROUND((long double)(y - CameraYr) * factor * FRatioY);

  return 1;
}

Func_LongProjectPoint *LongProjectPoint = LongProjectPoint3D;
