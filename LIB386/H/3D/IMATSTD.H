#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef void(Func_InitMatrix)(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma);

// -----------------------------------------------------------------------------
void InitMatrixStd_C(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma);
#define InitMatrixStd InitMatrixStd_C

// =============================================================================
#ifdef __cplusplus
}
#endif
