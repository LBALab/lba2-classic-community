;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include	3D.INC

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

		public	C ProjectList
ProjectList	dd	offset ProjectListInit

;*══════════════════════════════════════════════════════════════════════════*

temp		dd	?

		ASSUME  DS:SEG temp

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	TypeProj:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	ProjectList3D:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	ProjectListIso:PROC

;*══════════════════════════════════════════════════════════════════════════*

		public	C	ProjectListInit

;*══════════════════════════════════════════════════════════════════════════*
ProjectListInit proc

		mov	[temp], eax	; save eax
		pop	eax		; get return address

		xchg	[temp], eax	; restore eax and save return address

		call	[ProjectList3D]

		push	eax
		mov	eax, [ProjectList3D]

		mov	[ProjectList], eax
		pop	eax

		jmp	[temp]		; return

ProjectListInit endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
