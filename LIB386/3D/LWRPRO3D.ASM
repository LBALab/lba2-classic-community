;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser_RtnI:DWORD
		extrn	C	Chooser_RtnF:DWORD
		extrn	C	Chooser_Result:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		public	C LongWorldRotateProjectPoint3D

LongWorldRotateProjectPoint3D	dd	offset	LongWorldRotateProjectPoint3DInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongWorldRotateProjectPoint3DI:PROC
		extrn	C	LongWorldRotateProjectPoint3DF:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	LongWorldRotateProjectPoint3D(S32 x, S32 y, S32 z)	;

;#pragma aux 	LongWorldRotateProjectPoint3D	\
;	parm caller	[eax] [ebx] [ecx]	\
;	modify		[edx esi edi]

LongWorldRotateProjectPoint3DInit proc

		mov	[ds:Chooser_RtnI], offset LongWorldRotateProjectPoint3DI	; Integer
		mov	[ds:Chooser_RtnF], offset LongWorldRotateProjectPoint3DF	; FPU
		mov	[ds:Chooser_Result], offset LongWorldRotateProjectPoint3D	; Pointer
		jmp	Chooser

LongWorldRotateProjectPoint3DInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

