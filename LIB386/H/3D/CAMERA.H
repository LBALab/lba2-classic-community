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

/* Runtime isometric view scale (fixed-point, ISO_SCALE_ONE == 1.0). Multiplies the
   iso projection of models (LongProjectPointIso) and brick positions (Map2Screen)
   so an interior view can be zoomed or supersampled without touching world state.
   At the default ISO_SCALE_ONE the projection is bit-for-bit identical to the
   original, so the faithful path and the ASM equivalence tests are unaffected. */
#define ISO_SCALE_SHIFT 8
#define ISO_SCALE_ONE (1 << ISO_SCALE_SHIFT)
extern S32 IsoScale;

/* Scale a native iso pixel constant by the runtime view scale. At ISO_SCALE_ONE
   this is the identity (n*256/256 == n) for any n, so the faithful path is
   unchanged. Use it for every screen-pixel dimension in the iso path (clip
   bounds, brick column granularity, sprite offsets) so they all move with one
   factor instead of each carrying its own scaling. See docs/ISO_SPACE_AUDIT.md. */
#define ISO_PX(n) ((S32)(((long long)(n) * IsoScale) / ISO_SCALE_ONE))

extern U64 MMX_DEMI;
extern U64 MMX_DEMI2;

// -----------------------------------------------------------------------------
void SetCamera(S32 x, S32 y, S32 z);
void SetTargetCamera(S32 x, S32 y, S32 z, S32 gamma);
void SetAngleCamera(S32 alpha, S32 beta, S32 gamma);
void SetPosCamera(S32 x, S32 y, S32 z);
void SetFollowCamera(S32 targetx, S32 targety, S32 targetz, S32 alpha, S32 beta,
                     S32 gamma, S32 camzoom);
void SetIsoScale(S32 scale);

// =============================================================================
#ifdef __cplusplus
}
#endif
