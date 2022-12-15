;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	InitMatrixTransM

;*══════════════════════════════════════════════════════════════════════════*
;void	InitMatrixTransM(S32 *MatDst, S32 tx, S32 ty, S32 tz) ;

;#pragma aux InitMatrixTransM	"*"		\
;	parm		[edi] [eax] [ebx] [ecx]	\
;	modify exact	[]

InitMatrixTransM	PROC

			mov	[edi+32], eax
			mov	[edi+36], ebx

			shl	ebx, 16
			and	eax, 0FFFFh

			mov	[edi+40], ecx
			or	eax, ebx

			and	ecx, 0FFFFh
			mov	[edi+24], eax

			mov	[edi+28], ecx

			ret

InitMatrixTransM	ENDP

;*══════════════════════════════════════════════════════════════════════════*
;			The
			End
