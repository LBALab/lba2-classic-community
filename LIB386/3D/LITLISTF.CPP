#include <3D/LITLISTF.H>

#include <math.h>
#include <3D/LIGHT.H>
#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>

/*
 * Per-vertex lighting.  Matches ASM LightListF which uses x87 fistp
 * for rounding and clamps negative values to 0.
 *
 * Uses long double + lrintl to match fistp round-to-nearest-even.
 */
void LightList(TYPE_MAT *Mat, U16 *dst, TYPE_VT16 *src, S32 n)
{
	if (n == 0)
		return;

	LongInverseRotatePoint(Mat, CameraXLight, CameraYLight, CameraZLight);

	for (S32 i = 0; i < n; i++)
	{
		long double dot = (long double)X0 * src[i].X
		                + (long double)Y0 * src[i].Y
		                + (long double)Z0 * src[i].Z;
		S32 value = (S32)lrintl(dot * (long double)FactorLight);
		dst[i] = (value < 0) ? 0 : (U16)value;
	}
}
