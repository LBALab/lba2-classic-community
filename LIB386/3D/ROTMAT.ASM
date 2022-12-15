;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser_RtnI:DWORD
		extrn	C	Chooser_RtnF:DWORD
		extrn	C	Chooser_RtnM:DWORD
		extrn	C	Chooser_Result:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		public	C	RotateMatrix

;*══════════════════════════════════════════════════════════════════════════*

RotateMatrix	dd	offset RotateMatrixInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	RotateMatrixI:PROC
		extrn	C	RotateMatrixU:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*

		public	C	RotateMatrixInit

;*══════════════════════════════════════════════════════════════════════════*
;void	RotateMatrix(S32 *MatDst, S32 *MatSrc, S32 alpha, S32 beta, S32 gamma) ;

;#pragma aux RotateMatrix				\
;	parm		[edi] [esi] [eax] [ebx] [ecx]	\
;	modify		[edx]

RotateMatrixInit proc

		mov	[ds:Chooser_RtnI], offset RotateMatrixI	; Integer
		mov	[ds:Chooser_RtnF], offset RotateMatrixU	; FPU
		;mov	[ds:Chooser_RtnM], offset RotateMatrixU	; MMX
		mov	[ds:Chooser_Result], offset RotateMatrix	; Pointer
		jmp	Chooser

RotateMatrixInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
