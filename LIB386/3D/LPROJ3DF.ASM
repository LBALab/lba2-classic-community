;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include	MAT.INC

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C CameraX:DWORD
		extrn	C CameraY:DWORD
		extrn	C CameraZ:DWORD

		extrn	C CameraXr:DWORD
		extrn	C CameraYr:DWORD
		extrn	C CameraZr:DWORD

		extrn	C CameraZrClip:DWORD

		extrn	C FRatioX:DWORD
		extrn	C FRatioY:DWORD

		extrn	C XCentre:DWORD
		extrn	C YCentre:DWORD

		extrn	C Xp:DWORD
		extrn	C Yp:DWORD

		ASSUME  DS:SEG CameraX

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		public	C	LongProjectPoint3DF

;*══════════════════════════════════════════════════════════════════════════*
;void	LongProjectPoint3DF(S32 x, S32 y, S32 z)		;

;#pragma aux LongProjectPoint3DF		\
;	parm		[eax] [ebx] [ecx]	\
;	modify		[edx]

LongProjectPoint3DF proc

		cmp	ecx, [CameraZrClip]	; -Z
		jg	error

		mov	edx, [CameraZr]

		sub	edx, ecx

		push	 edx			; Z

		fild	dword ptr[esp]		; Z

		; PENALTY 2 cycle on Pentium

		fdivr	[FRatioX]		; fx/Z

		sub	eax, [CameraXr]
		sub	ebx, [CameraYr]

		mov	[esp], eax
		push	ebx

		mov	ecx, [XCentre]
		mov	edx, [YCentre]

		fild	dword ptr[esp]		; Y   fx/Z

		fild	dword ptr[esp+4]	; X   Y   fx/Z

		fxch	st(2)			; fx/Z Y    X

		fmul	st(1),st		; fx/Z Y'   X

		; PENALTY 1 cycle on Pentium

		fmulp	st(2),st		; Y'   Xe

		; PENALTY 1 cycle on Pentium

		fmul	[FRatioY]		; Ye   Xe
		fxch	st(1)			; Xe   Ye

		fistp	dword ptr[esp+4]	; Ye

		fistp	dword ptr[esp]

		pop	ebx
		pop	eax

		add	eax, ecx
		add	ebx, edx

		mov	[Xp], eax
		mov	[Yp], ebx

		mov	eax, 1

		ret

error:          mov	ebx, 80000000h
		xor	eax, eax

		mov	[Xp], ebx
		mov	[Yp], ebx

		ret

LongProjectPoint3DF endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
