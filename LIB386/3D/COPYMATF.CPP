#include <3D/COPYMAT.H>
#include <string.h>

void CopyMatrixF(TYPE_MAT *MatDst, TYPE_MAT *MatSrc)
{
	memcpy(MatDst, MatSrc, sizeof(TYPE_MAT));
}

// -----------------------------------------------------------------------------
Func_CopyFlipMatrix *CopyMatrix = CopyMatrixF; ///< Do Not call from C
