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

		public	C	MulMatrix

MulMatrix	dd	offset MulMatrixInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	MulMatrixI:PROC
		extrn	C	MulMatrixF:PROC
		;extrn	C	MulMatrixM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	MulMAtrix(S32 *MatDst, S32 *MatSrc1, S32* MatSrc2) ;

;#pragma aux MulMAtrix		       		\
;	parm		[edi] [esi] [ebx]	\
;	modify exact	[eax ecx edx]

; GOODIE: MatDst CAN BE EQUAL to MatSrc1

MulMatrixInit	proc

		mov	[ds:Chooser_RtnI], offset MulMatrixI	; Integer
		mov	[ds:Chooser_RtnF], offset MulMatrixF	; FPU
		;mov	[ds:Chooser_RtnM], offset MulMatrixM	; MMX
		mov	[ds:Chooser_Result], offset MulMatrix	; Pointer
		jmp	Chooser

MulMatrixInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

