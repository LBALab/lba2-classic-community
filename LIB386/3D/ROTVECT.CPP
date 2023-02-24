/*
;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C


;*══════════════════════════════════════════════════════════════════════════*
		.DATA

		extrn	C MatrixLib2:DWORD
		extrn	C InitMatrixStd:DWORD
		extrn	C LongRotatePoint:DWORD

		ASSUME  DS:SEG LongRotatePoint

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		public	C	RotateVector

;*══════════════════════════════════════════════════════════════════════════*

;*══════════════════════════════════════════════════════════════════════════*
;void RotateVector(S32 norme, S32 alpha, S32 beta, S32 gamma)	;

;#pragma aux RotateVector			\
;	parm		[edx] [eax] [ebx] [ecx]	\
;	modify		[esi edi]

RotateVector	proc

		mov	edi, offset MatrixLib2
		push	edx

		call	[InitMatrixStd]

		pop	ecx
		xor	eax, eax
		xor	ebx, ebx
		mov	esi, offset MatrixLib2

		jmp	[LongRotatePoint]

RotateVector	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

*/

#include <3D/ROTVECT.H>
#include <stdio.h>

void RotateVector(S32 norme, S32 alpha, S32 beta, S32 gamma)
{
 // TODO: Implement ASM
 printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}
