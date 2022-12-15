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

		public	C	InitMatrixTransI

;*══════════════════════════════════════════════════════════════════════════*
;void	InitMatrixTransI(S32 *MatDst, S32 tx, S32 ty, S32 tz) ;

;#pragma aux InitMatrixTransI	"*"		\
;	parm		[edi] [eax] [ebx] [ecx]	\
;	modify exact	[]

InitMatrixTransI	proc

		mov	[edi].Struc_MatriceMAT.MAT_MTX, eax
		mov	[edi].Struc_MatriceMAT.MAT_MTY, ebx

		mov	[edi].Struc_MatriceMAT.MAT_MTZ, ecx

		ret

InitMatrixTransI	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
