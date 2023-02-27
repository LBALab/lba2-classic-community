#include <3D/LITLISTF.H>

#include <3D/LIGHT.H>
#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>

#include <stdio.h>

void LightList(TYPE_MAT *Mat, S32 *dst, TYPE_VT16 *src, S32 n)
{
	/*
;void	LightListF(TYPE_MAT *Mat, S32 *dst, S32 *src, S32 n)	;

;#pragma aux LightListF	"*"		\
;	parm 	[ebx] [edi] [esi] [ecx]	\
;	modify	[eax ebx edx]

; ╔════════════════════════════════════════════════════════════════════╗
; ║ ListLight: Calculate the light at each point of the list of vect.  ║
; ╟────────────────────────────────────────────────────────────────────╢
; ║ Call: EBX = Rotation matrix                                        ║
; ║       ECX = Number of lights to calculated                         ║
; ║       ESI = List of vectors (which light has to be calculated)     ║
; ║       EDI = List of light (WORDs) to be filled                     ║
; ║                                                                    ║
; ╟────────────────────────────────────────────────────────────────────╢
; ║ Return: Each calculated light is in the form 8.8                   ║
; ║       EDI = Pointer of the next entry in the light list            ║
; ║                                                                    ║
; ╚════════════════════════════════════════════════════════════════════╝


LightListF		PROC
			test	ecx,ecx			; Nothing to do ?
			jz	@@Ret

			push	esi
			push	edi

			push	ecx
			mov	esi,ebx

			mov	eax,[CameraXLight]
			mov	ebx,[CameraYLight]

			mov	ecx,[CameraZLight]
			call	LongInverseRotatePointF	; Calculate the rotated light vector

			pop	ecx
			pop	edi
														; 00  01  02  03  04  05  06  07
			fild	[Z0]						; Z0

			pop	esi
			lea	edi,[edi+ecx*2-2]

			fild	[Y0]						; Y0  Z0
			fild	[X0]						; X0  Y0  Z0

			lea	esi,[esi+ecx*8]
			xor	ecx,-1

			xor	ebx, ebx
			add	ecx, 2

			fmul	[FactorLight]		; X1  Y0  Z0
			fxch	st(1)						; Y0  X1  Z0

			fmul	[FactorLight]		; Y1  X1  Z0
			fxch	st(2)						; Z0  X1  Y1

			fmul	[FactorLight]		; Z1  X1  Y1
			fxch	st(2)						; Y1  X1  Z1
			fstp	[Y0]						; X1  Z1
			fstp	[X0]						; Z1
			fstp	[Z0]						;

																						; 00  01  02  03  04  05  06  07
			fild	[esi+ecx*8-8].STRUC_VERTEX.VT_X	; X
			fild	[esi+ecx*8-8].STRUC_VERTEX.VT_Y	; Y   X
			fild	[esi+ecx*8-8].STRUC_VERTEX.VT_Z	; Z   Y   X
			fxch	st(2)														; X   Y   Z
			fmul	[X0]														; X'  Y   Z
			fxch	st(1)														; Y   X'  Z
			fmul	[Y0]														; Y'  X'  Z
			fxch	st(2)														; Z   X'  Y'
			fmul	[Z0]														; Z'  X'  Y'
			fxch	st(1)														; X'  Z'  Y'

			push	ebx			; <=> sub esp,4; but doesn't modify the flags
			jz	@@End1

			fild	[esi+ecx*8].STRUC_VERTEX.VT_Z		; Z   X'  Z'  Y'
			fild	[esi+ecx*8].STRUC_VERTEX.VT_Y		; Y   Z   Z'  X'  Z'  Y'
			fild	[esi+ecx*8].STRUC_VERTEX.VT_X		; X   Y   Z   X'  Z'  Y'
			fxch	st(1)														; Y   X   Z   X'  Z'  Y'
			fmul	[Y0]														; y'  X   Z   X'  Z'  Y'
			fxch	st(5)														; Y'  X   Z   X'  Z'  y'
			faddp	st(4),st												; X   Z   X'  I   y'
			fmul	[X0]														; x'  Z   X'  I   y'
			fxch	st(2)														; X'  Z   x'  I   y'
			faddp	st(3),st												; Z   x'  L   y'

			inc	ecx
			jne	@@Loop
			jmp	@@ExitLoop

@@Loop2:
			mov	[edi+ecx*2-2],ax

			inc	ecx
			jz	@@ExitLoop
																					; 00  01  02  03  04  05  06  07
@@Loop:
			fmul	[Z0]													; z'  x'  L   y'
			fxch	st(2)													; L   x'  z'  y'
			fild	[esi+ecx*8].STRUC_VERTEX.VT_X	; X   L   X'  Z'  Y'
			fild	[esi+ecx*8].STRUC_VERTEX.VT_Y	; Y   X   L   X'  Z'  Y'
			fild	[esi+ecx*8].STRUC_VERTEX.VT_Z	; Z   Y   X   L   X'  Z'  Y'
			fxch	st(3)													; L   Y   X   Z   X'  Z'  Y'
			fistp	dword ptr[esp]								; Y   X   Z   X'  Z'  Y'
			fmul	[Y0]													; y'  X   Z   X'  Z'  Y'
			fxch	st(5)													; Y'  X   Z   X'  Z'  y'
			faddp	st(4),st											; X   Z   X'  I   y'

			mov	eax, [esp]

			fmul	[X0]													; x'  Z   X'  I   y'
			fxch	st(2)													; X'  Z   x'  I   y'
			faddp	st(3),st											; Z   x'  L   y'

			test	eax, eax
			jns	@@Loop2

			mov	[edi+ecx*2-2],bx

			inc	ecx
			jnz	@@Loop


@@ExitLoop:
			fmul	[Z0]							; z'  x'  L   y'
			fxch	st(2)							; L   x'  z'  y'
			fistp	dword ptr[esp]		; x'  z'  y'
			faddp	st(2),st					; z'  I

			mov	eax, [esp]

			faddp	st(1),st					; L

			test	eax, eax
			jns	@@Pos_Ok1

			xor	eax, eax
@@Pos_Ok1:
			fistp	dword ptr[esp]

			mov	[edi-2],ax
@@End:			pop	eax

			test	eax, eax
			jns	@@Pos_Ok2

			xor	eax, eax
@@Pos_Ok2:
			mov	[edi],ax
@@Ret:
			ret

@@End1:
			faddp	st(2),st		; Z'
			faddp	st(1),st		; L
			fistp	dword ptr[esp]
			jmp	@@End

LightListF		ENDP
	*/
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;

	if (n == 0)
		return;

	// Calculate the rotated light vector
	LongInverseRotatePoint(Mat, CameraXLight, CameraYLight, CameraZLight);

	X0 *= FactorLight;
	Y0 *= FactorLight;
	Z0 *= FactorLight;

	TYPE_VT16 *v1 = &src[n - 1];
	float Xp = X0 * v1->X;
	float Yp = Y0 * v1->Y;
	float Zp = Z0 * v1->Z;

	TYPE_VT16 *v2 = &src[n];
	float yp = v2->Y * Y0;
	float xp = v2->X * X0;
	float zp = v2->Z * Z0;
	float I = Zp + Yp;
	float L = Xp + I;
/*
	float X1, Y1, Z1;

	for (int i = n; i > 0; i--, src += 8, dst++)
	{


		X1 = src[0];
		Y1 = src[1];
		Z1 = src[2];

		float Xp = Y0 * Z1 - Z0 * Y1;
		float Yp = Z0 * X1 - X0 * Z1;
		float Zp = X0 * Y1 - Y0 * X1;

		if (i != 1)
		{
			float Z2 = src[8 + 2];
			float Y2 = src[8 + 1];
			float X2 = src[8 + 0];

			*(float *)(dst + 1) = Xp * X2 + Yp * Y2 + Zp * Z2;
		}

		*dst = (int)(Zp * 256.0f);
	}
	*/
}
