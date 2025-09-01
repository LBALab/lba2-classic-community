#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
S32 RegleTrois(S32 Val1, S32 Val2, S32 NbSteps, S32 CurrentStep);
S32 BoundRegleTrois(S32 Val1, S32 Val2, S32 NbSteps, S32 CurrentStep);

// =============================================================================
#ifdef __cplusplus
}
#endif
