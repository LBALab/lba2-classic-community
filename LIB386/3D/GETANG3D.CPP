#include <3D/GETANG3D.H>

#include <3D/CAMERA.H>
#include <3D/GETANG2D.H>
#include <3D/SQRROOT.H>

S32 GetAngleVector3D(S32 x, S32 y, S32 z)
{
	S32 angleXZ = GetAngleVector2D(x, z);
	angleXZ = (~angleXZ + 2049) & 4095;
	Y0 = angleXZ;

	/* Full 64-bit products (matching ASM imul eax which is 32×32→64 signed) */
	S64 xz2 = (S64)x * x + (S64)z * z;
	U32 xzLength = QSqr((U32)xz2, (U32)(xz2 >> 32));

	X0 = (-GetAngleVector2D(y, xzLength)) & 4095;
	return X0;
}
