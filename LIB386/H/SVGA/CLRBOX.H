#pragma once

#include <SVGA/BOX.H>
#include <SVGA/DIRTYBOX.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
extern void ClearBox(void *dst, U32 *TabOffDst, T_BOX *box);
extern void SetClearColor(U32 color);

// =============================================================================
#ifdef __cplusplus
}
#endif
