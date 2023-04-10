#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define NORMAL_UNIT 16384
#define LIB_NORMAL_UNIT 15360

// -----------------------------------------------------------------------------
extern S32 AlphaLight, BetaLight, GammaLight;
extern S32 NormalXLight, NormalYLight, NormalZLight;
extern S32 CameraXLight, CameraYLight, CameraZLight;
extern S32 PosXLight, PosYLight, PosZLight;
extern S32 TargetXLight, TargetYLight, TargetZLight;
extern S32 LightNormalUnit;
extern float FactorLight;

// -----------------------------------------------------------------------------
void SetLightVector(S32 alpha, S32 beta, S32 gamma);
void RecomputeLight();

// -----------------------------------------------------------------------------
#define SetLightIntensity(intensity) (LightNormalUnit = (intensity))

// =============================================================================
#ifdef __cplusplus
}
#endif
