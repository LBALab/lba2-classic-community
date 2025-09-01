#include <3D/IMATTRA.H>

void InitMatrixTransF(TYPE_MAT *MatDst, S32 tx, S32 ty, S32 tz)
{
	MatDst->F.TX = tx;
	MatDst->F.TY = ty;
	MatDst->F.TZ = tz;
}

// -----------------------------------------------------------------------------
Func_InitMatrixTrans *InitMatrixTrans = InitMatrixTransF; ///< Do Not call from C

