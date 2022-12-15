;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser_RtnI:DWORD
		extrn	C	Chooser_RtnM:DWORD
		extrn	C	Chooser_RtnF:DWORD
		extrn	C	Chooser_Result:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		public	C RotateProjectPointIso

RotateProjectPointIso	dd	offset	RotateProjectPointIsoInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	RotateProjectPointIsoI:PROC
		extrn	C	LongRotateProjectPointIsoF:PROC
		;extrn	C	RotateProjectPointIsoM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	RotateProjectPointIso(S32 *Mat, S32 x, S32 y, S32 z)	;

;#pragma aux 	RotateProjectPointIso	\
;	parm caller	[esi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx edi]

RotateProjectPointIsoInit proc

		mov	[ds:Chooser_RtnI], offset RotateProjectPointIsoI	; Integer
		mov	[ds:Chooser_RtnF], offset LongRotateProjectPointIsoF; FPU
		;mov	[ds:Chooser_RtnM], offset RotateProjectPointIsoM	; MMX
		mov	[ds:Chooser_Result], offset RotateProjectPointIso	; Pointer
		jmp	Chooser

RotateProjectPointIsoInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

