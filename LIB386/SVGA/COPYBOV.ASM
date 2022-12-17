;*══════════════════════════════════════════════════════════════════════════*
		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	VESAMode:DWORD

		ASSUME DS:SEG VESAMode

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser_RtnI:DWORD
		extrn	C	Chooser_RtnF:DWORD
		extrn	C	Chooser_RtnM:DWORD
		extrn	C	Chooser_Result:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		public	C	CopyBoxVESA

CopyBoxVESA	dd	offset CopyBoxVESAInit

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	CopyBoxVESAI:PROC
		extrn	C	CopyBoxVESAF:PROC
		;extrn	C	CopyBoxVESAM:PROC

		extrn	C	CopyBoxI:PROC
		extrn	C	CopyBoxF:PROC
		;extrn	C	CopyBoxM:PROC

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
		public	C	CopyBoxVESAInit

;*══════════════════════════════════════════════════════════════════════════*
;void	CopyBoxVESA(void *dst, void *src, U32 *TabOffDst, T_BOX *box) ;

;#pragma aux CopyBoxVESA	       	       	\
;	parm	[edi] [esi] [edx] [ebx]	\
;	modify	[eax ecx]

CopyBoxVESAInit	proc

		mov	[ds:Chooser_Result], offset CopyBoxVESA	; Pointer

		test	[VESAMode], 4000h
		jnz	OkLinear

		mov	[ds:Chooser_RtnI], offset CopyBoxVESAI	; Integer
		mov	[ds:Chooser_RtnF], offset CopyBoxVESAF	; FPU
		;mov	[ds:Chooser_RtnM], offset CopyBoxVESAM	; MMX

		jmp	Chooser
OkLinear:
		mov	[ds:Chooser_RtnI], offset CopyBoxI		; Integer
		mov	[ds:Chooser_RtnF], offset CopyBoxF		; FPU
		;mov	[ds:Chooser_RtnM], offset CopyBoxM		; MMX

		jmp	Chooser

CopyBoxVESAInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

