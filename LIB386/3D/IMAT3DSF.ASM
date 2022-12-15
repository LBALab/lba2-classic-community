;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

		extrn	C SinTabF:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public	C	InitMatrix3DSF
		;ASSUME ds:SEG COSB

;*══════════════════════════════════════════════════════════════════════════*
;void	InitMatrix3DSF(S32 *MatDst, S32 alpha, S32 beta, S32 gamma) ;

;#pragma aux InitMatrix3DSF	"*"		\
;	parm		[edi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx esi]

InitMatrix3DSF	PROC

		fld	[edi].Struc_MatriceMAT.MAT_M11
		fld	[edi].Struc_MatriceMAT.MAT_M12
		fld	[edi].Struc_MatriceMAT.MAT_M13

		fld	[esi].Struc_MatriceMAT.MAT_M11
		fld	[esi].Struc_MatriceMAT.MAT_M21
		fld	[esi].Struc_MatriceMAT.MAT_M31

		and	eax,4095
		and	ebx,4095

		shl	eax,2
		and	ecx,4095

		shl	ebx,2

		shl	ecx,2

SINA		EQU	[ds:SinTabF+eax]		; Mettre l'adresse dans les reg
COSA		EQU	[ds:SinTabF+eax+1024*4]	; pour l'optimisation
SINB		EQU	[ds:SinTabF+ebx]
COSB		EQU	[ds:SinTabF+ebx+1024*4]
SING		EQU	[ds:SinTabF+ecx]
COSG		EQU	[ds:SinTabF+ecx+1024*4]


		fld	COSG
		fmul	COSB
		fld	SINA
		fmul	SING
		fmul	SINB
		fsubp	st(1),st
		fstp	[edi].Struc_MatriceMAT.MAT_M11		; M11 = Cos(Gamma)*Cos(Beta)
						;      -Sin(Alpha)*Sin(Gamma)*Sin(Beta)

		fld	COSA
		fmul	SING
		fchs
		fstp	[edi].Struc_MatriceMAT.MAT_M12		; M12 = -Cos(Alpha)*Sin(Gamma)

		fld	SINA
		fmul	SING
		fmul	COSB
		fld	COSG
		fmul	SINB
		faddp	st(1),st
		fstp	[edi].Struc_MatriceMAT.MAT_M13		; M13 = Sin(Alpha)*Sin(Gamma)*Cos(Beta)
						;      +Cos(Gamma)*Sin(Beta)

		fld	SING
		fmul	COSB
		fld	SINA
		fmul	COSG
		fmul	SINB
		faddp	st(1),st
		fstp	[edi].Struc_MatriceMAT.MAT_M21		; M21 = Sin(Gamma)*Cos(Beta)
						;      +Sin(Alpha)*Cos(Gamma)*Sin(Beta)

		fld	COSA
		fmul	COSG
		fstp	[edi].Struc_MatriceMAT.MAT_M22		; M22 = Cos(Alpha)*Cos(Gamma)

		fld	SING
		fmul	SINB
		fld	SINA
		fmul	COSG
		fmul	COSB
		fsubp	st(1),st
		fstp	[edi].Struc_MatriceMAT.MAT_M23		; M23 = Sin(Gamma)*Sin(Beta)
						;      -Sin(Alpha)*Cos(Gamma)*Cos(Beta)

		fld	COSA
		fmul	SINB
		fchs
		fstp	[edi].Struc_MatriceMAT.MAT_M31		; M31 = -Cos(Alpha)*Sin(Beta)

					; *** A remplacer par 2 MOV!!!!!!
		fld	SINA
		fstp	[edi].Struc_MatriceMAT.MAT_M32		; M32 = Sin(Alpha)

		fld	COSA
		fmul	COSB

		xor	eax, eax

		fstp	[edi].Struc_MatriceMAT.MAT_M33		; M33 = Cos(Alpha)*Cos(Beta)

		mov	[edi].Struc_MatriceMAT.MAT_MTX, eax
		mov	[edi].Struc_MatriceMAT.MAT_MTY, eax

		mov	[edi].Struc_MatriceMAT.MAT_MTZ, eax

		ret
InitMatrix3DSF	ENDP

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
