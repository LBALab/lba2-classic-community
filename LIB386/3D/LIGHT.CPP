#include <3D/LIGHT.H>

#include <3D/CAMERA.H>
#include <3D/IMATSTD.H>
#include <3D/LIROT3D.H>
#include <3D/ROT3D.H>

#include <assert.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
/// @brief Equivalent to INT(15*256*4)
/// @details LIB_NORMAL_UNIT 16381 which is equivalent to
///    INT(4095.49*65536/16384) is MAX *shift/ colinearity case.
///    16382 is too big, because we can get 4095.5, that's
///    bounded up to 4096=>overflow
#define LIB_NORMAL_UNIT 15360
#define NORMAL_UNIT 16384

// -----------------------------------------------------------------------------
S32 AlphaLight = 0, BetaLight = 0, GammaLight = 0;
S32 NormalXLight = LIB_NORMAL_UNIT, NormalYLight = 0, NormalZLight = 0;
S32 CameraXLight = LIB_NORMAL_UNIT, CameraYLight = 0, CameraZLight = 0;
S32 PosXLight = 0, PosYLight = 0, PosZLight = 0;
S32 TargetXLight = 0, TargetYLight = 0, TargetZLight = 0;
S32 LightNormalUnit = LIB_NORMAL_UNIT;
float FactorLight = 1.52587890625E-5f; // 1.0 / 65536.0

// -----------------------------------------------------------------------------
void SetLightVector(S32 alpha, S32 beta, S32 /*gamma*/) {
  assert(alpha > 0);
  assert(beta > 0);

  // Guarantee values on interval [0, 4095]
  alpha %= 4096;
  beta %= 4096;

  AlphaLight = alpha;
  BetaLight = beta;

  InitMatrixStd(&MatrixLib2, alpha, beta, 0);
  LongInverseRotatePoint(&MatrixLib2, 0, 0, LightNormalUnit);
  NormalXLight = X0;
  NormalYLight = Y0;
  NormalZLight = Z0;

  RecomputeLight();
}

void RecomputeLight() {
  LongRotatePoint(&MatriceWorld, NormalXLight, NormalYLight, NormalZLight);
  CameraXLight = X0;
  CameraYLight = Y0;
  CameraZLight = Z0;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
