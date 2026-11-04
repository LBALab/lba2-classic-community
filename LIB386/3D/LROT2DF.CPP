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
 * Double + lrint matches x87 fistp round-to-nearest-even: the table entries
 * are floats in [-1,1] (24-bit mantissa) and the coordinates are small
 * world-space S32 values (WORLD_SIZE = 2^15), so every product and their
 * sum stay under the 53-bit double mantissa — exact, i.e. the same value
 * the 80-bit x87 evaluation produced, without ARM64's software long double.
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

    double cosA = (double)CosTabF[angle];
    double sinA = (double)SinTabF[angle];

    X0 = (S32)lrint((double)x * cosA + (double)z * sinA);
    Z0 = (S32)lrint((double)z * cosA - (double)x * sinA);
    LIB386_TRACE_CPP("LongRotateF", "2", "X0=%d Z0=%d", X0, Z0);
}
