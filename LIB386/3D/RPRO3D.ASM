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

		public	C RotateProjectPoint3D

RotateProjectPoint3D	dd	offset	RotateProjectPoint3DInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	RotateProjectPoint3DI:PROC
		extrn	C	LongRotateProjectPoint3DF:PROC
		;extrn	C	RotateProjectPoint3DM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	RotateProjectPoint3D(S32 *Mat, S32 x, S32 y, S32 z)	;

;#pragma aux RotatePoint			\
;	parm caller	[esi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx edi]

RotateProjectPoint3DInit proc

		mov	[ds:Chooser_RtnI], offset RotateProjectPoint3DI	; Integer
		mov	[ds:Chooser_RtnF], offset LongRotateProjectPoint3DF; FPU
		;mov	[ds:Chooser_RtnM], offset RotateProjectPoint3DM	; MMX
		mov	[ds:Chooser_Result], offset RotateProjectPoint3D	; Pointer
		jmp	Chooser

RotateProjectPoint3DInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

