;*══════════════════════════════════════════════════════════════════════════*
		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

fmt_getang3d_1	DB	"[ASM][GetAngleVector3D][1] x=%d y=%d z=%d", 10, 0
fmt_getang3d_2	DB	"[ASM][GetAngleVector3D][2] return=%d X0=%d Y0=%d", 10, 0

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	X0			:DWORD
		extrn	C	Y0			:DWORD
		extrn	C	printf			:NEAR

;*══════════════════════════════════════════════════════════════════════════*
		.CODE

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	GetAngleVector2D	:PROC
		extrn	C	QSqr			:PROC

;*══════════════════════════════════════════════════════════════════════════*
		public	C	GetAngleVector3D

;*══════════════════════════════════════════════════════════════════════════*
;void	GetAngleVector3D(S32 x, S32 y, S32 z)	;

;#pragma aux GetAngleVector3D	"*"		\
;	parm		[eax] [ebx] [ecx]	\
;	modify		[edx esi edi]

GetAngleVector3D PROC \
			uses ebx ecx edx edi esi ebp \
			x: DWORD, y: DWORD, z: DWORD

			mov eax, x
			mov ebx, y
			mov ecx, z

		; --- debug trace [ASM][GetAngleVector3D][1] params ---
		pushad
		push	ecx
		push	ebx
		push	eax
		push	offset fmt_getang3d_1
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---

		push	ebx		; Y
		push	eax		; X

		mov	ebx, ecx	; Z
		mov	edi, ecx

		call	GetAngleVector2D

		xor	eax, -1
		pop	ecx		; X

		lea	ebx, [eax+2048+1]
		mov	eax, edi	; Z

		imul	eax		; Z^2

		mov	esi, eax
		mov	edi, edx

		and	ebx, 4095
		mov	eax, ecx	; X

		imul	eax		; X^2

		add	eax, esi
		mov	[ds:Y0], ebx

		adc	edx, edi
		call	QSqr

		mov	ebx, eax
		pop	eax

		call	GetAngleVector2D

		neg	eax

		and	eax, 4095

		mov	[ds:X0], eax

		; --- debug trace [ASM][GetAngleVector3D][2] return ---
		pushad
		push	dword ptr [ds:Y0]
		push	dword ptr [ds:X0]
		push	eax
		push	offset fmt_getang3d_2
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---

		ret

GetAngleVector3D endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
