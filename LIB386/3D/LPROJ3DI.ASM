;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

;*══════════════════════════════════════════════════════════════════════════*

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

;*══════════════════════════════════════════════════════════════════════════*

		public	C	LongProjectPoint3DI

;*══════════════════════════════════════════════════════════════════════════*
;void	LongProjectPoint3DI(S32 x, S32 y, S32 z)		;

;#pragma aux LongProjectPoint3DI		\
;	parm		[eax] [ebx] [ecx]	\
;	modify		[edx]

LongProjectPoint3DI proc

		cmp	ecx, [CameraZrClip]	; -Z
		jg	error

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

LongProjectPoint3DI endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END


