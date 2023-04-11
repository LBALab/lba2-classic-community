#include <3D/LIGHT.H>

#include <stdio.h>
#include <3D/IMATSTD.H>
#include <3D/LIROT3D.H>
#include <3D/ROT3D.H>
#include <3D/CAMERA.H>

S32 AlphaLight = 0, BetaLight = 0, GammaLight = 0;
S32 NormalXLight = LIB_NORMAL_UNIT, NormalYLight = 0, NormalZLight = 0;
S32 CameraXLight = LIB_NORMAL_UNIT, CameraYLight = 0, CameraZLight = 0;
S32 PosXLight = 0, PosYLight = 0, PosZLight = 0;
S32 TargetXLight = 0, TargetYLight = 0, TargetZLight = 0;
S32 LightNormalUnit = LIB_NORMAL_UNIT;
float FactorLight = 1.52587890625E-5;	// 1.0 / 65536.0

void SetLightVector(S32 alpha, S32 beta, S32 gamma)
{
    AlphaLight = alpha & 4095;
    BetaLight = beta & 4095;

    // For some reason, gamma is not used in the original implementation :shrug:
    InitMatrixStd(&MatrixLib2, alpha, beta, 0);

    LongInverseRotatePoint(&MatrixLib2, 0, 0, LightNormalUnit);

    NormalXLight = X0;
    NormalYLight = Y0;
    NormalZLight = Z0;

    RotatePoint(&MatriceWorld, X0, Y0, Z0);

    CameraXLight = X0;
    CameraYLight = Y0;
    CameraZLight = Z0;
}

void RecomputeLight()
{
	RotatePoint(&MatriceWorld, NormalXLight, NormalYLight, NormalZLight);

	CameraXLight = X0;
	CameraYLight = Y0;
	CameraZLight = Z0;
}

