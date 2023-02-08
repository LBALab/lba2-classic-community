#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef void(Func_CopyFlipMatrix)(TYPE_MAT *MatDst, TYPE_MAT *MatSrc);

// -----------------------------------------------------------------------------
extern Func_CopyFlipMatrix *CopyMatrix; ///< Do Not call from C

// =============================================================================
#ifdef __cplusplus
}
#endif
