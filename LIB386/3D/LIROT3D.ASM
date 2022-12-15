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

		public	C	LongInverseRotatePoint

LongInverseRotatePoint	dd	offset LongInverseRotatePointInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongInverseRotatePointI:PROC
		extrn	C	LongInverseRotatePointF:PROC
		;extrn	C	LongInverseRotatePointM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	LongInverseRotatePoint(S32 *Mat, S32 x, S32 y, S32 z)	;

;#pragma aux LongInverseRotatePoint			\
;	parm caller	[esi] [eax] [ebx] [ecx]		\
;	modify exact	[eax ebx ecx edx edi]

LongInverseRotatePointInit proc

		mov	[ds:Chooser_RtnI], offset LongInverseRotatePointI	; Integer
		mov	[ds:Chooser_RtnF], offset LongInverseRotatePointF	; FPU
		;mov	[ds:Chooser_RtnM], offset LongInverseRotatePointM	; MMX
		mov	[ds:Chooser_Result], offset LongInverseRotatePoint	; Pointer
		jmp	Chooser

LongInverseRotatePointInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
