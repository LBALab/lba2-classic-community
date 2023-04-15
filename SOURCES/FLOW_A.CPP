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

void ShadeBoxBlk(S32 x0, S32 y0, S32 x1, S32 y1, S32 deccoul)
{
  if (x0 > ClipXMax || x1 < ClipXMin || y0 > ClipYMax || y1 < ClipYMin)
  {
    return;
  }

  if (x0 < ClipXMin)
  {
    x0 = ClipXMin;
  }
  if (x1 > ClipXMax)
  {
    x1 = ClipXMax;
  }
  if (y0 < ClipYMin)
  {
    y0 = ClipYMin;
  }
  if (y1 > ClipYMax)
  {
    y1 = ClipYMax;
  }

  S32 deltaY = y1 - y0 + 1;
  S32 deltaX = x1 - x0 + 1;

  U8 *dst = (U8 *)Log + TabOffLine[y0] + x0;
  U32 screenWidth = TabOffLine[1];

  U32 clutOffset = 15 * 256;
  U8 clutOffsetHighByte = (clutOffset >> 8) & 0xFF;
  clutOffsetHighByte -= (U8)(deccoul & 0xFF);
  clutOffset = (clutOffset & 0xFFFF00FF) | (clutOffsetHighByte << 8);
  U8 *clut = PtrCLUTGouraud + clutOffset;

  for (S32 y = 0; y < deltaY; y++)
  {
    for (S32 x = 0; x < deltaX; x++)
    {
      dst[x] = clut[dst[x]];
    }
    dst += screenWidth;
  }
}

void	CopyBlockShade( S32 x0, S32 y0, S32 x1, S32 y1, U8 *src,
				S32 xd, S32 yd, U8 *dst, S32 shade )
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}
