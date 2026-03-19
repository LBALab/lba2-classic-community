#include <3D/ROT2D.H>

#include <math.h>
#include <3D/SINTABF.H>
#include <3D/CAMERA.H>
#include <SYSTEM/DEBUG_TRACES.H>

void Rotate(S32 x, S32 z, S32 angle) {
    LongRotate(x, z, angle);
}

/*
 * 2D rotation matching ASM LongRotateF.
 * Uses long double + lrintl to match x87 fistp round-to-nearest-even.
 */
void LongRotate(S32 x, S32 z, S32 angle) {
    LIB386_TRACE_CPP("LongRotateF", "1", "x=%d z=%d angle=%d", x, z, angle);
    X0 = x;
    Z0 = z;

    angle &= 4095;

    if (angle == 0) {
        LIB386_TRACE_CPP("LongRotateF", "2", "X0=%d Z0=%d", X0, Z0);
        return;
    }

    long double cosA = (long double)CosTabF[angle];
    long double sinA = (long double)SinTabF[angle];

    X0 = (S32)lrintl((long double)x * cosA + (long double)z * sinA);
    Z0 = (S32)lrintl((long double)z * cosA - (long double)x * sinA);
    LIB386_TRACE_CPP("LongRotateF", "2", "X0=%d Z0=%d", X0, Z0);
}
