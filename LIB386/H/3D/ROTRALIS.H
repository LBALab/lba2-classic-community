#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef void(Func_RotTransList)(TYPE_MAT *Mat, TYPE_VT16 *Dst, TYPE_VT16 *Src,
                                S32 NbPoints);

// -----------------------------------------------------------------------------
extern Func_RotTransList *RotTransList; ///< Do Not call from C

// =============================================================================
#ifdef __cplusplus
}
#endif
