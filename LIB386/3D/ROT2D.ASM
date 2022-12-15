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

		public	C	Rotate

Rotate	dd	offset RotateInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	RotateI:PROC
		extrn	C	LongRotateF:PROC
		;extrn	C	RotateM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	Rotate(S32 x, S32 z, S32 angle)	;

;#pragma	aux	Rotate			\
;	parm		[eax] [ecx] [edx]	\
;	modify		[ebx esi edi]

RotateInit	proc

		mov	[ds:Chooser_RtnI], offset RotateI		; Integer
		mov	[ds:Chooser_RtnF], offset LongRotateF	; FPU
		;mov	[ds:Chooser_RtnM], offset RotateM		; MMX
		mov	[ds:Chooser_Result], offset Rotate		; Pointer
		jmp	Chooser

RotateInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
