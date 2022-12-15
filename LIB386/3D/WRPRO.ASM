;*══════════════════════════════════════════════════════════════════════════*
		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		public	C WorldRotateProjectPoint

;*══════════════════════════════════════════════════════════════════════════*
WorldRotateProjectPoint	dd	offset WorldRotateProjectPointInit

;*══════════════════════════════════════════════════════════════════════════*
temp		dd	?

		ASSUME  DS:SEG temp

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	WorldRotateProjectPoint3D:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		public	C	WorldRotateProjectPointInit

;*══════════════════════════════════════════════════════════════════════════*
WorldRotateProjectPointInit proc

		mov	[temp], eax	; save eax
		pop	eax		; get return address

		xchg	[temp], eax	; restore eax and save return address

		call	[WorldRotateProjectPoint3D]

		push	eax
		mov	eax, [WorldRotateProjectPoint3D]

		mov	[WorldRotateProjectPoint], eax
		pop	eax

		jmp	[temp]		; return

WorldRotateProjectPointInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
