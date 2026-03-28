#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
/* SinTab holds 5120 contiguous S16 values: 1024 sine entries followed by 4096
   cosine entries.  CosTab aliases &SinTab[1024], matching the original ASM
   layout where both labels shared one block. */
extern S16 SinTab[];
#define CosTab (&SinTab[1024])

// =============================================================================
#ifdef __cplusplus
}
#endif
