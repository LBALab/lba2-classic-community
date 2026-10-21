#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef void(Func_LongInverseRotatePoint)(TYPE_MAT *Mat, S32 x, S32 y, S32 z);

// -----------------------------------------------------------------------------
extern Func_LongInverseRotatePoint *LongInverseRotatePoint;

// The 1997 integer path (LIROT3DI.ASM): Mat->I in 2.14, writes X0, Y0, Z0.
extern void LongInverseRotatePointI(TYPE_MAT *Mat, S32 x, S32 y, S32 z);

// -----------------------------------------------------------------------------
#define LongWorldInverseRotatePoint(x, y, z) \
    LongInverseRotatePoint(&MatriceWorld, x, y, z)

// =============================================================================
#ifdef __cplusplus
}
#endif
