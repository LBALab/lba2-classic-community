#include <SVGA/CPYBLOCK.H>

#include <memory.h>

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

void CopyBlock(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd,
               void *dst)
{
	/*
;void	CopyBlock(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd, void *dst)

;#pragma aux CopyBlock	"*"				\
;	parm caller	[eax] [ebx] [ecx] [edx] [esi]	\
;	modify		[edi]

CopyBlock		PROC \
			uses eax ebx ecx edx edi esi ebp\
			x0:DWORD, y0:DWORD, x1:DWORD, y1:DWORD, src:DWORD,  \
			xd:DWORD, yd:DWORD, dst:DWORD

			mov eax, x0
			mov ebx, y0
			mov ecx, x1
			mov edx, y1
			mov esi, src

			cmp	eax,[ClipXMax]
			jg	endcopy
			cmp	ebx,[ClipYMax]
			jg	endcopy

			mov	edi,[ClipXMin]

			sub	edi,eax
			jle	@@NoClipXMin

			mov	eax,[ClipXMin]
			add	[xd],edi
@@NoClipXMin:
			mov	edi,[ClipYMin]
			sub	edi,ebx
			jle	@@NoClipYMin

			mov	ebx,[ClipYMin]
			add	[yd],edi
@@NoClipYMin:
			mov	edi,[ClipXMax]
			cmp	ecx,edi
			jle	@@NoClipXMax
			mov	ecx,edi
@@NoClipXMax:
			mov	edi,[ClipYMax]
			cmp	edx,edi
			jle	@@NoClipYMax
			mov	edx,edi
@@NoClipYMax:
			sub	ecx, eax
			js	endcopy

			sub	edx, ebx
			js	endcopy

			mov	edi,[xd]
			cmp	edi,[ClipXMin]
			jl	endcopy
			cmp	edi,[ClipXMax]
			jg	endcopy

			mov	edi,[yd]
			cmp	edi,[ClipYMin]
			jl	endcopy
			cmp	edi,[ClipYMax]
			jg	endcopy


			mov	edi,[xd]
			add	edi,ecx
			sub	edi,[ClipXMax]
			jle	@@NoClipXMaxD
			sub	ecx,edi
			jle	endcopy
@@NoClipXMaxD:

			mov	edi,[yd]
			add	edi,edx
			sub	edi,[ClipYMax]
			jle	@@NoClipYMaxD
			sub	edx,edi
			jle	endcopy
@@NoClipYMaxD:



			add	esi, eax		; Offset Src
			mov	edi, dst

			mov	eax, yd
			add	edi, xd			; Offset Dst

			add	esi, TabOffLine[ebx*4]
			lea	ebx, [ecx+1]		; Delta X

			add	edi, TabOffLine[eax*4]
			mov	eax, TabOffLine[1*4]

			inc	edx                     ; Delta Y
			sub	eax, ebx

			cmp	ebx, 3+4+3
			jae	copydword
nextbyte:
			mov	ecx, ebx
			rep	movsb

			add	esi, eax
			add	edi, eax

			dec	edx
			jnz	nextbyte
endcopy:
			ret
copydword:
			mov	ecx, ebx
			mov	ebx, 4

			sub	ebx, edi

			and	ebx, 3

			sub	ecx, ebx

			mov	bh, cl
			push	ebp

			shr	ecx, 2
			and	bh, 3

			mov	ebp, ecx

			test	bl, bl
			jz	checkend

			test	bh, bh
			jz	juststart
startend:
			xor	ecx, ecx
			mov	cl, bl
			rep	movsb
			mov	ecx, ebp
			; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
            ;   is it confusing it with some other instruction? Is it not supported by 486?
			rep	movsd
			xor	ecx, ecx
			mov	cl, bh
			rep	movsb

			add	esi, eax
			add	edi, eax

			dec	edx
			jnz	startend

			pop	ebp
			ret
juststart:
			xor	ecx, ecx
			mov	cl, bl
			rep	movsb
			mov	ecx, ebp
			; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
			;   is it confusing it with some other instruction? Is it not supported by 486?
			rep	movsd

			add	esi, eax
			add	edi, eax

			dec	edx
			jnz	juststart

			pop	ebp
			ret
checkend:
			test	bh, bh
			jz	noend
justend:
			mov	ecx, ebp
			; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
            ;   is it confusing it with some other instruction? Is it not supported by 486?
			rep	movsd
			xor	ecx, ecx
			mov	cl, bh
			rep	movsb

			add	esi, eax
			add	edi, eax

			dec	edx
			jnz	justend

			pop	ebp
			ret
noend:
			mov	ecx, ebp
			; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
            ;   is it confusing it with some other instruction? Is it not supported by 486?
			rep	movsd

			add	esi, eax
			add	edi, eax

			dec	edx
			jnz	noend

			pop	ebp
			ret

CopyBlock		endp
	*/
  if (x0 > ClipXMax || y0 > ClipYMax)
	{
    return;
	}

  if (ClipXMin > x0)
  {
    x0 = ClipXMin;
    xd += ClipXMin - x0;
  }

  if (ClipYMin > y0)
  {
    y0 = ClipYMin;
    yd += ClipYMin - y0;
  }

  if (x1 > ClipXMax)
  {
    x1 = ClipXMax;
  }

  if (y1 > ClipYMax)
  {
    y1 = ClipYMax;
  }

  U32 width = x1 - x0;
  if (x0 > x1)
  {
    return;
  }
  U32 height = y1 - y0;
  if (y0 > y1)
  {
    return;
  }
  if (xd < ClipXMin)
  {
    return;
  }
  if (xd > ClipXMax)
  {
    return;
  }
  if (yd < ClipYMin)
  {
    return;
  }
  if (yd > ClipYMax)
  {
    return;
  }
  S32 distanceToXMax = width + xd - ClipXMax;
  if (width + xd > ClipXMax)
  {
    if (width <= distanceToXMax)
    {
      return;
    }
    width -= distanceToXMax;
  }
  S32 distanceToYMax = height + yd - ClipYMax;
  if (height + yd > ClipYMax)
  {
    if (height <= distanceToYMax)
    {
      return;
    }
    height -= distanceToYMax;
  }
  // Offset Src
  U8 *srcOffset = (U8 *)(TabOffLine[y0] + x0 + (U8 *)src);
  // Delta X
  U32 deltaX = width + 1;
  // Offset Dst
  U8 *dstOffset = (U8 *)(TabOffLine[yd] + xd + (U8 *)dst);
  // Delta Y
  U32 deltaY = height + 1;
  U32 screenOffset = TabOffLine[1] - (width + 1);

  do
  {
    memcpy(dstOffset, srcOffset, deltaX);
    srcOffset += screenOffset + deltaX;
    dstOffset += screenOffset + deltaX;
    --deltaY;
  }
  while (deltaY);
}
