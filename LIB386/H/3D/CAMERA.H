#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
extern S32 LFactorX;
extern S32 LFactorY;

extern S32 CameraAlpha;
extern S32 CameraBeta;
extern S32 CameraGamma;

extern S32 CameraX;
extern S32 CameraY;
extern S32 CameraZ;

extern S32 CameraXr;
extern S32 CameraYr;
extern S32 CameraZr;

extern S32 CameraZrClip;

extern S32 NearClip;

extern S32 XCentre;
extern S32 YCentre;

extern S32 Xp;
extern S32 Yp;

extern S32 X0;
extern S32 Y0;
extern S32 Z0;

extern S32 TypeProj;

extern float FRatioX;
extern float FRatioY;

extern U64 MMX_DEMI;
extern U64 MMX_DEMI2;

// -----------------------------------------------------------------------------
void SetCamera(S32 x, S32 y, S32 z);
void SetTargetCamera(S32 x, S32 y, S32 z, S32 gamma);
void SetAngleCamera(S32 alpha, S32 beta, S32 gamma);
void SetPosCamera(S32 x, S32 y, S32 z);
void SetFollowCamera(S32 targetx, S32 targety, S32 targetz, S32 alpha, S32 beta,
                     S32 gamma, S32 camzoom);

// =============================================================================
#ifdef __cplusplus
}
#endif
