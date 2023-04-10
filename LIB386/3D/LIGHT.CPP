/*
;****************************************************************************

		.386p

		.model SMALL, C

		include 3D.INC

;****************************************************************************
		.DATA

;****************************************************************************
		public	C AlphaLight
		public	C BetaLight
AlphaLight	dd	0000
BetaLight	dd	0000

		public	C CameraXLight
		public	C CameraYLight
		public	C CameraZLight
CameraXLight	dd	LIB_NORMAL_UNIT
CameraYLight	dd	0000
CameraZLight	dd	0000

		public	C NormalXLight
		public	C NormalYLight
		public	C NormalZLight
NormalXLight	dd	LIB_NORMAL_UNIT
NormalYLight	dd	0000
NormalZLight	dd	0000

		public	C FactorLight
FactorLight	REAL4	1.52587890625E-5	;	1.0 / 65536.0

		public	C PosXLight
		public	C PosYLight
		public	C PosZLight
PosXLight	dd	0000
PosYLight	dd	0000
PosZLight	dd	0000

		public	C TargetXLight
		public	C TargetYLight
		public	C TargetZLight
TargetXLight	dd	0000
TargetYLight	dd	0000
TargetZLight	dd	0000

		public	C LightNormalUnit
LightNormalUnit	dd	LIB_NORMAL_UNIT

ASSUME ds:SEG LightNormalUnit

;****************************************************************************
		extrn	C	MatriceWorld:DWORD
		extrn	C	MatrixLib2:DWORD

		extrn	C	X0:DWORD
		extrn	C	Y0:DWORD
		extrn	C	Z0:DWORD

		extrn	C	InitMatrixStd:DWORD
		extrn	C	RotatePoint:DWORD
		extrn	C	LongInverseRotatePoint:DWORD
*/

#include <3D/LIGHT.H>

#include <stdio.h>
#include <3D/IMATSTD.H>
#include <3D/LIROT3D.H>
#include <3D/ROT3D.H>
#include <3D/CAMERA.H>

S32 AlphaLight = 0, BetaLight = 0, GammaLight = 0;
S32 NormalXLight = 0, NormalYLight = 0, NormalZLight = 0;
S32 CameraXLight = 0, CameraYLight = 0, CameraZLight = 0;
S32 PosXLight = 0, PosYLight = 0, PosZLight = 0;
S32 TargetXLight = 0, TargetYLight = 0, TargetZLight = 0;
S32 LightNormalUnit = LIB_NORMAL_UNIT;
float FactorLight = 1.52587890625E-5;	// 1.0 / 65536.0

void SetLightVector(S32 alpha, S32 beta, S32 gamma)
{
	/*
;void	SetLightVector( S32 alpha, S32 beta) ;

;#pragma aux SetLightVector	"*"		\
;	parm		[eax] [ebx]		\
;	modify		[ecx edx esi edi]

SetLightVector PROC \
			uses eax ebx ecx edx edi esi ebp\
			alpha: DWORD, beta: DWORD
			mov eax, alpha
			mov ebx, beta


		and	eax, 4095
		and	ebx, 4095

		xor	ecx, ecx
		mov	[AlphaLight], eax

		mov	[BetaLight], ebx
		mov	edi, offset MatrixLib2

		call	[InitMatrixStd]

		xor	eax, eax
		xor	ebx, ebx

		mov	ecx, [LightNormalUnit]
		mov	esi, offset MatrixLib2

		call	[LongInverseRotatePoint]

		mov	eax, [X0]
		mov	ebx, [Y0]

		mov	ecx, [Z0]
		mov	[NormalXLight], eax

		mov	[NormalYLight], ebx
		mov	[NormalZLight], ecx
		mov	esi, offset MatriceWorld
		call	[RotatePoint]

		mov	eax, [X0]
		mov	ebx, [Y0]

		mov	ecx, [Z0]
		mov	[CameraXLight], eax

		mov	[CameraYLight], ebx
		mov	[CameraZLight], ecx

		ret

SetLightVector	endp
	*/
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
	/*
;void	RecomputeLight();

;#pragma aux RecomputeLight	"*"		\
;	modify		[eax ebx ecx edx esi edi]

RecomputeLight	proc

		mov	eax, [NormalXLight]
		mov	ebx, [NormalYLight]

		mov	ecx, [NormalZLight]

		mov	esi, offset MatriceWorld
		call	[RotatePoint]

		mov	eax, [X0]
		mov	ebx, [Y0]

		mov	ecx, [Z0]
		mov	[CameraXLight], eax

		mov	[CameraYLight], ebx
		mov	[CameraZLight], ecx

		ret
RecomputeLight	endp
	*/
	RotatePoint(&MatriceWorld, NormalXLight, NormalYLight, NormalZLight);

	CameraXLight = X0;
	CameraYLight = Y0;
	CameraZLight = Z0;
}

