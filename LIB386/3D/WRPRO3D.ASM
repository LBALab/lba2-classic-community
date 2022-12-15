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
		public	C WorldRotateProjectPoint3D

WorldRotateProjectPoint3D	dd	offset	WorldRotateProjectPoint3DInit

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	WorldRotateProjectPoint3DI:PROC
		extrn	C	LongWorldRotateProjectPoint3DF:PROC

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	WorldRotateProjectPoint3D(S32 x, S32 y, S32 z)	;

;#pragma aux RotatePoint			\
;	parm caller	[eax] [ebx] [ecx]	\
;	modify		[edx esi edi]

WorldRotateProjectPoint3DInit proc

		mov	[ds:Chooser_RtnI], offset WorldRotateProjectPoint3DI	; Integer
		mov	[ds:Chooser_RtnF], offset LongWorldRotateProjectPoint3DF	; FPU
		mov	[ds:Chooser_Result], offset WorldRotateProjectPoint3D	; Pointer
		jmp	Chooser

WorldRotateProjectPoint3DInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

