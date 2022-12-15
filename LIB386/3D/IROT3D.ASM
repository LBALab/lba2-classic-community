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

		public	C	InverseRotatePoint

InverseRotatePoint	dd	offset InverseRotatePointInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	InverseRotatePointI:PROC
		extrn	C	LongInverseRotatePointF:PROC
		;extrn	C	InverseRotatePointM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	InverseRotatePoint(S32 *Mat, S32 x, S32 y, S32 z)	;

;#pragma aux InverseRotatePoint			\
;	parm caller	[esi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx edi]

InverseRotatePointInit proc

		mov	[ds:Chooser_RtnI], offset InverseRotatePointI	; Integer
		mov	[ds:Chooser_RtnF], offset LongInverseRotatePointF	; FPU
		;mov	[ds:Chooser_RtnM], offset InverseRotatePointM	; MMX
		mov	[ds:Chooser_Result], offset InverseRotatePoint	; Pointer
		jmp	Chooser

InverseRotatePointInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
