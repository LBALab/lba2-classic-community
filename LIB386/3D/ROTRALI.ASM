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

		public	C	RotTransList

RotTransList	dd	offset RotTransListInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	RotTransListI:PROC
		extrn	C	RotTransListF:PROC
		;extrn	C	RotTransListM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	RotTransList(TYPE_MAT *Mat, S32 *dst, S32 *src, S32 n)	;

;#pragma aux RotTransList	"*"	\
;	parm 	[ebx] [edi] [esi] [ecx]	\
;	modify	[eax edx]

RotTransListInit proc

		mov	[ds:Chooser_RtnI], offset RotTransListI	; Integer
		mov	[ds:Chooser_RtnF], offset RotTransListF	; FPU
		;mov	[ds:Chooser_RtnM], offset RotTransListM	; MMX
		mov	[ds:Chooser_Result], offset RotTransList	; Pointer
		jmp	Chooser

RotTransListInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
