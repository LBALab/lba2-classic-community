;*══════════════════════════════════════════════════════════════════════════*
		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		public	C LongProjectList
LongProjectList	dd	offset LongProjectListInit

ASSUME ds:SEG temp

;*══════════════════════════════════════════════════════════════════════════*
temp		dd	?

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	LongProjectList3D:DWORD

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		public	C	LongProjectListInit

;*══════════════════════════════════════════════════════════════════════════*
LongProjectListInit proc

		mov	[ds:temp], eax	; save eax
		pop	eax		; get return address

		xchg	[ds:temp], eax	; restore eax and save return address

		call	[ds:LongProjectList3D]

		push	eax
		mov	eax, [ds:LongProjectList3D]

		mov	[ds:LongProjectList], eax
		pop	eax

		jmp	[ds:temp]		; return

LongProjectListInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
