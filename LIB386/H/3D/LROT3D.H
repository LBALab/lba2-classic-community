#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
//****************************************************************************
typedef void(Func_LongRotatePoint)(TYPE_MAT *Mat, S32 x, S32 y, S32 z);

extern Func_LongRotatePoint *LongRotatePoint;

// The 1997 integer path (LROT3DI.ASM): Mat->I in 2.14, writes X0, Y0, Z0.
extern void LongRotatePointI(TYPE_MAT *Mat, S32 x, S32 y, S32 z);

// -----------------------------------------------------------------------------
#define LongWorldRotatePoint(x, y, z) LongRotatePoint(&MatriceWorld, x, y, z)

// =============================================================================
#ifdef __cplusplus
}
#endif
