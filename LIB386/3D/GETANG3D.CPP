#include <3D/GETANG3D.H>

#include <3D/CAMERA.H>
#include <3D/GETANG2D.H>
#include <3D/SQRROOT.H>
#include <SYSTEM/DEBUG_TRACES.H>

S32 GetAngleVector3D(S32 x, S32 y, S32 z)
{
	LIB386_TRACE_CPP("GetAngleVector3D", "1", "x=%d y=%d z=%d", x, y, z);
	S32 angleXZ = GetAngleVector2D(x, z);
	angleXZ = (~angleXZ + 2049) & 4095;
	Y0 = angleXZ;

	/* Full 64-bit products (matching ASM imul eax which is 32×32→64 signed) */
	S64 xz2 = (S64)x * x + (S64)z * z;
	U32 xzLength = QSqr((U32)xz2, (U32)(xz2 >> 32));

	X0 = (-GetAngleVector2D(y, xzLength)) & 4095;
	LIB386_TRACE_CPP("GetAngleVector3D", "2", "return=%d X0=%d Y0=%d", X0, X0, Y0);
	return X0;
}
