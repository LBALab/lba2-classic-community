#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
S32 AffGraph(S32 numgraph, S32 x, S32 y, const void *bankgraph);
S32 GetDxDyGraph(U8 numgraph, S32 *x, S32 *y, const void *bankgraph);
S32 GetBoxGraph(S32 numgraph, S32 *x0, S32 *y0, S32 *x1, S32 *y1,
                const void *bankgraph);

// =============================================================================
#ifdef __cplusplus
}
#endif
