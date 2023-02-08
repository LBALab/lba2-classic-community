#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef void(Func_LongRotatePoint)(TYPE_MAT *Mat, S32 x, S32 y, S32 z);

// -----------------------------------------------------------------------------
void LongRotatePoint_C(TYPE_MAT *Mat, S32 x, S32 y, S32 z);
#define LongRotatePoint LongRotatePoint_C

// -----------------------------------------------------------------------------
#define LongWorldRotatePoint(x, y, z) LongRotatePoint(&MatriceWorld, x, y, z)

// =============================================================================
#ifdef __cplusplus
}
#endif
