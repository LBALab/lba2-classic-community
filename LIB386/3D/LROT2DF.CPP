#include <3D/ROT2D.H>

#include <3D/X87REAL.H>
#include <3D/SINTABF.H>
#include <3D/CAMERA.H>
#include <SYSTEM/DEBUG_TRACES.H>

void Rotate(S32 x, S32 z, S32 angle) {
    LongRotate(x, z, angle);
}

/*
 * 2D rotation matching ASM LongRotateF.
 * X87_REAL + X87Round match the x87 chain and its fistp (3D/X87REAL.H).
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

    X87_REAL cosA = (X87_REAL)CosTabF[angle];
    X87_REAL sinA = (X87_REAL)SinTabF[angle];

    X0 = (S32)X87Round((X87_REAL)x * cosA + (X87_REAL)z * sinA);
    Z0 = (S32)X87Round((X87_REAL)z * cosA - (X87_REAL)x * sinA);
    LIB386_TRACE_CPP("LongRotateF", "2", "X0=%d Z0=%d", X0, Z0);
}
