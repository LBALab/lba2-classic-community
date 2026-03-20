#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <3D/DATAMAT.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

typedef void(Func_LightList)(TYPE_MAT *Mat, U16 *dst, TYPE_VT16 *src, S32 n);

extern void LightList(TYPE_MAT *Mat, U16 *dst, TYPE_VT16 *src, S32 n);
extern Func_LightList *LightListPtr;

#pragma aux LightList "*" parm[ebx][edi][esi][ecx] modify[eax ebx edx]

// =============================================================================
#ifdef __cplusplus
}
#endif
