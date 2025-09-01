#pragma once

#include <SVGA/GRAPH.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
extern U8 ColMask;

// -----------------------------------------------------------------------------
S32 AffMask(S32 nummask, S32 x, S32 y, void *bankmask);

// -----------------------------------------------------------------------------
#define ColorMask(color) ColMask = (U8)(color)

#define GetDxDyMask(nummask, px, py, bankmask)                                 \
  GetDxDyGraph(nummask, px, py, bankmask)

// =============================================================================
#ifdef __cplusplus
}
#endif
