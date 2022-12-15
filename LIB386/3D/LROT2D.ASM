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

		public	C	LongRotate

LongRotate	dd	offset LongRotateInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongRotateI:PROC
		extrn	C	LongRotateF:PROC
		;extrn	C	LongRotateM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	LongRotate(S32 x, S32 z, S32 angle)	;

;#pragma	aux	LongRotate		\
;	parm		[eax] [ecx] [edx]	\
;	modify		[ebx esi edi]

LongRotateInit	proc

		mov	[ds:Chooser_RtnI], offset LongRotateI	; Integer
		mov	[ds:Chooser_RtnF], offset LongRotateF	; FPU
		;mov	[ds:Chooser_RtnM], offset LongRotateM	; MMX
		mov	[ds:Chooser_Result], offset LongRotate	; Pointer
		jmp	Chooser

LongRotateInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
