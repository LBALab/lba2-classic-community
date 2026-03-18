#include <3D/IMATTRA.H>
#include <SYSTEM/DEBUG_TRACES.H>

void InitMatrixTransF(TYPE_MAT *MatDst, S32 tx, S32 ty, S32 tz)
{
	LIB386_TRACE_CPP("InitMatrixTransF", "1", "Dst=%p tx=%d ty=%d tz=%d", (void*)MatDst, tx, ty, tz);
	MatDst->F.TX = tx;
	MatDst->F.TY = ty;
	MatDst->F.TZ = tz;
}

// -----------------------------------------------------------------------------
Func_InitMatrixTrans *InitMatrixTrans = InitMatrixTransF; ///< Do Not call from C

