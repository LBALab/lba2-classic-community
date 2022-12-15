;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

		extrn	C	SinTab:WORD
		extrn	C	CosTab:WORD

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	GetMatrixTransI

;*══════════════════════════════════════════════════════════════════════════*
;void	GetMatrixTransI(S32 *MatDst, S32 *tx, S32 *ty, S32 *tz) ;

;#pragma aux GetMatrixTransI	"*"		\
;	parm		[eds] [eax] [ebx] [ecx]	\
;	modify exact	[edx esi edi]

GetMatrixTransI	proc

		mov	edx, [esi].Struc_MatriceMAT.MAT_MTX
		mov	edi, [esi].Struc_MatriceMAT.MAT_MTY
		mov	esi, [esi].Struc_MatriceMAT.MAT_MTZ

		mov	[eax], edx
		mov	[ebx], edi
		mov	[ecx], esi

		ret

GetMatrixTransI	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
