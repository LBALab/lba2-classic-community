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

		public	C	RotatePoint
		public	C	RotatePointNoMMX

RotatePoint	dd	offset RotatePointInit
RotatePointNoMMX dd	offset RotatePointInitNoMMX
; Useful for ObjectDisplay, bcoz within Object/BodyDisplay, we use the FPU,
; either for triangles and for normal transforms (environment mapping). And, if
; we would use the MMX for normals transforms, then we should run an EMMS, which can
; be worse than FPU transform...

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	RotatePointI:PROC
		extrn	C	LongRotatePointF:PROC
		;extrn	C	RotatePointM:PROC

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	Chooser:PROC

;*══════════════════════════════════════════════════════════════════════════*
;void	RotatePoint(S32 *Mat, S32 x, S32 y, S32 z)	;

;#pragma aux RotatePoint			\
;	parm caller	[esi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx edi]

RotatePointInit proc

		mov	[ds:Chooser_RtnI], offset RotatePointI	; Integer
		mov	[ds:Chooser_RtnF], offset LongRotatePointF	; FPU
		;mov	[ds:Chooser_RtnM], offset RotatePointM	; MMX
		mov	[ds:Chooser_Result], offset RotatePoint	; Pointer
		jmp	Chooser

RotatePointInit endp


RotatePointInitNoMMX proc

		mov	[ds:Chooser_RtnI], offset RotatePointI	; Integer
		mov	[ds:Chooser_RtnF], offset LongRotatePointF	; FPU
		mov	[ds:Chooser_Result], offset RotatePointNoMMX; Pointer
		jmp	Chooser

RotatePointInitNoMMX endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
