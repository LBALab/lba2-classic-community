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

		public	C	InitMatrixStd

InitMatrixStd	dd	offset InitMatrixStdInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	InitMatrixStdI:PROC
		extrn	C	InitMatrixStdF:PROC
		;extrn	C	InitMatrixStdM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	InitMatrixStd(S32 *MatDst, S32 alpha, S32 beta, S32 gamma) ;

;#pragma aux InitMatrixStd	"*"		\
;	parm		[edi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx esi]

InitMatrixStdInit proc

		mov	[ds:Chooser_RtnI], offset InitMatrixStdI	; Integer
		mov	[ds:Chooser_RtnF], offset InitMatrixStdF	; FPU
		;mov	[ds:Chooser_RtnM], offset InitMatrixStdM	; MMX
		mov	[ds:Chooser_Result], offset InitMatrixStd	; Pointer
		jmp	Chooser

InitMatrixStdInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
