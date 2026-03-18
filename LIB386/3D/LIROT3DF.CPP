#include <3D/LIROT3D.H>

#include <math.h>
#include <3D/CAMERA.H>
#include <SYSTEM/DEBUG_TRACES.H>

/*
 * Inverse rotation: multiply (x,y,z) by the transpose of Mat.
 *
 * The ASM uses x87 which keeps 80-bit extended precision through the
 * fmul/fadd chain, then fistp rounds to integer.  We match this by
 * promoting to long double (80-bit on x86) and using lrintl.
 */
void LongInverseRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z)
{
	LIB386_TRACE_CPP("LongInverseRotatePointF", "1", "Mat=%p x=%d y=%d z=%d", (void*)Mat, x, y, z);
	X0 = (S32)lrintl((long double)Mat->F.M11 * x + (long double)Mat->F.M21 * y + (long double)Mat->F.M31 * z);
	Y0 = (S32)lrintl((long double)Mat->F.M12 * x + (long double)Mat->F.M22 * y + (long double)Mat->F.M32 * z);
	Z0 = (S32)lrintl((long double)Mat->F.M13 * x + (long double)Mat->F.M23 * y + (long double)Mat->F.M33 * z);
	LIB386_TRACE_CPP("LongInverseRotatePointF", "2", "X0=%d Y0=%d Z0=%d", X0, Y0, Z0);
}

Func_LongInverseRotatePoint *LongInverseRotatePoint = LongInverseRotatePointF;
