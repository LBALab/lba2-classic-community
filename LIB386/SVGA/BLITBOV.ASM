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
		public	C	BlitBoxVESA

BlitBoxVESA	dd	offset BlitBoxVESAInit

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	BlitBoxVESAI:PROC
		extrn	C	BlitBoxVESAF:PROC
		;extrn	C	BlitBoxVESAM:PROC

		extrn	C	BlitBoxI:PROC
		extrn	C	BlitBoxF:PROC
		;extrn	C	BlitBoxM:PROC

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
		public	C	BlitBoxVESAInit

;*══════════════════════════════════════════════════════════════════════════*
;void	BlitBoxVESA(void *dst, U32 *TabOffDst, void *box) ;

;#pragma aux BlitBoxVESA	       	\
;	parm	[edi] [edx] [esi]	\
;	modify	[eax ebx ecx]

BlitBoxVESAInit	proc

		mov	[ds:Chooser_Result], offset BlitBoxVESA	; Pointer

		test	[VESAMode], 4000h
		jnz	OkLinear

		mov	[ds:Chooser_RtnI], offset BlitBoxVESAI	; Integer
		mov	[ds:Chooser_RtnF], offset BlitBoxVESAF	; FPU
		;mov	[ds:Chooser_RtnM], offset BlitBoxVESAM	; MMX

		jmp	Chooser
OkLinear:
		mov	[ds:Chooser_RtnI], offset BlitBoxI		; Integer
		mov	[ds:Chooser_RtnF], offset BlitBoxF		; FPU
		;mov	[ds:Chooser_RtnM], offset BlitBoxM		; MMX

		jmp	Chooser

BlitBoxVESAInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

