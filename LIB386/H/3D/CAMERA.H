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

// The same camera on the 1997 integer path (IMATSTDI, LROT3DI, LIROT3DI), kept
// beside the float one by the calls below: MatriceWorldI (DATAMAT.H) and these.
// A 486 drew everything this way, because the 1997 Chooser (CHOOSER.ASM, at
// 333929a) bound each 3D routine to its integer version there; lba2cc binds them
// to the FPU versions, a Pentium's, and keeps this camera so the exterior
// terrain, sea and sky can take the integer path on their own (TERRAIN.CPP).
extern S32 CameraXI; // world position: SetFollowCamera places it by the integer
extern S32 CameraYI; // inverse rotation, so it can differ from CameraX/Y/Z by a
extern S32 CameraZI; // unit where the float one rounds the other way
extern S32 CameraXrI;
extern S32 CameraYrI;
extern S32 CameraZrI;

// -----------------------------------------------------------------------------
void SetCamera(S32 x, S32 y, S32 z);
void SetTargetCamera(S32 x, S32 y, S32 z, S32 gamma);
void SetAngleCamera(S32 alpha, S32 beta, S32 gamma);
void SetPosCamera(S32 x, S32 y, S32 z);
void SetFollowCamera(S32 targetx, S32 targety, S32 targetz, S32 alpha, S32 beta,
                     S32 gamma, S32 camzoom);

// Moves the integer camera only. SetPosCamera moves both to the same place;
// this lets a caller step the integer camera from its own position instead.
void SetPosCameraI(S32 x, S32 y, S32 z);

// -----------------------------------------------------------------------------
// One camera's world rotation and projection, picked once and called through,
// as the Chooser bound a routine. Both read the live camera, so a path picked
// before a SetPosCamera still draws from where the camera is now.
typedef struct {
    void (*WorldRotate)(S32 x, S32 y, S32 z); // world point through the path's matrix, into X0, Y0, Z0
    S32 (*Project)
    (S32 x, S32 y, S32 z); // rotated point to Xp, Yp; 0 when it is clipped
    const S32 *Xr;         // the path's camera, rotated
    const S32 *Yr;
    const S32 *Zr;
} T_CAMERA_PATH;

// MatriceWorld and LongProjectPoint: what lba2cc has always drawn, and a 1997
// Pentium with it.
extern const T_CAMERA_PATH CameraPathFloat;
// MatriceWorldI and LongProjectPointIRound: the 1997 integer path, except that
// the projection rounds to nearest instead of truncating (LPROJ3DI.CPP says why).
extern const T_CAMERA_PATH CameraPathInteger;

// =============================================================================
#ifdef __cplusplus
}
#endif
