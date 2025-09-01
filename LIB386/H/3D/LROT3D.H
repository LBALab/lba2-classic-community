#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
//****************************************************************************
typedef	void (Func_LongRotatePoint)(TYPE_MAT *Mat, S32 x, S32 y, S32 z) ;

extern Func_LongRotatePoint *LongRotatePoint;

// -----------------------------------------------------------------------------
#define LongWorldRotatePoint(x, y, z) LongRotatePoint(&MatriceWorld, x, y, z)

// =============================================================================
#ifdef __cplusplus
}
#endif
