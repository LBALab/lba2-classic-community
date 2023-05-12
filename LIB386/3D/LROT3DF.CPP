#include <3D/ROT3D.H>

#include <3D/CAMERA.H>
#include <SYSTEM/UTILS.H>

void LongRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z)
{
	X0 = ROUND(x * Mat->F.M11 + y * Mat->F.M12 + z * Mat->F.M13);
	Y0 = ROUND(x * Mat->F.M21 + y * Mat->F.M22 + z * Mat->F.M23);
	Z0 = ROUND(x * Mat->F.M31 + y * Mat->F.M32 + z * Mat->F.M33);
}

Func_RotatePoint *RotatePoint = LongRotatePointF;
Func_RotatePoint *RotatePointNoMMX = LongRotatePointF;
Func_LongRotatePoint *LongRotatePoint = LongRotatePointF;
