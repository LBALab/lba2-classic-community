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

		public	C	RotateList

RotateList	dd	offset RotateListInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	RotateListI:PROC
		extrn	C	RotateListF:PROC
		;extrn	C	RotateListM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	RotateList(TYPE_MAT *Mat, S32 *dst, S32 *src, S32 n)	;

;#pragma aux RotateList	"*"		\
;	parm 	[ebx] [edi] [esi] [ecx]	\
;	modify	[eax edx]

RotateListInit proc

		mov	[ds:Chooser_RtnI], offset RotateListI	; Integer
		mov	[ds:Chooser_RtnF], offset RotateListF	; FPU
		;mov	[ds:Chooser_RtnM], offset RotateListM	; MMX
		mov	[ds:Chooser_Result], offset RotateList	; Pointer
		jmp	Chooser

RotateListInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
