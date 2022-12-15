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

		public	C	LongRotTransList

LongRotTransList	dd	offset LongRotTransListInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongRotTransListI:PROC
		extrn	C	LongRotTransListF:PROC
		;extrn	C	LongRotTransListM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	LongRotTransList(TYPE_MAT *Mat, S32 *dst, S32 *src, S32 n)	;

;#pragma aux LongRotTransList	"*"		\
;	parm 	[ebx] [edi] [esi] [ecx]	\
;	modify	[eax edx]

LongRotTransListInit proc

		mov	[ds:Chooser_RtnI], offset LongRotTransListI	; Integer
		mov	[ds:Chooser_RtnF], offset LongRotTransListF	; FPU
		;mov	[ds:Chooser_RtnM], offset LongRotTransListM	; MMX
		mov	[ds:Chooser_Result], offset LongRotTransList	; Pointer
		jmp	Chooser

LongRotTransListInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
