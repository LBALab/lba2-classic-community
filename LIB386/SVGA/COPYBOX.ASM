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
		public	C	CopyBox

CopyBox		dd	offset CopyBoxInit

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	CopyBoxI:PROC
		extrn	C	CopyBoxF:PROC
		;extrn	C	CopyBoxM:PROC

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	CopyBox(void *dst, void *src, U32 *TabOffDst, T_BOX *box) ;

;#pragma aux CopyBox	       	       	\
;	parm	[edi] [esi] [edx] [ebx]	\
;	modify	[eax ecx]

CopyBoxInit	proc

		mov	[ds:Chooser_RtnI], offset CopyBoxI	; Integer
		mov	[ds:Chooser_RtnF], offset CopyBoxF	; FPU
		;mov	[ds:Chooser_RtnM], offset CopyBoxM	; MMX
		mov	[ds:Chooser_Result], offset CopyBox; Pointer
		jmp	Chooser

CopyBoxInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

