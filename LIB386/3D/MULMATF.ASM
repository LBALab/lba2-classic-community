;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	MulMatrixF

;*══════════════════════════════════════════════════════════════════════════*
;void	MulMAtrixF(S32 *MatDst, S32 *MatSrc1, S32* MatSrc2) ;

;#pragma aux MulMAtrixF		       		\
;	parm		[edi] [esi] [ebx]	\
;	modify exact	[]

MulMatrixF	PROC

		xor	eax, eax
					;  0   1   2   3   4   5   6   7
		fld	[esi].Struc_MatriceMAT.MAT_M11
		fmul	[ebx].Struc_MatriceMAT.MAT_M11	; M11
		fld	[esi].Struc_MatriceMAT.MAT_M11
		fmul	[ebx].Struc_MatriceMAT.MAT_M12	; M12 M11
		fld	[esi].Struc_MatriceMAT.MAT_M11
		fmul	[ebx].Struc_MatriceMAT.MAT_M13	; M13 M12 M11

		fld	[esi].Struc_MatriceMAT.MAT_M12
		fmul	[ebx].Struc_MatriceMAT.MAT_M21	; M21 M13 M12 M11
		fld	[esi].Struc_MatriceMAT.MAT_M12
		fmul	[ebx].Struc_MatriceMAT.MAT_M22	; M22 M21 M13 M12 M11
		fld	[esi].Struc_MatriceMAT.MAT_M12
		fmul	[ebx].Struc_MatriceMAT.MAT_M23	; M23 M22 M21 M13 M12 M11
		fxch	st(2)		; M21 M22 M23 M13 M12 M11
		faddp	st(5), st	; M22 M23 M13 M12 s1
		faddp	st(3), st	; M23 M13 s2  s1
		faddp	st(1), st	; s3  s2  s1

		fld	[esi].Struc_MatriceMAT.MAT_M13
		fmul	[ebx].Struc_MatriceMAT.MAT_M31	; M31 s3  s2  s1
		fld	[esi].Struc_MatriceMAT.MAT_M13
		fmul	[ebx].Struc_MatriceMAT.MAT_M32	; M32 M31 s3  s2  s1
		fld	[esi].Struc_MatriceMAT.MAT_M13
		fmul	[ebx].Struc_MatriceMAT.MAT_M33	; M33 M32 M31 s3  s2  s1
		fxch	st(2)		; M31 M32 M33 s3  s2  s1
		faddp	st(5), st	; M32 M33 s3  s2  S1
		faddp	st(3), st	; M33 s3  S2  S1
		faddp	st(1), st	; S3  S2  S1
		fxch	st(2)		; S1  S2  S3

		fstp	[edi].Struc_MatriceMAT.MAT_M11
		fstp	[edi].Struc_MatriceMAT.MAT_M12
		fstp	[edi].Struc_MatriceMAT.MAT_M13


					;  0   1   2   3   4   5   6   7
		fld	[esi].Struc_MatriceMAT.MAT_M21
		fmul	[ebx].Struc_MatriceMAT.MAT_M11	; M11
		fld	[esi].Struc_MatriceMAT.MAT_M21
		fmul	[ebx].Struc_MatriceMAT.MAT_M12	; M12 M11
		fld	[esi].Struc_MatriceMAT.MAT_M21
		fmul	[ebx].Struc_MatriceMAT.MAT_M13	; M13 M12 M11

		fld	[esi].Struc_MatriceMAT.MAT_M22
		fmul	[ebx].Struc_MatriceMAT.MAT_M21	; M21 M13 M12 M11
		fld	[esi].Struc_MatriceMAT.MAT_M22
		fmul	[ebx].Struc_MatriceMAT.MAT_M22	; M22 M21 M13 M12 M11
		fld	[esi].Struc_MatriceMAT.MAT_M22
		fmul	[ebx].Struc_MatriceMAT.MAT_M23	; M23 M22 M21 M13 M12 M11
		fxch	st(2)		; M21 M22 M23 M13 M12 M11
		faddp	st(5), st	; M22 M23 M13 M12 s1
		faddp	st(3), st	; M23 M13 s2  s1
		faddp	st(1), st	; s3  s2  s1

		fld	[esi].Struc_MatriceMAT.MAT_M23
		fmul	[ebx].Struc_MatriceMAT.MAT_M31	; M31 s3  s2  s1
		fld	[esi].Struc_MatriceMAT.MAT_M23
		fmul	[ebx].Struc_MatriceMAT.MAT_M32	; M32 M31 s3  s2  s1
		fld	[esi].Struc_MatriceMAT.MAT_M23
		fmul	[ebx].Struc_MatriceMAT.MAT_M33	; M33 M32 M31 s3  s2  s1
		fxch	st(2)		; M31 M32 M33 s3  s2  s1
		faddp	st(5), st	; M32 M33 s3  s2  S1
		faddp	st(3), st	; M33 s3  S2  S1
		faddp	st(1), st	; S3  S2  S1
		fxch	st(2)		; S1  S2  S3

		fstp	[edi].Struc_MatriceMAT.MAT_M21
		fstp	[edi].Struc_MatriceMAT.MAT_M22
		fstp	[edi].Struc_MatriceMAT.MAT_M23


					;  0   1   2   3   4   5   6   7
		fld	[esi].Struc_MatriceMAT.MAT_M31
		fmul	[ebx].Struc_MatriceMAT.MAT_M11	; M11
		fld	[esi].Struc_MatriceMAT.MAT_M31
		fmul	[ebx].Struc_MatriceMAT.MAT_M12	; M12 M11
		fld	[esi].Struc_MatriceMAT.MAT_M31
		fmul	[ebx].Struc_MatriceMAT.MAT_M13	; M13 M12 M11

		fld	[esi].Struc_MatriceMAT.MAT_M32
		fmul	[ebx].Struc_MatriceMAT.MAT_M21	; M21 M13 M12 M11
		fld	[esi].Struc_MatriceMAT.MAT_M32
		fmul	[ebx].Struc_MatriceMAT.MAT_M22	; M22 M21 M13 M12 M11
		fld	[esi].Struc_MatriceMAT.MAT_M32
		fmul	[ebx].Struc_MatriceMAT.MAT_M23	; M23 M22 M21 M13 M12 M11
		fxch	st(2)		; M21 M22 M23 M13 M12 M11
		faddp	st(5), st	; M22 M23 M13 M12 s1
		faddp	st(3), st	; M23 M13 s2  s1
		faddp	st(1), st	; s3  s2  s1

		fld	[esi].Struc_MatriceMAT.MAT_M33
		fmul	[ebx].Struc_MatriceMAT.MAT_M31	; M31 s3  s2  s1
		fld	[esi].Struc_MatriceMAT.MAT_M33
		fmul	[ebx].Struc_MatriceMAT.MAT_M32	; M32 M31 s3  s2  s1
		fld	[esi].Struc_MatriceMAT.MAT_M33
		fmul	[ebx].Struc_MatriceMAT.MAT_M33	; M33 M32 M31 s3  s2  s1
		fxch	st(2)		; M31 M32 M33 s3  s2  s1
		faddp	st(5), st	; M32 M33 s3  s2  S1
		faddp	st(3), st	; M33 s3  S2  S1
		faddp	st(1), st	; S3  S2  S1
		fxch	st(2)		; S1  S2  S3

		fstp	[edi].Struc_MatriceMAT.MAT_M31
		fstp	[edi].Struc_MatriceMAT.MAT_M32
		fstp	[edi].Struc_MatriceMAT.MAT_M33

		mov	[edi].Struc_MatriceMAT.MAT_MTX, eax
		mov	[edi].Struc_MatriceMAT.MAT_MTY, eax

		mov	[edi].Struc_MatriceMAT.MAT_MTZ, eax

		ret
MulMatrixF	ENDP

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
