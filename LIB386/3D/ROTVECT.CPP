#include <3D/ROTVECT.H>

#include <3D/IMATSTD.H>
#include <3D/LROT3D.H>

void RotateVector(S32 norme, S32 alpha, S32 beta, S32 gamma)
{
  InitMatrixStd(&MatrixLib2, alpha, beta, gamma);
  LongRotatePoint(&MatrixLib2, 0, 0, norme);
}
