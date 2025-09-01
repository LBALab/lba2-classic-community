#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
enum ProjectionType { TYPE_3D = 0, TYPE_ISO = 1 };

// -----------------------------------------------------------------------------
extern void SetProjection(S32 xcentre, S32 ycentre, S32 clip, S32 factorx,
                          S32 factory);
extern void SetIsoProjection(S32 xcentre, S32 ycentre);

// =============================================================================
#ifdef __cplusplus
}
#endif
