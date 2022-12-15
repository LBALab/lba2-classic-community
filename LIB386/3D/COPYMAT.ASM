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

		public	C	CopyMatrix

CopyMatrix	dd	offset CopyMatrixInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	CopyMatrixI:PROC
		extrn	C	CopyMatrixF:PROC
		;extrn	C	CopyMatrixM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	CopyMatrix(S32 *MatDst, S32 *MatSrc) ;

;#pragma aux CopyMatrix	"*"		\
;	parm		[edi] [esi]	\
;	modify exact	[eax ebx ecx]

CopyMatrixInit proc

		mov	[ds:Chooser_RtnI], offset CopyMatrixI	; Integer
		mov	[ds:Chooser_RtnF], offset CopyMatrixF	; FPU
		;mov	[ds:Chooser_RtnM], offset CopyMatrixM	; MMX
		mov	[ds:Chooser_Result], offset CopyMatrix	; Pointer
		jmp	Chooser

CopyMatrixInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
