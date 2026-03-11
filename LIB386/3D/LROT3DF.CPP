#include <3D/ROT3D.H>

#include <math.h>
#include <3D/CAMERA.H>

/*
 * 3D rotation matching ASM LongRotatePointF.
 * Uses long double + lrintl to match x87 fistp round-to-nearest-even.
 */
void LongRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z)
{
	X0 = (S32)lrintl((long double)x * Mat->F.M11 + (long double)y * Mat->F.M12 + (long double)z * Mat->F.M13);
	Y0 = (S32)lrintl((long double)x * Mat->F.M21 + (long double)y * Mat->F.M22 + (long double)z * Mat->F.M23);
	Z0 = (S32)lrintl((long double)x * Mat->F.M31 + (long double)y * Mat->F.M32 + (long double)z * Mat->F.M33);
}

Func_RotatePoint *RotatePoint = LongRotatePointF;
Func_RotatePoint *RotatePointNoMMX = LongRotatePointF;
Func_LongRotatePoint *LongRotatePoint = LongRotatePointF;
