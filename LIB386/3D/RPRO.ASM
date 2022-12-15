;*══════════════════════════════════════════════════════════════════════════*
		.386p

		.model SMALL, C

		include	3D.INC

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		public	C RotateProjectPoint

;*══════════════════════════════════════════════════════════════════════════*
RotateProjectPoint	dd	offset RotateProjectPointInit

;*══════════════════════════════════════════════════════════════════════════*
temp		dd	?

		ASSUME  DS:SEG temp

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	TypeProj:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	RotateProjectPoint3D:DWORD
		extrn	C	RotateProjectPointIso:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		public	C	RotateProjectPointInit

;*══════════════════════════════════════════════════════════════════════════*
RotateProjectPointInit proc

		mov	[temp], eax	; save eax
		pop	eax		; get return address

		xchg	[temp], eax	; restore eax and save return address

		cmp	[TypeProj], TYPE_3D
		jne	iso

		call	[RotateProjectPoint3D]

		push	eax
		mov	eax, [RotateProjectPoint3D]

		jmp	save
iso:
		call	[RotateProjectPointIso]

		push	eax
		mov	eax, [RotateProjectPointIso]
save:
		mov	[RotateProjectPoint], eax
		pop	eax

		jmp	[temp]		; return

RotateProjectPointInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
