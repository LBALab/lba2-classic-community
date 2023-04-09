#include <3D/ROTMAT.H>
#include <3D/IMATSTD.H>
#include <3D/MULMAT.H>

void RotateMatrixU(TYPE_MAT *MatDst, TYPE_MAT *MatSrc,
                   S32 x, S32 y, S32 z)
{
  InitMatrixStd(&MatrixLib2, x, y, z);
  MulMatrix(MatDst, MatSrc, &MatrixLib2);
}

// -----------------------------------------------------------------------------
Func_RotateMatrix *RotateMatrix = RotateMatrixU;
