;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C


;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*

		public	C	Distance2D_ASM
		public	C	Distance3D_ASM

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	QSqr:PROC

;*══════════════════════════════════════════════════════════════════════════*
;U32	Distance2D_ASM(S32 x0, S32 y0, S32 x1, S32 y1)	;

;#pragma aux Distance2D	"*"			\
;	parm		[eax] [ecx] [ebx] [edx]	\
;	modify		[esi edi]

Distance2D_ASM		PROC \
			uses ebx ecx edx edi esi ebp\
			x0: DWORD, y0: DWORD, x1: DWORD, y1: DWORD
			mov eax, x0
			mov ecx, y0
			mov ebx, x1
			mov edx, y1

		sub	eax, ebx		; Dx = x0-x1
		sub	ecx, edx		; Dy = y0-y1
		imul	eax			; Dx²
		mov	esi, eax
		mov	eax, ecx
		mov	edi, edx
		imul	eax			; Dy²
		add	eax, esi		; Dx² + Dy²
		adc	edx, edi		; ...

		push edx
		push eax

		call	QSqr			; eax = √ (Dx²+Dy²)

		add esp, 4*2

		ret

Distance2D_ASM	endp

;*══════════════════════════════════════════════════════════════════════════*
;U32	Distance3D(S32 x0, S32 y0, S32 z0, S32 x1, S32 y1, S32 z1)	;

;#pragma aux Distance3D	"*"					\
;	parm		[eax] [ecx] [ebx] [edx]	[esi] [edi]	\
;       modify		[esi edi]

Distance3D_ASM	PROC \
			uses ebx ecx edx edi esi ebp\
			x0: DWORD, y0: DWORD, z0: DWORD, x1: DWORD, y1: DWORD, z1
			mov eax, x0
			mov ecx, y0
			mov ebx, z0
			mov edx, x1
			mov esi, y1
			mov edi, z1

		sub	eax, edx
		sub	ecx, esi
		imul	eax			; Dx²
		sub	ebx, edi
		mov	esi, eax
		mov	eax, ecx
		mov	edi, edx
		imul	eax			; Dy²
		add	esi, eax
		mov	eax, ebx
		adc	edi, edx
		imul	eax			; Dz²
		add	eax, esi
		adc	edx, edi

		push edx
		push eax

		call	QSqr			; eax = √ (Dx²+Dy²+Dz²)

		add esp, 4*2

		ret

Distance3D_ASM	endp

;*══════════════════════════════════════════════════════════════════════════*
		;The
		 End

