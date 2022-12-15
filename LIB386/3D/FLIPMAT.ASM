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

		public	C	FlipMatrix

FlipMatrix	dd	offset FlipMatrixInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	FlipMatrixI:PROC
		;extrn	C	FlipMatrixM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	FlipMatrix(S32 *MatDst, S32 *MatSrc) ;

;#pragma aux FlipMatrix	"*"		\
;	parm		[edi] [esi]	\
;	modify exact	[eax ebx ecx]

FlipMatrixInit proc

		mov	[ds:Chooser_RtnI], offset FlipMatrixI	; Integer or FPU
		;mov	[ds:Chooser_RtnM], offset FlipMatrixM	; MMX
		mov	[ds:Chooser_Result], offset FlipMatrix	; Pointer
		jmp	Chooser

FlipMatrixInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
