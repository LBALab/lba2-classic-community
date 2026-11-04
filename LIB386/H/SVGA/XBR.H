#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
/* 2x edge-directed upscale (xBR-lv2 rules, see XBR.CPP for attribution).
 * src holds srcW*srcX XRGB8888 pixels (alpha ignored); dst receives
 * (2*srcW)*(2*srcH) opaque pixels. The two buffers must not overlap. */
void XbrUpscale2x(const U32 *src, U32 srcW, U32 srcH, U32 *dst);

// =============================================================================
#ifdef __cplusplus
}
#endif
