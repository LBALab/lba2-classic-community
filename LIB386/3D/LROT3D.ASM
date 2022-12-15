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

		public	C	LongRotatePoint

LongRotatePoint	dd	offset LongRotatePointInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongRotatePointI:PROC
		extrn	C	LongRotatePointF:PROC
		;extrn	C	LongRotatePointM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	LongRotatePoint(S32 *Mat, S32 x, S32 y, S32 z)	;

;#pragma aux LongRotatePoint			\
;	parm caller	[esi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx edi]

LongRotatePointInit proc

		mov	[ds:Chooser_RtnI], offset LongRotatePointI	; Integer
		mov	[ds:Chooser_RtnF], offset LongRotatePointF	; FPU
		;mov	[ds:Chooser_RtnM], offset LongRotatePointM	; MMX
		mov	[ds:Chooser_Result], offset LongRotatePoint; Pointer
		jmp	Chooser

LongRotatePointInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
