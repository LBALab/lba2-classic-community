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

		public	C	LongRotateList

LongRotateList	dd	offset LongRotateListInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongRotateListI:PROC
		extrn	C	LongRotateListF:PROC
		;extrn	C	LongRotateListM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	LongRotateList(TYPE_MAT *Mat, S32 *dst, S32 *src, S32 n)	;

;#pragma aux LongRotateList	"*"		\
;	parm 	[ebx] [edi] [esi] [ecx]	\
;	modify	[eax edx]

LongRotateListInit proc

		mov	[ds:Chooser_RtnI], offset LongRotateListI	; Integer
		mov	[ds:Chooser_RtnF], offset LongRotateListF	; FPU
		;mov	[ds:Chooser_RtnM], offset LongRotateListM	; MMX
		mov	[ds:Chooser_Result], offset LongRotateList	; Pointer
		jmp	Chooser

LongRotateListInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
