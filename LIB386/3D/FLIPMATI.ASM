;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*

		.DATA


;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	FlipMatrixI

;*══════════════════════════════════════════════════════════════════════════*
;void	FlipMatrixI(S32 *MatDst, S32 *MatSrc) ;

;#pragma aux FlipMatrixI	"*"	\
;	parm		[edi] [esi]	\
;	modify exact	[eax ebx ecx]


; this routine can flip a matrix onto itself

FlipMatrixI	proc

		mov	eax, [esi].Struc_MatriceMAT.MAT_M11
		mov	ebx, [esi].Struc_MatriceMAT.MAT_M12

		mov	ecx, [esi].Struc_MatriceMAT.MAT_M21
		mov	[edi].Struc_MatriceMAT.MAT_M11, eax

		mov	[edi].Struc_MatriceMAT.MAT_M21, ebx
		mov	[edi].Struc_MatriceMAT.MAT_M12, ecx

		mov	eax, [esi].Struc_MatriceMAT.MAT_M31
		mov	ebx, [esi].Struc_MatriceMAT.MAT_M13

		mov	ecx, [esi].Struc_MatriceMAT.MAT_M33
		mov	[edi].Struc_MatriceMAT.MAT_M13, eax

		mov	[edi].Struc_MatriceMAT.MAT_M31, ebx
		mov	[edi].Struc_MatriceMAT.MAT_M33, ecx

		mov	eax, [esi].Struc_MatriceMAT.MAT_M22
		mov	ebx, [esi].Struc_MatriceMAT.MAT_M23

		mov	ecx, [esi].Struc_MatriceMAT.MAT_M32
		mov	[edi].Struc_MatriceMAT.MAT_M22, eax

		mov	[edi].Struc_MatriceMAT.MAT_M32, ebx
		mov	[edi].Struc_MatriceMAT.MAT_M23, ecx

		ret
FlipMatrixI	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
