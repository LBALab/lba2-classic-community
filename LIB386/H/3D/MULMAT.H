#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef void(Func_MulMatrix)(S32 *MatDst, S32 *MatSrc1, S32 *MatSrc2);

// -----------------------------------------------------------------------------
extern Func_MulMatrix *MulMatrix; ///< Do Not call from C

// =============================================================================
#ifdef __cplusplus
}
#endif
