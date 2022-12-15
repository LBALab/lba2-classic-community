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

		public	C	InitMatrix3DS

InitMatrix3DS	dd	offset InitMatrix3DSInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	InitMatrix3DSI:PROC
		extrn	C	InitMatrix3DSF:PROC
		;extrn	C	InitMatrix3DSM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	InitMatrix3DS(S32 *MatDst, S32 alpha, S32 beta, S32 gamma) ;

;#pragma aux InitMatrix3DS	"*"		\
;	parm		[edi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx esi]

InitMatrix3DSInit proc

		mov	[ds:Chooser_RtnI], offset InitMatrix3DSI	; Integer
		mov	[ds:Chooser_RtnF], offset InitMatrix3DSF	; FPU
		;mov	[ds:Chooser_RtnM], offset InitMatrix3DSM	; MMX
		mov	[ds:Chooser_Result], offset InitMatrix3DS	; Pointer
		jmp	Chooser

InitMatrix3DSInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
