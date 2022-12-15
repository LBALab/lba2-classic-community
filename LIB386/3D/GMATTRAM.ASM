;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	GetMatrixTransM

;*══════════════════════════════════════════════════════════════════════════*
;void	GetMatrixTransM(S32 *MatDst, S32 *tx, S32 *ty, S32 *tz) ;

;#pragma aux GetMatrixTransM	"*"		\
;	parm		[esi] [eax] [ebx] [ecx]	\
;	modify exact	[edx esi edi]

GetMatrixTransM	PROC

		mov	edx, [esi+32]
		mov	edi, [esi+36]

		mov	esi, [esi+40]
		mov	[eax], edx

		mov	[ebx], edi
		mov	[ecx], esi

		ret

GetMatrixTransM	ENDP

;*══════════════════════════════════════════════════════════════════════════*
;			The
			End
