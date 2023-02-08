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
void LongInverseRotatePoint_C(TYPE_MAT *Mat, S32 x, S32 y, S32 z);
#define LongInverseRotatePoint LongInverseRotatePoint_C

// -----------------------------------------------------------------------------
#define LongWorldInverseRotatePoint(x, y, z)                                   \
  LongInverseRotatePoint(&MatriceWorld, x, y, z)

// =============================================================================
#ifdef __cplusplus
}
#endif
