#include <3D/LITLISTF.H>

#include <math.h>

#include <3D/LIGHT.H>
#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>

void LightList(TYPE_MAT *Mat, U16 *dst, TYPE_VT16 *src, S32 n)
{
  if (n == 0)
  {
    return;
  }

  // Calculate the rotated light vector
  LongInverseRotatePoint(Mat, CameraXLight, CameraYLight, CameraZLight);

  for (int i = 0; i < n; i++)
  {
    TYPE_VT16 *v = &src[i];
    float value = roundf((X0 * v->X + Y0 * v->Y + Z0 * v->Z) * FactorLight);
    if (value < 0.0f)
    {
      value = 0.0f;
    }
    dst[i] = (U16)value;
  }
}
