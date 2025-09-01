#include <SVGA/CLRBOX.H>
#include <stdio.h>
#include <string.h>

// ClearColor	dq	0
U64 ClearColor = 0;

void ClearBox(void *dst, U32 *TabOffDst, T_BOX *box)
{
	/*
;void	ClearBoxF(void *dst,  U32 *TabOffDst, T_BOX *box) ;

;#pragma aux ClearBoxF	       	       	\
;	parm	[edi] [edx] [ebx]	\
;	modify	[eax ecx esi]

ClearBoxF		PROC \
			uses edi edx ebx eax ecx esi \
			dst: DWORD, TabOffDst: DWORD, box: DWORD

			mov edi, dst
			mov edx, TabOffDst
			mov ebx, box

			mov	esi, [ebx]
			mov	ebx, [ebx+4]

			mov	eax, esi
			mov	ecx, ebx

			shr	esi, 16
			and	eax, 0FFFFh

			shr	ebx, 16
			add	edi, [edx+esi*4]

			mov	edx, [edx+1*4]
			and	ecx, 0FFFFh

			sub	eax, ecx		; -deltaX
			add	edi, ecx

			sub 	ebx, esi		; deltaY
			mov	ecx, eax

			fld	[ClearColor]

			test	eax, 8
			jnz	odd
loopClear:
			fst	REAL8 ptr[edi+eax]
			fst	REAL8 ptr[edi+eax+8]
			add	eax, 16
			jnz	loopClear
			mov	eax, ecx
			add	edi, edx
			dec	ebx
			jnz	loopClear

			fsubp	st(0), st(0)
			ret
odd:
			add	eax, 8
			je	@@Just_A_Col

			sub	edi, 8

			add	ecx, 8
loopClearY:
			fst	REAL8 ptr[edi+eax]
loopClearX:
			fst	REAL8 ptr[edi+eax+8]
			fst	REAL8 ptr[edi+eax+16]
			add	eax, 16
			jnz	loopClearX
			mov	eax, ecx
			add	edi, edx
			dec	ebx
			jnz	loopClearY
ClearBoxFEnd::
			fsubp	st(0), st(0)
			ret
@@Just_A_Col:
			fst	REAL8 ptr[edi-8]
			add	edi, edx

			dec	ebx
			jnz	@@Just_A_Col

			fsubp	st(0), st(0)
			ret

ClearBoxF		ENDP

	*/
	// The original code used the FPU to clear the screen, by copying the clear
	// color (which is a double -8 bytes- value, with the actual clear color -1 byte-
	// repeated 8 times) to the screen, overriding 8 bytes at a time.
	// Since I don't know if this will be a bottleneck in modern devices, I'll
	// use the simplest and most naive implementation.

	U8 *dst8 = (U8 *)dst;
	U8 color = ClearColor & 0xFF;

	for (S16 y = box->y0; y < box->y1; y++)
	{
		U8 *start = dst8 + TabOffDst[y] + box->x0;
		memset(start, color, box->x1 - box->x0);
	}
}

void SetClearColor(U32 color)
{
	/*
;void	SetClearColor(U32 color)	;

;#pragma aux SetClearColor		\
;	parm	[eax]			\
;	modify	[ecx edx]

SetClearColor	proc

		and	eax, 0FFh	; 000C

		mov	edx, eax	; 000C

		shl	edx, 8		; 00C0

		mov	ecx, edx	; 00C0
		or	edx, eax	; 00CC

		shl	edx, 16		; CC00
		or	eax, ecx	; 00CC

		or	eax, edx	; CCCC

		mov	dword ptr[ClearColor], eax
		mov	dword ptr[ClearColor+4], eax

		ret

SetClearColor	endp
	*/
	// The clear color is only 1 byte, but it's stored in a 8 byte variable
	// as a more efficient way of clearing the screen, overriding 8 byte at a time
	memset(&ClearColor, color & 0xFF, sizeof(ClearColor));
}

