;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	CopyMatrixI

;*══════════════════════════════════════════════════════════════════════════*
;void	CopyMatrixI(S32 *MatDst, S32 *MatSrc) ;

;#pragma aux CopyMatrixI	"*"	\
;	parm		[edi] [esi]	\
;	modify exact	[eax ebx ecx]

CopyMatrixI	proc

		mov	eax, [esi].Struc_MatriceMAT.MAT_M11
		mov	ebx, [esi].Struc_MatriceMAT.MAT_M12
		mov	ecx, [esi].Struc_MatriceMAT.MAT_M13
		mov	[edi].Struc_MatriceMAT.MAT_M11, eax
		mov	[edi].Struc_MatriceMAT.MAT_M12, ebx
		mov	[edi].Struc_MatriceMAT.MAT_M13, ecx

		mov	eax, [esi].Struc_MatriceMAT.MAT_M21
		mov	ebx, [esi].Struc_MatriceMAT.MAT_M22
		mov	ecx, [esi].Struc_MatriceMAT.MAT_M23
		mov	[edi].Struc_MatriceMAT.MAT_M21, eax
		mov	[edi].Struc_MatriceMAT.MAT_M22, ebx
		mov	[edi].Struc_MatriceMAT.MAT_M23, ecx

		mov	eax, [esi].Struc_MatriceMAT.MAT_M31
		mov	ebx, [esi].Struc_MatriceMAT.MAT_M32
		mov	ecx, [esi].Struc_MatriceMAT.MAT_M33
		mov	[edi].Struc_MatriceMAT.MAT_M31, eax
		mov	[edi].Struc_MatriceMAT.MAT_M32, ebx
		mov	[edi].Struc_MatriceMAT.MAT_M33, ecx

		ret

CopyMatrixI	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
