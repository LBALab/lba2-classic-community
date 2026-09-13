#include <3D/ROT3D.H>

#include <math.h>
#include <3D/CAMERA.H>
#include <SYSTEM/DEBUG_TRACES.H>

/*
 * 3D rotation matching ASM LongRotatePointF.
 * Uses double + lrint to match x87 fistp round-to-nearest-even.
 */
void LongRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z) {
    LIB386_TRACE_CPP("LongRotatePointF", "1", "Mat=%p x=%d y=%d z=%d", (void *)Mat, x, y, z);
    X0 = (S32)lrint((double)x * Mat->F.M11 + (double)y * Mat->F.M12 + (double)z * Mat->F.M13);
    Y0 = (S32)lrint((double)x * Mat->F.M21 + (double)y * Mat->F.M22 + (double)z * Mat->F.M23);
    Z0 = (S32)lrint((double)x * Mat->F.M31 + (double)y * Mat->F.M32 + (double)z * Mat->F.M33);
    LIB386_TRACE_CPP("LongRotatePointF", "2", "X0=%d Y0=%d Z0=%d", X0, Y0, Z0);
}

Func_RotatePoint *RotatePoint = LongRotatePointF;
Func_RotatePoint *RotatePointNoMMX = LongRotatePointF;
Func_LongRotatePoint *LongRotatePoint = LongRotatePointF;
