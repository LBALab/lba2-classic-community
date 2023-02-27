#include <SVGA/BOX.H>

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

void Box(S32 x0, S32 y0, S32 x1, S32 y1, S32 col)
{
	/*
;void	Box(S32 x0, S32 y0, S32 x1, S32 y1, S32 col)

;#pragma aux Box	"*"			\
;	parm	[eax] [ebx] [ecx] [edx] [esi]	\
;	modify	[edi]

Box			PROC \
			uses eax ebx ecx edx edi esi ebp\
			x0: DWORD, \
			y0: DWORD, \
			x1: DWORD, \
			y1: DWORD, \
			col: DWORD
			mov eax, x0
			mov ebx, y0
			mov ecx, x1
			mov edx, y1
			mov esi, col

;----------------------
			cmp	eax, ClipXMax
			jg	Box_End
			cmp     ecx, ClipXMin
			jl	Box_End
			cmp	ebx, ClipYMax
			jg	Box_End
			cmp	edx, ClipYMin
			jl	Box_End
TestLeft:
			cmp	eax, ClipXMin
			jge	TestRight
			mov	eax, ClipXMin
TestRight:
			cmp	ecx, ClipXMax
			jle	TestUp
			mov	ecx, ClipXMax
TestUp:
			cmp	ebx, ClipYMin
			jge	TestDown
			mov	ebx, ClipYMin
TestDown:
			cmp	edx, ClipYMax
			jle	OkNoClip
			mov	edx, ClipYMax
OkNoClip:
;----------------------
			mov	edi, TabOffLine[ebx*4]
			sub	edx, ebx		; Init Delta Y

			add	edi, Log
			sub	ecx, eax		; Init Delta X

			add	edi, eax		; EDI Offset Src
			mov	eax, esi

			shl	esi, 8
			inc	edx

			or	eax, esi
			inc	ecx

			mov	esi, eax
			mov	ebp, TabOffLine[1*4]

			shl	esi, 16
			sub	ebp, ecx		; ebp Delta Screen

			or	eax, esi
			mov	esi, ecx

			mov	ebx, ecx

			shr	esi, 2
			and	ebx, 11b
Again:
			mov	ecx, esi
			rep	stosd
			mov	ecx, ebx
			rep	stosb

			add	edi, ebp
			dec	edx
			jne	Again
Box_End:
			ret
Box			endp
	*/
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

	U32 screenWidth = TabOffLine[1];
	U32 deltaY = y1 - y0;
	U32 deltaX = x1 - x0;
	U32 deltaScreen = screenWidth - deltaX;
	U32 offset = TabOffLine[y0] + x0;

	for (int j = 0; j <= deltaY; j++)
	{
		U32 yOffset = offset + j * screenWidth;

		for (int i = 0; i <= deltaX; i++)
		{
			((U8 *)Log)[yOffset + i] = col;
		}
	}
}
