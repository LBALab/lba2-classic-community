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

/* Stereo (dual-eye) rendering injection. Both default 0, which keeps the
 * ported camera/projection byte-identical to the ASM. StereoCamShiftX is
 * added to the camera-space X position in SetCamera (a lateral eye offset);
 * StereoProjShiftX is added to the projection centre X in the 3D projector
 * (asymmetric-frustum convergence). See GL_SetStereoEye. */
extern S32 StereoCamShiftX;
extern S32 StereoCamShiftY;
extern S32 StereoProjShiftX;
/* True-pose stereo eye override (see AffGrilleExt): while StereoEyeOverride
 * is set, the camera orientation is re-applied from these angles after the
 * rig's own SetFollowCamera, keeping the rig position game-controlled while
 * the render orientation is the exact tracked per-eye view. */
extern S32 StereoEyeOverride;
extern S32 StereoEyeAlpha;
extern S32 StereoEyeBeta;
extern S32 StereoEyeGamma;

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
