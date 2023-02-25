/*
				OPTION SCOPED
				.386P
				.model FLAT, C

				.data
;*══════════════════════════════════════════════════════════════════════════*

			extrn  C		PtrCLUTGouraud:DWORD

			extrn  C		ClipXMin:DWORD
			extrn  C		ClipYMin:DWORD
			extrn  C		ClipXMax:DWORD
			extrn  C		ClipYMax:DWORD

			extrn  C		TabOffLine:DWORD
			extrn  C		Log:DWORD



				.code
			public	C      	BoxFlow
			public	C      	ShadeBoxBlk
			public	C      	CopyBlockShade
;Seg_Code		SEGMENT	PUBLIC PARA USE32

;*══════════════════════════════════════════════════════════════════════════*
;----------------------------------------------------------------------------

; SEBER

;// ATTENTION: passage de parametres par registres le 04/04/96
;
;			BoxFlow( x,  y, col )
;                               eax ebx ecx
;
;----------------------------------------------------------------------------
;
BoxFlow			proc

			cmp	eax, ds:ClipXMax
			jge	Box_End
			cmp     eax, ClipXMin
			jl	Box_End
			cmp	ebx, ClipYMax
			jge	Box_End
			cmp	ebx, ClipYMin
			jl	Box_End
;----------------------
			mov	edx, TabOffLine[ebx*4]
			add	edx, Log
			add	edx, eax		; EDI Offset Src
;----------------------

			mov	ch, cl
			mov	[edx], cx
			mov	[edx+640], cx
Box_End:
			ret
BoxFlow			endp

;***************************************************************************
;			CopyBlockShade( x0, y0, x1, y1, *src, xd, yd, *dst, shade )
;                                      edx ecx ebx eax   esi
;
CopyBlockShade		proc	uses edi ebp,\
			xd:DWORD, yd:DWORD, dest:DWORD, shade:DWORD

;----------------------

			add	esi, TabOffLine[ecx*4]
			add	esi, edx		; SI Offset Src

			mov	edi, yd
			mov	edi, TabOffLine[edi*4]
			add	edi, dest
			add	edi, xd			; DI Offset Dst

			sub	ebx, edx
			js	@@error
			inc	ebx 			; BX Delta X
			cmp	ebx, 640
			ja	@@error

			sub	eax, ecx
			js	@@error
			inc	eax                     ; AX Delta Y
			cmp	eax, 480
			ja	@@error

			mov	edx, TabOffLine[1*4]	; ScanLine
			inc	eax
			sub	edx, ebx		; DX Delta Screen

			push	ebx
			mov	bl, byte ptr [shade]

			mov	ebp, edx		; deltax
			mov	edx, eax		; nbline

next_line:
			mov	ecx, [esp]

next_one:
			mov	al, byte ptr[esi]
			mov	ah, al
			and	eax, 0F00Fh
			sub	al, bl
			js	lesszero

			add	al, ah
			mov	byte ptr[edi], al
			inc	edi
			inc	esi

			dec	ecx
			jnz	next_one

			add	esi, ebp
			add	edi, ebp

			dec	edx
			jnz	next_line
			pop	ebx
			ret

lesszero:
			mov	byte ptr[edi], ah
			inc	edi
			inc	esi
			dec	ecx
			jnz	next_one

			add	esi, ebp
			add	edi, ebp

			dec	edx
			jnz	next_line
			pop	ebx

@@error:
			ret

CopyBlockShade		endp


;*══════════════════════════════════════════════════════════════════════════*

;*══════════════════════════════════════════════════════════════════════════*
;Seg_Code		ENDS
;*══════════════════════════════════════════════════════════════════════════*
;				The
				End
*/

#include "FLOW.H"

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <POLYGON/POLY.H>
#include <stdio.h>

void	BoxFlow( S32 x, S32 y, S32 coul )
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}

void ShadeBoxBlk(S32 X0, S32 Y0, S32 X1, S32 Y1, S32 deccoul)
{
	/*
ShadeBoxBlk		PROC	USES esi edi ebp ebx,\
			x0:DWORD, y0:DWORD, x1:DWORD, y1:DWORD,\
			deccoul:DWORD

			mov	eax,[x0]
			mov	ebx,[y0]
			mov	ecx,[x1]
			mov	edx,[y1]

			cmp	eax,[ClipXMax]
			jg	Box_End
			cmp     ecx,[ClipXMin]
			jl	Box_End
			cmp	ebx,[ClipYMax]
			jg	Box_End
			cmp	edx,[ClipYMin]
			jl	Box_End


			cmp	eax,[ClipXMin]
			jl	Bad_Left
@@Test_Right:
			cmp	ecx,[ClipXMax]
			jg	Bad_Right
@@Test_Up:
			cmp	ebx,[ClipYMin]
			jl	Bad_Up
@@Test_Down:
			cmp	edx,[ClipYMax]
			jg	Bad_Down

@@Disp:
			mov	edi,TabOffLine[ebx*4]
			sub	edx,ebx			; Init Delta Y
			add	edi,[Log]
			mov	ebx,edx
			add	edi,eax			; EDI Offset Src
			inc	ebx


			mov	edx,ecx			; Init Delta X
			mov	esi,TabOffLine[1*4]	; ScanLine
			sub	edx,eax
			mov	ecx,15*256
			inc	edx
			sub	ch,byte ptr [deccoul]
			sub	esi,edx			; ebp Delta Screen
			add	ecx,[PtrCLUTGouraud]
			xor	eax,eax

Again:
			mov	al,[edi]
			mov	ebp,edx
loopx:
			mov	al,[ecx+eax]
			mov	[edi],al
			inc	edi

			dec	ebp
			mov	al,[edi]
			jne	loopx

			add	edi,esi
			dec	ebx
			jne	Again
Box_End:
			ret

Bad_Left:
			mov	eax,[ClipXMin]
			jmp	@@Test_Right
Bad_Right:
			mov	ecx,[ClipXMax]
			jmp	@@Test_Up
Bad_Up:
			mov	ebx,[ClipYMin]
			jmp	@@Test_Down
Bad_Down:
			mov	edx,[ClipYMax]
			jmp	@@Disp
ShadeBoxBlk		ENDP
	*/
	if (X0 > ClipXMax || X1 < ClipXMin || Y0 > ClipYMax || Y1 < ClipYMin)
	{
		return;
	}

	if (X0 < ClipXMin)
	{
		X0 = ClipXMin;
	}
	if (X1 > ClipXMax)
	{
		X1 = ClipXMax;
	}
	if (Y0 < ClipYMin)
	{
		Y0 = ClipYMin;
	}
	if (Y1 > ClipYMax)
	{
		Y1 = ClipYMax;
	}

	S32 deltaY = Y1 - Y0;
	S32 deltaX = X1 - X0;

	U8 *dst = (U8 *)Log + TabOffLine[Y0] + X0;
	U32 lineOffset = TabOffLine[1] - deltaX - 1;

	U8 *clut = PtrCLUTGouraud + 15 * 256 - deccoul;

	for (S32 y = 0; y <= deltaY; y++)
	{
		for (S32 x = 0; x <= deltaX; x++)
		{
			dst[x] = clut[dst[x]];
		}
		dst += lineOffset;
	}
}

void	CopyBlockShade( S32 x0, S32 y0, S32 x1, S32 y1, U8 *src,
				S32 xd, S32 yd, U8 *dst, S32 shade )
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}
