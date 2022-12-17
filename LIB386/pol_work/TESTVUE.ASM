			.386

PUBLIC	C		TestVuePoly


			.model SMALL, C

			.DATA
				; Chooser.asm
Extrn	C		Chooser_RtnI	:	DWORD
Extrn	C		Chooser_RtnF	:	DWORD
Extrn	C		Chooser_RtnM	:	DWORD
Extrn	C		Chooser_Result	:	DWORD

TestVuePoly		dd	Init_TestVuePoly




			.CODE
Extrn	C		Chooser		:	PROC
Extrn	C		TestVuePolyI	:	PROC
Extrn	C		TestVuePolyF	:	PROC
;Extrn	C		TestVuePolyM	:	PROC

Init_TestVuePoly	PROC
			mov	[ds:Chooser_RtnI],Offset TestVuePolyI	; Integer
			mov	[ds:Chooser_RtnF],Offset TestVuePolyF	; FPU
			;mov	[ds:Chooser_RtnM],Offset TestVuePolyM	; MMX
			mov	[ds:Chooser_Result],Offset TestVuePoly	; Pointer
			jmp	Chooser
Init_TestVuePoly	ENDP


;			The
			End

