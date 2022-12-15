;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	GetMatrixTransF

;*══════════════════════════════════════════════════════════════════════════*
;void	GetMatrixTransF(S32 *MatDst, S32 *tx, S32 *ty, S32 *tz) ;

;#pragma aux GetMatrixTransF	"*"		\
;	parm		[esi] [eax] [ebx] [ecx]	\
;	modify exact	[]

GetMatrixTransF	PROC

		fld	[esi].Struc_MatriceMAT.MAT_MTX	; X

		fld	[esi].Struc_MatriceMAT.MAT_MTY	; Y X

		fld	[esi].Struc_MatriceMAT.MAT_MTZ	; Z Y X

		fistp	dword ptr[ecx]	; Y X

		fistp	dword ptr[ebx]	; Y

		fistp	dword ptr[eax]

		ret

GetMatrixTransF	ENDP

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
