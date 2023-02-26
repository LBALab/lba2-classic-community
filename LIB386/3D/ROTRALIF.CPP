
#include <3D/ROTRALIS.H>

void RotTransListF(TYPE_MAT *Mat, TYPE_VT16 *Dst, TYPE_VT16 *Src,
									 S32 NbPoints)
{
	/*
;void	RotTransListF(MTYPE_MAT *Mat, S32 *dst, S32 *src, S32 n)	;

;#pragma aux RotTransListF			\
;	parm 		[ebx] [edi] [esi] [ecx]	\
;	modify exact	[ecx esi edi]

RotTransListF	proc
		lea	esi, [esi+ecx*8]
		lea	ecx, [ecx*8]

		add	edi, ecx
		xor	ecx, -1

		inc	ecx
		jmp	EnterLoop
																	; 0  1  2  3  4  5  6  7
NextPoint:
		faddp	st(3), st(0)						; 1  Z3 2  P3
		fistp	word ptr[edi+ecx-8]			; Z3 2  P3
		faddp	st(2), st(0)						; 2  3
		fistp	word ptr[edi+ecx-6]			; 3
		fistp	word ptr[edi+ecx-4]

EnterLoop:
		fild	word ptr[esi+ecx]				; X
		fild	word ptr[esi+ecx+2]			; Y  X
		fild	word ptr[esi+ecx+4]			; Z  Y  X
		fld	st(2)											; X  Z  Y  X
		fmul	[ebx].M11								; X1 Z  Y  X
		fld	st(2)											; Y  X1 Z  Y  X
		fmul	[ebx].M12								; Y1 X1 Z  Y  X
		fxch	st(1)										; X1 Y1 Z  Y  X
		fadd	[ebx].MTX								; X1 Y1 Z  Y  X
		fxch	st(1)										; Y1 X1 Z  Y  X
		fld	st(2)											; Z  Y1 X1 Z  Y  X
		fmul	[ebx].M13								; Z1 Y1 X1 Z  Y  X
		fld	st(5)											; X  Z1 Y1 X1 Z  Y  X
		fmul	[ebx].M21   						; X2 Z1 Y1 X1 Z  Y  X
		fld	st(5)											; Y  X2 Z1 Y1 X1 Z  Y  X
		fmul	[ebx].M22   						; Y2 X2 Z1 Y1 X1 Z  Y  X
		fxch	st(1)										; X2 Y2 Z1 Y1 X1 Z  Y  X
		fadd	[ebx].MTY								; X2 Y2 Z1 Y1 X1 Z  Y  X
		fxch	st(3)										; Y1 Y2 Z1 X2 X1 Z  Y  X
		faddp	st(4), st(0)    				; Y2 Z1 X2 P1 Z  Y  X
		fxch	st(6)										; X  Z1 X2 P1 Z  Y  Y2
		fmul	[ebx].M31								; X3 Z1 X2 P1 Z  Y  Y2
		fxch	st(6)										; Y2 Z1 X2 P1 Z  Y  X3
		fld	st(4)											; Z  Y2 Z1 X2 P1 Z  Y  X3
		fmul	[ebx].M23								; Z2 Y2 Z1 X2 P1 Z  Y  X3
		fxch	st(1)										; Y2 Z2 Z1 X2 P1 Z  Y  X3
		faddp	st(3), st(0)						; Z2 Z1 P2 P1 Z  Y  X3
		fxch	st(5)										; Y  Z1 P2 P1 Z  Z2 X3
		fmul	[ebx].M32								; Y3 Z1 P2 P1 Z  Z2 X3
		fxch	st(6)										; X3 Z1 P2 P1 Z  Z2 Y3
		fadd	[ebx].MTZ								; X3 Z1 P2 P1 Z  Z2 Y3
		fxch	st(1)										; Z1 X3 P2 P1 Z  Z2 Y3
		faddp	st(3), st(0)						; X3 P2 1  Z  Z2 Y3
		fxch	st(3)										; Z  P2 1  X3 Z2 Y3
		fmul	[ebx].M33								; Z3 P2 1  X3 Z2 Y3
		fxch	st(3)										; X3 P2 1  Z3 Z2 Y3
		faddp	st(5), st(0)						; P2 1  Z3 Z2 P3

		add	ecx, 8
		jnz	NextPoint

		faddp	st(3), st(0)						; 1  Z3 2  P3
		fistp	word ptr[edi+ecx-8]			; Z3 2  P3
		faddp	st(2), st(0)						; 2  3
		fistp	word ptr[edi+ecx-6]			; 3
		fistp	word ptr[edi+ecx-4]

		ret
RotTransListF	endp
	*/

	for (S32 i = 0; i < NbPoints; i++)
	{
		Dst[i].X = Src[i].X * Mat->F.M11 + Src[i].Y * Mat->F.M12 + Src[i].Z * Mat->F.M13 + Mat->F.TX;
		Dst[i].Y = Src[i].X * Mat->F.M21 + Src[i].Y * Mat->F.M22 + Src[i].Z * Mat->F.M23 + Mat->F.TY;
		Dst[i].Z = Src[i].X * Mat->F.M31 + Src[i].Y * Mat->F.M32 + Src[i].Z * Mat->F.M33 + Mat->F.TZ;
	}
}

// -----------------------------------------------------------------------------
Func_RotTransList *RotTransList = RotTransListF;

