#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef void(Func_MulMatrix)(TYPE_MAT *MatDst, TYPE_MAT *MatSrc1, TYPE_MAT *MatSrc2);

// -----------------------------------------------------------------------------
extern Func_MulMatrix *MulMatrix; ///< Do Not call from C

// =============================================================================
#ifdef __cplusplus
}
#endif
