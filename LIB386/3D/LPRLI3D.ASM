;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser_RtnI:DWORD
		extrn	C	Chooser_RtnF:DWORD
		extrn	C	Chooser_RtnMF:DWORD
		extrn	C	Chooser_Result:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		public	C	LongProjectList3D

LongProjectList3D	dd	offset LongProjectList3DInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongProjectList3DI:PROC
		extrn	C	LongProjectList3DF:PROC
		;extrn	C	LongProjectList3DMF:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	LongProjectList3D(S32 *Dst, S32 *Src, S32 NbPt, S32 OrgX, S32 OrgY, OrgZ);

;#pragma aux LongProjectList3D			\
;	parm		[edi] [esi] [ecx]	\
;	modify		[eax ebx edx]

;WARING: OrgX, OrgY, OrgZ pushed on the stack

LongProjectList3DInit	proc

		mov	[ds:Chooser_RtnI], offset LongProjectList3DI	; Integer
		mov	[ds:Chooser_RtnF], offset LongProjectList3DF	; FPU
		;mov	[ds:Chooser_RtnMF], offset LongProjectList3DMF	; MMX+FPU
		mov	[ds:Chooser_Result], offset LongProjectList3D	; Pointer
		jmp	Chooser

LongProjectList3DInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

