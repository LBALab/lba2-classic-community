#include <3D/LIROT3D.H>

#include <3D/CAMERA.H>
#include <SYSTEM/UTILS.H>

void LongInverseRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z)
{
	X0 = ROUND(Mat->F.M11 * x + Mat->F.M21 * y + Mat->F.M31 * z);
	Y0 = ROUND(Mat->F.M12 * x + Mat->F.M22 * y + Mat->F.M32 * z);
	Z0 = ROUND(Mat->F.M13 * x + Mat->F.M23 * y + Mat->F.M33 * z);
}

Func_LongInverseRotatePoint *LongInverseRotatePoint = LongInverseRotatePointF;
