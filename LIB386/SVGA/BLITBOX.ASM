;*══════════════════════════════════════════════════════════════════════════*
		.386p
		OPTION	LANGUAGE:C
		OPTION	SCOPED

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*
		.DATA
;DGROUP		GROUP	_DATA

;_DATA		SEGMENT USE32 PARA PUBLIC 'DATA'

		

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser_RtnI:DWORD
		extrn	C	Chooser_RtnF:DWORD
		extrn	C	Chooser_RtnM:DWORD
		extrn	C	Chooser_Result:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		public	C	BlitBox

		ASSUME DS:SEG BlitBox

BlitBox	dd	offset BlitBoxInit

;*══════════════════════════════════════════════════════════════════════════*
;_DATA		ENDS

;*══════════════════════════════════════════════════════════════════════════*
.CODE
;_TEXT		SEGMENT	USE32 PARA PUBLIC 'CODE'
       		ASSUME	CS:FLAT, DS:FLAT, ES:FLAT, SS:FLAT

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	BlitBoxI:PROC
		extrn	C	BlitBoxF:PROC
		;extrn	C	BlitBoxM:PROC

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	BlitBox(void *dst, U32 *TabOffDst, void *src) ;

;#pragma aux BlitBox	       	       	\
;	parm	[edi] [edx] [esi]	\
;	modify	[eax ebx ecx]

BlitBoxInit	proc

		mov	[ds:Chooser_RtnI], offset BlitBoxI	; Integer
		mov	[ds:Chooser_RtnF], offset BlitBoxF	; FPU
		;mov	[ds:Chooser_RtnM], offset BlitBoxM	; MMX
		mov	[ds:Chooser_Result], offset BlitBox	; Pointer
		jmp	Chooser

BlitBoxInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;_TEXT		ENDS

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
