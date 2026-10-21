#include <3D/ROT3D.H>

#include <3D/X87REAL.H>
#include <3D/CAMERA.H>
#include <SYSTEM/DEBUG_TRACES.H>

/*
 * 3D rotation matching ASM LongRotatePointF.
 * X87_REAL + X87Round match the x87 chain and its fistp (3D/X87REAL.H).
 */
void LongRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z) {
    LIB386_TRACE_CPP("LongRotatePointF", "1", "Mat=%p x=%d y=%d z=%d", (void *)Mat, x, y, z);
    X0 = (S32)X87Round((X87_REAL)x * Mat->F.M11 + (X87_REAL)y * Mat->F.M12 + (X87_REAL)z * Mat->F.M13);
    Y0 = (S32)X87Round((X87_REAL)x * Mat->F.M21 + (X87_REAL)y * Mat->F.M22 + (X87_REAL)z * Mat->F.M23);
    Z0 = (S32)X87Round((X87_REAL)x * Mat->F.M31 + (X87_REAL)y * Mat->F.M32 + (X87_REAL)z * Mat->F.M33);
    LIB386_TRACE_CPP("LongRotatePointF", "2", "X0=%d Y0=%d Z0=%d", X0, Y0, Z0);
}

Func_RotatePoint *RotatePoint = LongRotatePointF;
Func_RotatePoint *RotatePointNoMMX = LongRotatePointF;
Func_LongRotatePoint *LongRotatePoint = LongRotatePointF;
