;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include	3D.INC

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

		public	C LongProjectPoint
LongProjectPoint	dd	offset LongProjectPointInit

		ASSUME  DS:SEG LongProjectPoint


;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	LongProjectPoint3D:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		public	C	LongProjectPointInit

;*══════════════════════════════════════════════════════════════════════════*
LongProjectPointInit proc

		call	[LongProjectPoint3D]

		push	eax
		mov	eax, [LongProjectPoint3D]

		mov	[LongProjectPoint], eax
		pop	eax

		ret

LongProjectPointInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
