;*══════════════════════════════════════════════════════════════════════════*
		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	VESAMode:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser_RtnI:DWORD
		extrn	C	Chooser_RtnF:DWORD
		extrn	C	Chooser_RtnM:DWORD
		extrn	C	Chooser_Result:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		public	C	ClearBoxVESA

ClearBoxVESA	dd	offset ClearBoxVESAInit

	ASSUME DS:SEG ClearBoxVESAInit

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	ClearBoxVESAI:PROC
		extrn	C	ClearBoxVESAF:PROC
		;extrn	C	ClearBoxVESAM:PROC

		extrn	C	ClearBoxI:PROC
		extrn	C	ClearBoxF:PROC
		;extrn	C	ClearBoxM:PROC

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
		public	C	ClearBoxVESAInit

		ASSUME DS:SEG VESAMode

;*══════════════════════════════════════════════════════════════════════════*
;void	ClearBoxVESA(void *dst, U32 *TabOffDst, T_BOX *box) ;

;#pragma aux ClearBoxVESA	       	\
;	parm	[edi] [edx] [ebx]	\
;	modify	[eax ecx esi]

ClearBoxVESAInit	proc

		mov	[ds:Chooser_Result], offset ClearBoxVESA	; Pointer

		test	[VESAMode], 4000h
		jnz	OkLinear

		mov	[ds:Chooser_RtnI], offset ClearBoxVESAI	; Integer
		mov	[ds:Chooser_RtnF], offset ClearBoxVESAF	; FPU
		;mov	[ds:Chooser_RtnM], offset ClearBoxVESAM	; MMX

		jmp	Chooser
OkLinear:
		mov	[ds:Chooser_RtnI], offset ClearBoxI	; Integer
		mov	[ds:Chooser_RtnF], offset ClearBoxF	; FPU
		;mov	[ds:Chooser_RtnM], offset ClearBoxM	; MMX

		jmp	Chooser

ClearBoxVESAInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

