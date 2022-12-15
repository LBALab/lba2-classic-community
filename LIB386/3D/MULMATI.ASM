;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		public	C	MulMatrixI

;*══════════════════════════════════════════════════════════════════════════*
;void	MulMAtrixI(S32 *MatDst, S32 *MatSrc1, S32* MatSrc2) ;

;#pragma aux MulMAtrixI		       		\
;	parm		[edi] [esi] [ebx]	\
;	modify exact	[eax ecx edx]

MulMatrixI	proc uses ebp

		mov	eax, [esi].Struc_MatriceMAT.MAT_M11
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M12

		imul	eax, [ebx].Struc_MatriceMAT.MAT_M11
		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M21

		add	eax, ebp
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M13

		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M31

		add	eax, ebp
		mov	ecx, [esi].Struc_MatriceMAT.MAT_M11

		mov	ebp, [esi].Struc_MatriceMAT.MAT_M12

		imul	ecx, [ebx].Struc_MatriceMAT.MAT_M12
		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M22

		add	ecx, ebp
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M13

		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M32

		sar	eax, 14
		mov	edx, [esi].Struc_MatriceMAT.MAT_M11

		adc	eax, 0
		add	ecx, ebp

		sar	ecx, 14
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M12

		adc	ecx, 0

		imul	edx, [ebx].Struc_MatriceMAT.MAT_M13
		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M23

		add	edx, ebp
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M13

		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M33

		add	edx, ebp
		mov	[edi].Struc_MatriceMAT.MAT_M11, eax

		sar	edx, 14
		mov	[edi].Struc_MatriceMAT.MAT_M12, ecx

		adc	edx, 0
		mov	eax, [esi].Struc_MatriceMAT.MAT_M21

		mov	[edi].Struc_MatriceMAT.MAT_M13, edx
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M22

		imul	eax, [ebx].Struc_MatriceMAT.MAT_M11
		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M21

		add	eax, ebp
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M23

		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M31

		add	eax, ebp
		mov	ecx, [esi].Struc_MatriceMAT.MAT_M21

		sar	eax, 14
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M22

		adc	eax, 0

		imul	ecx, [ebx].Struc_MatriceMAT.MAT_M12
		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M22

		add	ecx, ebp
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M23

		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M32

		add	ecx, ebp
		mov	edx, [esi].Struc_MatriceMAT.MAT_M21

		mov	ebp, [esi].Struc_MatriceMAT.MAT_M22

		imul	edx, [ebx].Struc_MatriceMAT.MAT_M13
		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M23

		add	edx, ebp
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M23

		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M33

		sar	ecx, 14
		mov	[edi].Struc_MatriceMAT.MAT_M21, eax

		adc	ecx, 0
		add	edx, ebp

		sar	edx, 14
		mov	[edi].Struc_MatriceMAT.MAT_M22, ecx

		adc	edx, 0
		mov	eax, [esi].Struc_MatriceMAT.MAT_M31

		mov	[edi].Struc_MatriceMAT.MAT_M23, edx
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M32

		imul	eax, [ebx].Struc_MatriceMAT.MAT_M11
		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M21

		add	eax, ebp
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M33

		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M31

		add	eax, ebp
		mov	ecx, [esi].Struc_MatriceMAT.MAT_M31

		mov	ebp, [esi].Struc_MatriceMAT.MAT_M32

		imul	ecx, [ebx].Struc_MatriceMAT.MAT_M12
		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M22

		add	ecx, ebp
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M33

		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M32

		sar	eax, 14
		mov	edx, [esi].Struc_MatriceMAT.MAT_M31

		adc	eax, 0
		add	ecx, ebp

		sar	ecx, 14
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M32

		adc	ecx, 0

		imul	edx, [ebx].Struc_MatriceMAT.MAT_M13
		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M23

		add	edx, ebp
		mov	ebp, [esi].Struc_MatriceMAT.MAT_M33

		imul	ebp, [ebx].Struc_MatriceMAT.MAT_M33

		mov	[edi].Struc_MatriceMAT.MAT_M31, eax
		xor	eax, eax

		add	edx, ebp
		mov	[edi].Struc_MatriceMAT.MAT_M32, ecx

		sar	edx, 14
		mov	[edi].Struc_MatriceMAT.MAT_MTX, eax

		adc	edx, eax
		mov	[edi].Struc_MatriceMAT.MAT_MTY, eax

		mov	[edi].Struc_MatriceMAT.MAT_M33, edx
		mov	[edi].Struc_MatriceMAT.MAT_MTZ, eax

		ret

MulMatrixI	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

