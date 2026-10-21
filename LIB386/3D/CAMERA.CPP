#include <3D/CAMERA.H>

#include <3D/DATAMAT.H>
#include <3D/GETANG3D.H>
#include <3D/IMATSTD.H>
#include <3D/LIGHT.H>
#include <3D/LIROT3D.H>
#include <3D/LPROJ.H>
#include <3D/LROT3D.H>
#include <3D/PROJ.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
S32 LFactorX = 0;
S32 LFactorY = 0;

S32 CameraAlpha = 0;
S32 CameraBeta = 0;
S32 CameraGamma = 0;

S32 CameraX = 0;
S32 CameraY = 0;
S32 CameraZ = 0;

S32 CameraXr = 0;
S32 CameraYr = 0;
S32 CameraZr = 0;

S32 CameraZrClip = 0;

S32 NearClip = 1;

S32 XCentre = 0;
S32 YCentre = 0;

S32 Xp = 0;
S32 Yp = 0;

S32 X0 = 0;
S32 Y0 = 0;
S32 Z0 = 0;

S32 TypeProj = TYPE_3D;

float FRatioX = 0.0f;
float FRatioY = 0.0f;

U64 MMX_DEMI = (8192LL << 32) | 8192LL;
U64 MMX_DEMI2 = (4096LL << 32) | 4096LL;

S32 CameraXI = 0;
S32 CameraYI = 0;
S32 CameraZI = 0;

S32 CameraXrI = 0;
S32 CameraYrI = 0;
S32 CameraZrI = 0;

// -----------------------------------------------------------------------------
// Each camera call moves the integer camera first and the float one last, so
// X0, Y0, Z0 are left as the float path alone leaves them.

static void SetCameraIOnly(S32 x, S32 y, S32 z) {
    if (TypeProj != TYPE_3D) {
        CameraXrI = x;
        CameraYrI = y;
        CameraZrI = z;
    } else {
        LongRotatePointI(&MatriceWorldI, x, y, z);
        CameraXrI = X0;
        CameraYrI = Y0;
        CameraZrI = Z0;
    }
}

static void SetCameraFOnly(S32 x, S32 y, S32 z) {
    if (TypeProj != TYPE_3D) {
        // iso:
        CameraXr = x;
        CameraYr = y;
        CameraZr = z;
        CameraZrClip = z;
        NearClip = 0;
    } else {
        LongRotatePoint(&MatriceWorld, x, y, z);
        CameraXr = X0;
        CameraYr = Y0;
        CameraZr = Z0;
        CameraZrClip = Z0 - NearClip;
    }
}

void SetCamera(S32 x, S32 y, S32 z) {
    SetCameraIOnly(x, y, z);
    SetCameraFOnly(x, y, z);
}

void SetTargetCamera(S32 x, S32 y, S32 z, S32 gamma) {
    y -= CameraZ;
    z -= CameraY;
    x -= CameraX;

    GetAngleVector3D(x, y, z);
    SetAngleCamera(X0, Y0, gamma);
}

void SetAngleCamera(S32 alpha, S32 beta, S32 gamma) {
    if (TypeProj != TYPE_3D) {
        // tiso:
        alpha = 0;
        beta = 0;
        gamma = 0;
    } else {
        // Guarantee values on interval [0, 4095]
        alpha %= 4096;
        beta %= 4096;
        gamma %= 4096;
    }

    // tback:
    CameraAlpha = alpha;
    CameraBeta = beta;
    CameraGamma = gamma;
    InitMatrixStdI(&MatriceWorldI, alpha, beta, gamma);
    InitMatrixStd(&MatriceWorld, alpha, beta, gamma);
    RecomputeLight();

    SetCameraIOnly(CameraXI, CameraYI, CameraZI);
    SetCameraFOnly(CameraX, CameraY, CameraZ);
}

void SetPosCamera(S32 x, S32 y, S32 z) {
    CameraXI = x;
    CameraYI = y;
    CameraZI = z;
    CameraX = x;
    CameraY = y;
    CameraZ = z;
    SetCamera(x, y, z);
}

void SetPosCameraI(S32 x, S32 y, S32 z) {
    CameraXI = x;
    CameraYI = y;
    CameraZI = z;
    SetCameraIOnly(x, y, z);
}

void SetFollowCamera(S32 targetx, S32 targety, S32 targetz, S32 alpha, S32 beta,
                     S32 gamma, S32 camzoom) {
    CameraXI = targetx;
    CameraYI = targety;
    CameraZI = targetz;
    CameraX = targetx;
    CameraY = targety;
    CameraZ = targetz;

    SetAngleCamera(alpha, beta, gamma);

    if (TypeProj == TYPE_3D) {
        // Back out along the view by camzoom, each camera by its own maths: the
        // integer one as a 486 placed it, so nothing float reaches it.
        CameraZrI = CameraZrI + camzoom;
        LongInverseRotatePointI(&MatriceWorldI, CameraXrI, CameraYrI, CameraZrI);
        CameraXI = X0;
        CameraYI = Y0;
        CameraZI = Z0;

        CameraZr = CameraZr + camzoom;
        CameraZrClip = CameraZrClip + camzoom;

        LongInverseRotatePoint(&MatriceWorld, CameraXr, CameraYr, CameraZr);
        CameraX = X0;
        CameraY = Y0;
        CameraZ = Z0;
    }
}

// -----------------------------------------------------------------------------
static void WorldRotateFloat(S32 x, S32 y, S32 z) {
    LongRotatePoint(&MatriceWorld, x, y, z);
}

static S32 ProjectFloat(S32 x, S32 y, S32 z) {
    return LongProjectPoint(x, y, z);
}

static void WorldRotateInteger(S32 x, S32 y, S32 z) {
    LongRotatePointI(&MatriceWorldI, x, y, z);
}

static S32 ProjectInteger(S32 x, S32 y, S32 z) {
    return LongProjectPointIRound(x, y, z, CameraXrI, CameraYrI, CameraZrI);
}

const T_CAMERA_PATH CameraPathFloat = {WorldRotateFloat, ProjectFloat, &CameraXr, &CameraYr, &CameraZr};
const T_CAMERA_PATH CameraPathInteger = {WorldRotateInteger, ProjectInteger, &CameraXrI, &CameraYrI, &CameraZrI};

// =============================================================================
#ifdef __cplusplus
}
#endif
