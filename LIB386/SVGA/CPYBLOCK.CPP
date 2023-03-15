#include <SVGA/CPYBLOCK.H>

#include <memory.h>

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

void CopyBlock(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd,
               void *dst)
// int __cdecl __spoils<ecx> CopyBlock(int x0, int y0, int x1, int y1, int src, int xd, int yd, int dst)
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
  int v8; // eax@1
  int v9; // ebx@1
  int v10; // ecx@1
  int v11; // edx@1
  int v12; // ecx@11
  int v13; // edx@12
  int v14; // edi@17
  unsigned __int8 v15; // of@18
  int v16; // edi@19
  char *v17; // esi@21
  unsigned int v18; // ebx@21
  char *v19; // edi@21
  int v20; // edx@21
  int v21; // eax@21
  int result; // eax@23
  unsigned __int8 v23; // bl@24
  unsigned __int8 v24; // bh@24
  char *v25; // edi@26
  char *v26; // esi@26
  char *v27; // edi@26
  char *v28; // esi@26
  char *v29; // edi@28
  char *v30; // esi@28
  char *v31; // edi@31
  char *v32; // esi@31
  int v33; // [sp+4h] [bp-4h]@0
  int savedregs; // [sp+8h] [bp+0h]@26

  v8 = x0;
  v9 = y0;
  v10 = x1;
  v11 = y1;
  if ( x0 > ClipXMax || y0 > ClipYMax )
	{
    return;
	}

  if ( ClipXMin > x0 )
  {
    v8 = ClipXMin;
    xd += ClipXMin - x0;
  }
  if ( ClipYMin > y0 )
  {
    v9 = ClipYMin;
    yd += ClipYMin - y0;
  }
  if ( x1 > ClipXMax )
    v10 = ClipXMax;
  if ( y1 > ClipYMax )
    v11 = ClipYMax;
  v12 = v10 - v8;
  if ( v12 < 0 )
    return;
  v13 = v11 - v9;
  if ( v13 < 0 )
    return;
  if ( xd < ClipXMin )
    return;
  if ( xd > ClipXMax )
    return;
  if ( yd < ClipYMin )
    return;
  if ( yd > ClipYMax )
    return;
  v14 = v12 + xd - ClipXMax;
  if ( v12 + xd > ClipXMax )
  {
    v15 = v12 <= v14;
    v12 -= v14;
    if (v15)
      return;
  }
  v16 = v13 + yd - ClipYMax;
  if ( v13 + yd > ClipYMax )
  {
    v15 = v13 <= v16;
    v13 -= v16;
    if (v15)
      return;
  }
  v17 = (char *)(TabOffLine[v9] + v8 + (U8 *)src);
  v18 = v12 + 1;
  v19 = (char *)(TabOffLine[yd] + xd + (U8 *)dst);
  v20 = v13 + 1;
  v21 = TabOffLine[1] - (v12 + 1);
  if ( (unsigned int)(v12 + 1) < 0xA )
  {
    do
    {
      memcpy(v19, v17, v18);
      v17 += v21 + v18;
      v19 += v21 + v18;
      --v20;
    }
    while ( v20 );
    return;
  }
  v23 = (4 - v19) & 3;
  v24 = (v12 + 1 - v23) & 3;
  if ( v23 )
  {
    if ( v24 )
    {
      do
      {
        memcpy(v19, v17, v23);
        v26 = &v17[v23];
        v25 = &v19[v23];
        memcpy(v25, v26, 4 * (_DWORD)&savedregs);
        v28 = &v26[4 * (_DWORD)&savedregs];
        v27 = &v25[4 * (_DWORD)&savedregs];
        memcpy(v27, v28, v24);
        v17 = &v28[v24] + v21;
        v19 = &v27[v24] + v21;
        --v20;
      }
      while ( v20 );
      result = v33;
    }
    else
    {
      do
      {
        memcpy(v19, v17, v23);
        v30 = &v17[v23];
        v29 = &v19[v23];
        memcpy(v29, v30, 4 * (_DWORD)&savedregs);
        v17 = &v30[4 * (_DWORD)&savedregs] + v21;
        v19 = &v29[4 * (_DWORD)&savedregs] + v21;
        --v20;
      }
      while ( v20 );
      result = v33;
    }
  }
  else if ( v24 )
  {
    do
    {
      memcpy(v19, v17, 4 * (_DWORD)&savedregs);
      v32 = &v17[4 * (_DWORD)&savedregs];
      v31 = &v19[4 * (_DWORD)&savedregs];
      memcpy(v31, v32, v24);
      v17 = &v32[v24] + v21;
      v19 = &v31[v24] + v21;
      --v20;
    }
    while ( v20 );
    result = v33;
  }
  else
  {
    do
    {
      memcpy(v19, v17, 4 * (_DWORD)&savedregs);
      v17 += 4 * (_DWORD)&savedregs + v21;
      v19 += 4 * (_DWORD)&savedregs + v21;
      --v20;
    }
    while ( v20 );
    result = v33;
  }
}
