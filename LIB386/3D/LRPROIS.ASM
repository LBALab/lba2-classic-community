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

		public	C LongRotateProjectPointIso

LongRotateProjectPointIso	dd	offset	LongRotateProjectPointIsoInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongRotateProjectPointIsoI:PROC
		extrn	C	LongRotateProjectPointIsoF:PROC
		;extrn	C	LongRotateProjectPointIsoM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	LongRotateProjectPointIso(S32 *Mat, S32 x, S32 y, S32 z)	;

;#pragma aux 	LongRotateProjectPointIso	\
;	parm caller	[esi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx edi]

LongRotateProjectPointIsoInit proc

		mov	[ds:Chooser_RtnI], offset LongRotateProjectPointIsoI	; Integer
		mov	[ds:Chooser_RtnF], offset LongRotateProjectPointIsoF	; FPU
		;mov	[ds:Chooser_RtnM], offset LongRotateProjectPointIsoM	; MMX
		mov	[ds:Chooser_Result], offset LongRotateProjectPointIso	; Pointer
		jmp	Chooser

LongRotateProjectPointIsoInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

