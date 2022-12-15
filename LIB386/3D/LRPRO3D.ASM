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

		public	C LongRotateProjectPoint3D

LongRotateProjectPoint3D	dd	offset	LongRotateProjectPoint3DInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongRotateProjectPoint3DI:PROC
		extrn	C	LongRotateProjectPoint3DF:PROC
		;extrn	C	LongRotateProjectPoint3DM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	LongRotateProjectPoint3D(S32 *Mat, S32 x, S32 y, S32 z)	;

;#pragma aux 	LongRotateProjectPoint3D	\
;	parm caller	[esi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx edi]

LongRotateProjectPoint3DInit proc

		mov	[ds:Chooser_RtnI], offset LongRotateProjectPoint3DI	; Integer
		mov	[ds:Chooser_RtnF], offset LongRotateProjectPoint3DF	; FPU
		;mov	[ds:Chooser_RtnM], offset LongRotateProjectPoint3DM	; MMX
		mov	[ds:Chooser_Result], offset LongRotateProjectPoint3D	; Pointer
		jmp	Chooser

LongRotateProjectPoint3DInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

