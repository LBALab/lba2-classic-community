/*
;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

		extrn	C	SinTabF:DWORD
		extrn	C	CosTabF:DWORD

		extrn	C	X0:DWORD
		extrn	C	Z0:DWORD

		public	C	LongRotate
		public	C	Rotate

		LongRotate	dd	offset LongRotateF
		Rotate		dd	offset LongRotateF

		ASSUME  DS:SEG SinTabF

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	LongRotateF

;*══════════════════════════════════════════════════════════════════════════*

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
*/

#include <3D/ROT2D.H>
#include <3D/SINTABF.H>
#include <3D/CAMERA.H>
#include <stdio.h>

void Rotate(S32 x, S32 z, S32 angle)
{
	LongRotate(x, z, angle);
}

void LongRotate(S32 x, S32 z, S32 angle)
{
	X0 = x;
	Z0 = z;

	angle &= 4095;

	if (angle == 0)
	{
		return;
	}

	double cosVal = CosTabF[angle];
	double sinVal = SinTabF[angle];

	double X = (double)x;
	double Z = (double)z;
	double X2 = X * cosVal + Z * sinVal;
	double Z2 = Z * cosVal - X * sinVal;

	X0 = (S32)X2;
	Z0 = (S32)Z2;
}
