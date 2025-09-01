#include <3D/GETANG3D.H>

#include <3D/CAMERA.H>
#include <3D/GETANG2D.H>
#include <3D/SQRROOT.H>

S32 GetAngleVector3D(S32 x, S32 y, S32 z)
{
  S32 angleXZ = GetAngleVector2D(x, z);
  angleXZ = ~angleXZ + 2049;
  angleXZ &= 4095;
  Y0 = angleXZ;

  U64 x2 = x * x;
  U64 z2 = z * z;
  U64 xz2 = x2 + z2;

  U32 xzLength = QSqr(xz2 & 0xFFFFFFFF, xz2 >> 32);

  S32 angleXYZ = -GetAngleVector2D(y, xzLength);
  angleXYZ &= 4095;
  X0 = angleXYZ;

  return X0;
}
