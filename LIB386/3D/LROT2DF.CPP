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
	/*
;void	LongRotateF(S32 x, S32 z, S32 angle)	;

;#pragma aux	LongRotateF			\
;	parm		[eax] [ecx] [edx]	\
;	modify exact	[edx]

LongRotateF	PROC \
			uses eax ebx ecx edx edi esi ebp\
			x: DWORD, z: DWORD, angle: DWORD
			mov eax, x
			mov ecx, z
			mov edx, angle

		mov	[X0], eax
		mov	[Z0], ecx

		and	edx, 4095
		jz	norot			; opt edx=0

		fild	[X0]			;  X
		shl	edx, 2
		fild	[Z0]			;  Z   X
		fld	st(1)			;  X   Z   X
		fmul	CosTabF[edx]		;  XC  Z   X
		fld	st(1)			;  Z   XC  Z    X
		fmul	SinTabF[edx]		;  ZS  XC  Z    X
		fxch	st(3)			;  X   XC  Z    ZS
						;  NOP
		fmul	SinTabF[edx]		;  XS  XC  Z    ZS
		fxch	st(1)			;  XC  XS  Z    ZS
		faddp	st(3), st(0)		;  XS  Z   X'
		fxch	st(1)			;  Z   XS  X'
		fmul	CosTabF[edx]		;  ZC  XS  X'
		fxch	st(1)			;  XS  ZC  X'
						; NOP
						; NOP
		fsubp	st(1), st(0)		;  Y'  X'
		fxch	st(1)			;  X'  Y'
		fistp	[X0]			;  Y'
		fistp	[Z0]
norot:
		ret

LongRotateF	endp
	*/
	X0 = x;
	Z0 = z;

	angle &= 4095;

	if (angle == 0)
	{
		return;
	}

	double cosVal = CosTabF[angle << 2];
	double sinVal = SinTabF[angle << 2];

	double X = (double)x;
	double Z = (double)z;
	double X2 = X * cosVal + Z * sinVal;
	double Z2 = Z * cosVal - X * sinVal;

	X0 = (S32)X2;
	Z0 = (S32)Z2;
}
