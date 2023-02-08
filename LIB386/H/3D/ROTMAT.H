#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef void(Func_RotateMatrix)(TYPE_MAT *MatDst, TYPE_MAT *MatSrc, S32 x,
                                S32 y, S32 z);

// -----------------------------------------------------------------------------
extern Func_RotateMatrix *RotateMatrix; ///< Do Not call from C

// =============================================================================
#ifdef __cplusplus
}
#endif
