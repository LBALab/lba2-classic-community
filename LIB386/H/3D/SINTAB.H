#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
/* SinTab holds 5120 contiguous S16 values: 1024 sine entries followed by 4096
   cosine entries.  CosTab points at SinTab[1024] (same layout as CosTabF /
   SinTabU32 in SINTABF).  Original ASM used two labels on one block. */
extern S16 SinTab[];
extern S16 *CosTab;

// =============================================================================
#ifdef __cplusplus
}
#endif
