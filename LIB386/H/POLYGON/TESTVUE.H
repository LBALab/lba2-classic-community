#pragma once

#include <POLYGON/POLY.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef S32(Func_TestVuePoly)(Struc_Point *ptr);

// -----------------------------------------------------------------------------
extern Func_TestVuePoly *TestVuePoly;

// =============================================================================
#ifdef __cplusplus
}
#endif
