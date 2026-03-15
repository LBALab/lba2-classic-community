#include <3D/LPROJ.H>

#include <math.h>
#include <3D/CAMERA.H>
#include <SYSTEM/DEBUG_TRACES.H>

/*
 * Perspective projection of a 3D point onto the screen.
 *
 * The original ASM uses x87 extended precision throughout and fistp
 * (round-to-nearest-even) for the final integer conversion.  We match
 * this by using long double arithmetic and lrintl().
 */
S32 LongProjectPoint3D(S32 x, S32 y, S32 z)
{
  LIB386_TRACE_CPP("LongProjectPoint3DF", "1", "x=%d y=%d z=%d", x, y, z);
  if (z > CameraZrClip)
  {
    Xp = -2147483648; // 0x80000000
    Yp = -2147483648;

    LIB386_TRACE_CPP("LongProjectPoint3DF", "2", "ret=%d Xp=%d Yp=%d", 0, Xp, Yp);
    return 0;
  }

  long double factor = (long double)FRatioX / (long double)(CameraZr - z);

  Xp = XCentre + (S32)lrintl((long double)(x - CameraXr) * factor);
  Yp = YCentre + (S32)lrintl((long double)(y - CameraYr) * factor * (long double)FRatioY);

  LIB386_TRACE_CPP("LongProjectPoint3DF", "2", "ret=%d Xp=%d Yp=%d", 1, Xp, Yp);
  return 1;
}

Func_LongProjectPoint *LongProjectPoint = LongProjectPoint3D;
