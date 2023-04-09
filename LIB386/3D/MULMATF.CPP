
#include <3D/MULMAT.H>

void MulMatrixF(TYPE_MAT *MatDst, TYPE_MAT *MatSrc1, TYPE_MAT *MatSrc2)
{
  TYPE_MAT *m1 = MatSrc1;
  TYPE_MAT *m2 = MatSrc2;
  TYPE_MAT *m3 = MatDst;

  m3->F.M11 = m1->F.M11 * m2->F.M11 + m1->F.M12 * m2->F.M21 + m1->F.M13 * m2->F.M31;
  m3->F.M12 = m1->F.M11 * m2->F.M12 + m1->F.M12 * m2->F.M22 + m1->F.M13 * m2->F.M32;
  m3->F.M13 = m1->F.M11 * m2->F.M13 + m1->F.M12 * m2->F.M23 + m1->F.M13 * m2->F.M33;

  m3->F.M21 = m1->F.M21 * m2->F.M11 + m1->F.M22 * m2->F.M21 + m1->F.M23 * m2->F.M31;
  m3->F.M22 = m1->F.M21 * m2->F.M12 + m1->F.M22 * m2->F.M22 + m1->F.M23 * m2->F.M32;
  m3->F.M23 = m1->F.M21 * m2->F.M13 + m1->F.M22 * m2->F.M23 + m1->F.M23 * m2->F.M33;

  m3->F.M31 = m1->F.M31 * m2->F.M11 + m1->F.M32 * m2->F.M21 + m1->F.M33 * m2->F.M31;
  m3->F.M32 = m1->F.M31 * m2->F.M12 + m1->F.M32 * m2->F.M22 + m1->F.M33 * m2->F.M32;
  m3->F.M33 = m1->F.M31 * m2->F.M13 + m1->F.M32 * m2->F.M23 + m1->F.M33 * m2->F.M33;
}

// -----------------------------------------------------------------------------
Func_MulMatrix *MulMatrix = MulMatrixF;
