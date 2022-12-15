;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser_RtnI:DWORD
		extrn	C	Chooser_RtnF:DWORD
		;extrn	C	Chooser_RtnMF:DWORD
		extrn	C	Chooser_Result:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		public	C	ProjectList3D

ProjectList3D	dd	offset ProjectList3DInit

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	ProjectList3DI:PROC
		extrn	C	ProjectList3DF:PROC
		;extrn	C	ProjectList3DMF:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	ProjectList3D(S32 *Dst, S32 *Src, S32 NbPt, S32 OrgX, S32 OrgY, OrgZ);

;#pragma aux ProjectList3D			\
;	parm		[edi] [esi] [ecx]	\
;	modify		[eax ebx edx]

;WARING: OrgX, OrgY, OrgZ pushed on the stack

ProjectList3DInit	proc

		mov	[ds:Chooser_RtnI], offset ProjectList3DI	; Integer
		mov	[ds:Chooser_RtnF], offset ProjectList3DF	; FPU
		;mov	[ds:Chooser_RtnMF], offset ProjectList3DMF	; MMX+FPU
		mov	[ds:Chooser_Result], offset ProjectList3D	; Pointer
		jmp	Chooser

ProjectList3DInit	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END

