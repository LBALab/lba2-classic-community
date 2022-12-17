;----------------------------------------------------------------------------
			.386

			.MODEL	FLAT, C

;----------------------------------------------------------------------------
			.DATA

;----------------------------------------------------------------------------
			extrn	C	ClearColor:DWORD

;----------------------------------------------------------------------------
			.CODE

;----------------------------------------------------------------------------
			public	C	ClearBoxVESAI

                        public  C ClearBoxVESAI1
                        public  C ClearBoxVESAI2
                        public  C ClearBoxVESAI3
                        public  C ClearBoxVESAI4
                        public  C ClearBoxVESAI5
                        public  C ClearBoxVESAI6

;----------------------------------------------------------------------------
			Extrn	C	VirtualLinearFirst:PROC

;----------------------------------------------------------------------------
IFDEF	_WIN32
			extrn  C	AllowPatchAndPatch:PROC
ENDIF

;----------------------------------------------------------------------------
;void	ClearBoxVESAI(void *dst, U32 *TabOffDst, T_BOX *box) ;

;#pragma aux ClearBoxVESAI		\
;	parm	[edi] [edx] [ebx]	\
;	modify	[eax ecx esi]

ClearBoxVESAI		PROC
			ASSUME	FS:_DATA

			push	ebp
			mov	ebp, [ebx]		; ebp = eax = (XMin, YMin)

			mov	ebx, [ebx+4]		; ebx = (XMax, YMax)
			mov	eax, ebp

			shr	ebp, 16			; EBP = YMin
			and	eax, 0FFFFh		; EAX = XMin

			mov	ecx, ebx

			add	edi, eax

			add	edi, [edx+ebp*4]
			mov	edx, [edx+4]		; EDX = Pitch Dst

			shr	ebx, 16
			and	ecx, 0FFFFh

			sub	ecx, eax		; ECX = deltaX
			sub 	ebx, ebp		; EBX = deltaY

			xor	eax, eax

			call	VirtualLinearFirst

			mov	eax, [ClearColor]

			cmp	ecx, 4
			jz	Equal_4

			sub	edx, ecx

			shr	ecx, 3
			jc	odd

			mov	ebp, ecx
loopCopy:
ClearBoxVESAI1::	mov	fs:[edi], eax
ClearBoxVESAI2::	mov	fs:[edi+4], eax
			add	edi, 8
			dec	ecx
			jnz	loopCopy
			add	edi, edx
			dec	ebx
			mov	ecx, ebp
			jnz	loopCopy

			pop	ebp
			ret
odd:
			add	edx, 4
			mov	ebp, ecx
loopCopyY:
ClearBoxVESAI3::	mov	fs:[edi], eax
loopCopyX:
ClearBoxVESAI4::	mov	fs:[edi+4], eax
ClearBoxVESAI5::	mov	fs:[edi+8], eax
			add	edi, 8
			dec	ecx
			jnz	loopCopyX
			add	edi, edx
			dec	ebx
			mov	ecx, ebp
			jnz	loopCopyY
ClearBoxVESAIEnd::
			pop	ebp
			ret
Equal_4:
ClearBoxVESAI6::	mov	fs:[edi], eax
			add	edi, edx

			dec	ebx
			jnz	Equal_4

			pop	ebp
			ret

ClearBoxVESAI		ENDP

;----------------------------------------------------------------------------
;			The
			End
