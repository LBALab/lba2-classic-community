#include <3D/COPYMAT.H>
#include <string.h>

void CopyMatrixF(TYPE_MAT *MatDst, TYPE_MAT *MatSrc)
{
	/*
;void	CopyMatrixF(S32 *MatDst, S32 *MatSrc) ;

;#pragma aux CopyMatrixF	"*"	\
;	parm		[edi] [esi]	\
;	modify exact	[]

CopyMatrixF	proc
						; 0  1  2  3  4  5
		fld	qword ptr [esi]		; 0
		fld	qword ptr [esi+8]	; 8  0
		fld	qword ptr [esi+16]	; 16 8  0
		fld	qword ptr [esi+24]	; 24 16 8  0
		fld	qword ptr [esi+32]	; 32 24 16 8  0
		fld	qword ptr [esi+40]	; 40 32 24 16 8  0

		fxch	st(5)			; 0  32 24 16 8  40

		fstp	qword ptr [edi]

		fstp	qword ptr [edi+32]

		fstp	qword ptr [edi+24]

		fstp	qword ptr [edi+16]

		fstp	qword ptr [edi+8]

		fstp	qword ptr [edi+40]

		ret
CopyMatrixF	endp
	*/
	memcpy(MatDst, MatSrc, sizeof(TYPE_MAT));
}

// -----------------------------------------------------------------------------
Func_CopyFlipMatrix *CopyMatrix = CopyMatrixF; ///< Do Not call from C
