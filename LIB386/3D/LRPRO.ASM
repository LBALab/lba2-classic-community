;*══════════════════════════════════════════════════════════════════════════*
		.386p

		.model SMALL, C

		include	3D.INC

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		public	C LongRotateProjectPoint

;*══════════════════════════════════════════════════════════════════════════*
LongRotateProjectPoint	dd	offset LongRotateProjectPointInit

;*══════════════════════════════════════════════════════════════════════════*
temp		dd	?

			ASSUME  DS:SEG temp

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	TypeProj:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	LongRotateProjectPoint3D:DWORD
		extrn	C	LongRotateProjectPointIso:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		public	C	LongRotateProjectPointInit

;*══════════════════════════════════════════════════════════════════════════*
LongRotateProjectPointInit proc

		mov	[temp], eax	; save eax
		pop	eax		; get return address

		xchg	[temp], eax	; restore eax and save return address

		cmp	[TypeProj], TYPE_3D
		jne	iso

		call	[LongRotateProjectPoint3D]

		push	eax
		mov	eax, [LongRotateProjectPoint3D]

		jmp	save
iso:
		call	[LongRotateProjectPointIso]

		push	eax
		mov	eax, [LongRotateProjectPointIso]
save:
		mov	[LongRotateProjectPoint], eax
		pop	eax

		jmp	[temp]		; return

LongRotateProjectPointInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
