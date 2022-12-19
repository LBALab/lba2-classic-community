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
		public	C	FastCopy

FastCopy	dd	offset FastCopyInit

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	FastCopyI:PROC
		extrn	C	FastCopyF:PROC
		;extrn	C	FastCopyM:PROC

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	FastCopy(void *dst, void *src, U32 len) ;

;#pragma aux FastCopy	       	       	\
;	parm	[edi] [esi] [ecx]	\
;	modify	[eax ebx edx]

FastCopyInit	proc

		mov	[ds:Chooser_RtnI], offset FastCopyI	; Integer
		mov	[ds:Chooser_RtnF], offset FastCopyF	; FPU
		;mov	[ds:Chooser_RtnM], offset FastCopyM	; MMX
		mov	[ds:Chooser_Result], offset FastCopy	; Pointer
		jmp	Chooser

FastCopyInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
