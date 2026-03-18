;----------------------------------------------------------------------------
			.386

			.MODEL	FLAT, C

;----------------------------------------------------------------------------
			.DATA

			public	C	BlitBox
			BlitBox	dd	offset BlitBoxF

;----------------------------------------------------------------------------
			extrn	C TabOffPhysLine:DWORD

;----------------------------------------------------------------------------

		EXTRN	C	printf	:NEAR

fmt_blitboxf_1	DB	"[ASM][BlitBoxF][1] dst=%p src=%p", 10, 0

			.CODE

;----------------------------------------------------------------------------
			public	C	BlitBoxF

;----------------------------------------------------------------------------
;void	BlitBoxF(void *dst, void *src) ;

;#pragma aux BlitBoxF		\
;	parm	[edi] [esi]	\
;	modify	[eax ebx ecx edx]

BlitBoxF		PROC \
			uses eax ebx ecx edx edi esi \
			dst: DWORD, src: DWORD


		; --- debug trace [ASM][BlitBoxF][1] params ---
		pushad
		push	esi
		push	edi
		push	offset fmt_blitboxf_1
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---

					mov edi, dst
			mov esi, src

			mov	edx, offset TabOffPhysLine
			mov	ecx, 200

			mov	eax, [edx+140*4]
			mov	ebx, -320

			mov	edx, [edx+1*4]
			lea	edi, [edi+eax+160+320]
loopCopy:
			fld	REAL8 ptr [esi]
			fstp	REAL8 ptr [edi+ebx]
			fld	REAL8 ptr [esi+8]
			add	esi, 16
			fstp	REAL8 ptr [edi+ebx+8]
			add	ebx, 16
			jnz	loopCopy
			mov	ebx, -320
			add	edi, edx
			dec	ecx
			jnz	loopCopy

			ret

BlitBoxF		ENDP

;----------------------------------------------------------------------------
;			The
			End
