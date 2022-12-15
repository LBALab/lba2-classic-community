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

		public	C LongProjectPoint3D

LongProjectPoint3D	dd	offset	LongProjectPoint3DInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongProjectPoint3DI:PROC
		extrn	C	LongProjectPoint3DF:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	LongProjectPoint3D(S32 x, S32 y, S32 z)		;

;#pragma aux LongProjectPoint3D		\
;	parm		[eax] [ebx] [ecx]	\
;	modify		[edx]

LongProjectPoint3DInit proc

		mov	[ds:Chooser_RtnI], offset LongProjectPoint3DI	; Integer
		mov	[ds:Chooser_RtnF], offset LongProjectPoint3DF	; FPU
		;mov	[ds:Chooser_RtnM], offset LongProjectPoint3DI	; MMX = Integer
		mov	[ds:Chooser_Result], offset LongProjectPoint3D	; Pointer
		jmp	Chooser

LongProjectPoint3DInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

