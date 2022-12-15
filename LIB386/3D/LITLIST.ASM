;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include	3D.INC

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

		extrn	C	Chooser_RtnI:DWORD
		extrn	C	Chooser_RtnF:DWORD
		extrn	C	Chooser_RtnM:DWORD
		extrn	C	Chooser_Result:DWORD


		public	C LightList
LightList	dd	offset LightListInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		Extrn	C	LightListI	:	PROC
		Extrn	C	LightListF	:	PROC
		;Extrn	C	LightListM	:	PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC


;*══════════════════════════════════════════════════════════════════════════*
;void	LightList(TYPE_MAT *Mat, S32 *dst, S32 *src, S32 n)	;

;#pragma aux LightList	"*"		\
;	parm 	[ebx] [edi] [esi] [ecx]	\
;	modify	[eax ebx edx]

LightListInit	proc

		mov	[ds:Chooser_RtnI],   offset LightListI	; Integer
		mov	[ds:Chooser_RtnF],   offset LightListF	; FPU
		;mov	[ds:Chooser_RtnM],   offset LightListM	; MMX
		mov	[ds:Chooser_Result], offset LightList	; Pointer
		jmp	Chooser

LightListInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
