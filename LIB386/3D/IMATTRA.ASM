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

		public	C	InitMatrixTrans

InitMatrixTrans	dd	offset InitMatrixTransInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	InitMatrixTransI:PROC
		extrn	C	InitMatrixTransF:PROC
		;extrn	C	InitMatrixTransM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	InitMatrixTrans(S32 *MatDst, S32 tx, S32 ty, S32 tz) ;

;#pragma aux InitMatrixTrans	"*"		\
;	parm		[edi] [eax] [ebx] [ecx]	\
;	modify exact	[]

InitMatrixTransInit proc

		mov	[ds:Chooser_RtnI], offset InitMatrixTransI	; Integer
		mov	[ds:Chooser_RtnF], offset InitMatrixTransF	; FPU
		;mov	[ds:Chooser_RtnM], offset InitMatrixTransM	; MMX
		mov	[ds:Chooser_Result], offset InitMatrixTrans	; Pointer
		jmp	Chooser

InitMatrixTransInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
