;*══════════════════════════════════════════════════════════════════════════*
		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		public	C LongWorldRotateProjectPoint

;*══════════════════════════════════════════════════════════════════════════*
LongWorldRotateProjectPoint	dd	offset LongWorldRotateProjectPointInit

;*══════════════════════════════════════════════════════════════════════════*
temp		dd	?

		ASSUME  DS:SEG temp

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	LongWorldRotateProjectPoint3D:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		public	C	LongWorldRotateProjectPointInit

;*══════════════════════════════════════════════════════════════════════════*
LongWorldRotateProjectPointInit proc

		mov	[temp], eax	; save eax
		pop	eax		; get return address

		xchg	[temp], eax	; restore eax and save return address

		call	[LongWorldRotateProjectPoint3D]

		push	eax
		mov	eax, [LongWorldRotateProjectPoint3D]

		mov	[LongWorldRotateProjectPoint], eax
		pop	eax

		jmp	[temp]		; return

LongWorldRotateProjectPointInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
