;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include MAT.INC

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C MatriceWorld:DWORD

		extrn	C CameraXr:DWORD
		extrn	C CameraYr:DWORD
		extrn	C CameraZr:DWORD

		extrn	C CameraZrClip:DWORD

		extrn	C LFactorX:DWORD
		extrn	C LFactorY:DWORD

		extrn	C XCentre:DWORD
		extrn	C YCentre:DWORD

		extrn	C	Xp:DWORD
		extrn	C	Yp:DWORD

		ASSUME  DS:SEG CameraXr

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public C	LongRotateProjectPoint3DI
		public C	LongWorldRotateProjectPoint3DI

;*══════════════════════════════════════════════════════════════════════════*
;void	LongWorldRotateProjectPoint3DI(S32 x, S32 y, S32 z)	;

;#pragma aux LongWorldRotateProjectPoint3DI	\
;	parm caller	[eax] [ebx] [ecx]	\
;	modify		[edx esi edi]

LongWorldRotateProjectPoint3DI proc

		mov	esi, offset MatriceWorld

;*══════════════════════════════════════════════════════════════════════════*
;void	LongRotateProjectPoint3DI(S32 *Mat, S32 x, S32 y, S32 z)	;

;#pragma aux LongRotateProjectPoint3DI		\
;	parm caller	[esi] [eax] [ebx] [ecx]	\
;	modify exact	[eax ebx ecx edx edi]

LongRotateProjectPoint3DI::

		push	ebp
		mov	[Yp], eax

		imul	[esi].Struc_MatriceMAT.MAT_M11

		mov     ebp, eax
 		mov	eax, [esi].Struc_MatriceMAT.MAT_M12
		mov     edi, edx

		imul	ebx

		add     ebp, eax
		mov	eax, [esi].Struc_MatriceMAT.MAT_M13
		adc     edi, edx

		imul	ecx

		add     ebp, eax
		mov     eax, [esi].Struc_MatriceMAT.MAT_M21
		adc     edi, edx

		imul    [Yp]
		shrd    ebp, edi, 14

		adc	ebp, 0
		mov     edi, edx
		mov	[Xp], ebp
		mov     ebp, eax
		mov	eax, [esi].Struc_MatriceMAT.MAT_M22

		imul	ebx

		add     ebp, eax
		mov	eax, [esi].Struc_MatriceMAT.MAT_M23
		adc     edi, edx

		imul	ecx

		add     ebp, eax
		mov     eax, [esi].Struc_MatriceMAT.MAT_M31
		adc     edi, edx

		imul    [Yp]
		shrd    ebp, edi, 14

		adc	ebp, 0
		mov     edi, edx
		mov	[Yp], ebp
		mov     ebp, eax
		mov	eax, [esi].Struc_MatriceMAT.MAT_M32

		imul	ebx

		add     ebp, eax
		mov	eax, [esi].Struc_MatriceMAT.MAT_M33
		adc     edi, edx

		imul	ecx

		add     eax, ebp
		pop	ebp
		adc     edx, edi

		shrd    eax, edx, 14

		adc	eax, 0

		cmp	eax, [CameraZrClip]	; -Z
		jg	error

		mov	ecx, eax

		mov	eax, [Xp]
		mov	ebx, [Yp]

		sub	eax, [CameraXr]
		sub	ebx, [CameraYr]

		neg	eax
		sub	ecx, [CameraZr]

		imul	[LFactorX]		; -X
		idiv	ecx			; / -Z
		add	eax, [XCentre]

		mov	[Xp], eax		; result

		mov	eax, ebx
		imul	[LFactorY]              ; Y
		idiv	ecx			; / -Z
		add	eax, [YCentre]

		mov	[Yp], eax		; result

		mov	eax, 1
		ret

error:          mov	ebx, 80000000h
		xor	eax, eax

		mov	[Xp], ebx
		mov	[Yp], ebx

		ret

LongWorldRotateProjectPoint3DI	endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
