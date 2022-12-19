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
		public	C	FastSet

FastSet	dd	offset FastSetInit

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	FastSetI:PROC
		extrn	C	FastSetF:PROC
		;extrn	C	FastSetM:PROC

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	FastSet(void *dst, S32 val, U32 len) ;

;#pragma aux FastSet	       	       	\
;	parm	[edi] [eax] [ecx]	\
;	modify	[ebx edx esi]

FastSetInit	proc

		mov	[ds:Chooser_RtnI], offset FastSetI	; Integer
		mov	[ds:Chooser_RtnF], offset FastSetF	; FPU
		;mov	[ds:Chooser_RtnM], offset FastSetM	; MMX
		mov	[ds:Chooser_Result], offset FastSet; Pointer
		jmp	Chooser

FastSetInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
