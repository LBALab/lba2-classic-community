;*══════════════════════════════════════════════════════════════════════════*
		.386p

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		extrn	C	X0			:DWORD
		extrn	C	Y0			:DWORD

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

GetAngleVector3D proc

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

		ret

GetAngleVector3D endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
