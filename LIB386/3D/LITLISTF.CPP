#include <3D/LITLISTF.H>

#include <3D/LIGHT.H>
#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>
#include <SYSTEM/UTILS.H>

// ╔════════════════════════════════════════════════════════════════════╗
// ║ ListLight: Calculate the light at each point of the list of vect.  ║
// ╟────────────────────────────────────────────────────────────────────╢
// ║ Call: EBX = Rotation matrix                                        ║
// ║       ECX = Number of lights to calculated                         ║
// ║       ESI = List of vectors (which light has to be calculated)     ║
// ║       EDI = List of light (WORDs) to be filled                     ║
// ║                                                                    ║
// ╟────────────────────────────────────────────────────────────────────╢
// ║ Return: Each calculated light is in the form 8.8                   ║
// ║       EDI = Pointer of the next entry in the light list            ║
// ║                                                                    ║
// ╚════════════════════════════════════════════════════════════════════╝
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
    float value = ROUND((X0 * v->X + Y0 * v->Y + Z0 * v->Z) * FactorLight);
    if (value < 0.0f)
    {
      value = 0.0f;
    }
    dst[i] = (U16)value;
  }
}
