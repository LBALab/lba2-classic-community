#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef void(Func_InitMatrix)(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma);

extern Func_InitMatrix *InitMatrixStd;

// The 1997 integer path (IMATSTDI.ASM): 2.14 fixed point into MatDst->I.
extern void InitMatrixStdI(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma);

// =============================================================================
#ifdef __cplusplus
}
#endif
