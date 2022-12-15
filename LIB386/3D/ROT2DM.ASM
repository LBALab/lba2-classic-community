;*══════════════════════════════════════════════════════════════════════════*

		.586p
		.MMX

		.model SMALL, C

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

		extrn	C	SinTab:DWORD
		extrn	C	CosTab:DWORD

		extrn C	X0:DWORD
		extrn C	Z0:DWORD

		extrn C	MMX_DEMI:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

		public C	RotateM

;*══════════════════════════════════════════════════════════════════════════*
;void	RotateM(S32 x, S32 z, S32 angle)	;

;#pragma	aux	RotateM	"*"		\
;	parm		[eax] [ecx] [edx]	\
;	modify		[ebx esi edi]

RotateM		proc

		and		edx, 4095
		jz		norot

		add		edx, edx
		pxor		mm2, mm2	; mm2 = 0  0  0  0

		movd		mm7, eax	; mm7 = 0  0  .  X

		movd		mm6, ecx	; mm6 = 0  0  .  Z

		movd		mm0, SinTab[edx]; mm0 = 0  0  .  S	Penalty of 3 cycles with 50% chances
		punpcklwd	mm7, mm6	; mm7 = .  .  Z  X

		movd		mm1, CosTab[edx]; mm1 = 0  0  .  C	If penalty above then another 3 cyles penalty
		punpckldq	mm7, mm7	; mm7 = Z  X  Z  X

		psubw		mm2, mm0	; mm2 = 0  0  . -S

		punpcklwd	mm2, mm1	; mm2 = .  .  C -S

		punpcklwd	mm1, mm0	; mm1 = .  .  S  C

		punpckldq	mm2, mm1	; mm2 = S  C  C -S

		pmaddwd		mm7, mm2	; mm7 =  X'    Z'	(<<14)

		paddd		mm7, [MMX_DEMI]

		psrad		mm7, 14		; mm7 =  X'    Z'

		movd		[Z0], mm7
		psrlq		mm7, 32		; mm7 =  0     X'

		movd		[X0], mm7

		ret
norot:
		mov		[X0], eax
		mov		[Z0], ecx
  		ret

RotateM		endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
