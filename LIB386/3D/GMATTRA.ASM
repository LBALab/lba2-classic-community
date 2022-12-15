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

		public	C	GetMatrixTrans

GetMatrixTrans	dd	offset GetMatrixTransGet

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	GetMatrixTransI:PROC
		extrn	C	GetMatrixTransF:PROC
		;extrn	C	GetMatrixTransM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	GetMatrixTrans(S32 *MatDst, S32 *tx, S32 *ty, S32 *tz) ;

;#pragma aux GetMatrixTrans	"*"		\
;	parm		[esi] [eax] [ebx] [ecx]	\
;	modify exact	[edx esi edi]

GetMatrixTransGet proc

		mov	[ds:Chooser_RtnI], offset GetMatrixTransI	; Integer
		mov	[ds:Chooser_RtnF], offset GetMatrixTransF	; FPU
		;mov	[ds:Chooser_RtnM], offset GetMatrixTransM	; MMX
		mov	[ds:Chooser_Result], offset GetMatrixTrans	; Pointer
		jmp	Chooser

GetMatrixTransGet endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
