/*
;----------------------------------------------------------------------------
			.386P
			.MODEL FLAT, C

;----------------------------------------------------------------------------
			.DATA

;----------------------------------------------------------------------------
			EXTRN	C	TabOffLine:DWORD
			EXTRN	C	ClipXMin:DWORD
			EXTRN	C	ClipXMax:DWORD
			EXTRN	C	ClipYMin:DWORD
			EXTRN	C	ClipYMax:DWORD

;----------------------------------------------------------------------------
			.CODE

;----------------------------------------------------------------------------
			PUBLIC	C	CopyBlockIncrust

;----------------------------------------------------------------------------
;void	CopyBlockIncrust(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd, void *dst)

;#pragma aux CopyBlockIncrust	"*"				\
;	parm caller	[eax] [ebx] [ecx] [edx] [esi]	\
;	modify		[edi]

CopyBlockIncrust	proc uses ebp,\
			xd:DWORD, yd:DWORD, dst:DWORD

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
			xor	ecx, -1

			add	edi, TabOffLine[eax*4]
			mov	ebp, TabOffLine[1*4]

			mov	ebx, ecx
			inc	edx                     ; Delta Y

			sub	esi, ebx
			sub	edi, ebx
nextline:
			mov	ecx, ebx
nextbyte:
			mov	al, [esi+ecx]

			test	al, al
			jz	skip

			mov	[edi+ecx], al
skip:			inc	ecx

			jnz	nextbyte

			add	esi, ebp
			add	edi, ebp

			dec	edx
			jnz	nextline
endcopy:
			ret

CopyBlockIncrust	endp

;----------------------------------------------------------------------------
;			The
       			End
*/

#include <SVGA/CPYBLOCI.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <stdio.h>

void CopyBlockIncrust(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd,
                      void *dst)
{
  // Initial bounds check
  if (x0 > ClipXMax || y0 > ClipYMax) return;
  
  // Clip source coordinates to screen bounds
  S32 clipDeltaX = ClipXMin - x0;
  if (clipDeltaX > 0) {
    x0 = ClipXMin;
    xd += clipDeltaX;
  }
  
  S32 clipDeltaY = ClipYMin - y0;
  if (clipDeltaY > 0) {
    y0 = ClipYMin;
    yd += clipDeltaY;
  }
  
  if (x1 > ClipXMax) x1 = ClipXMax;
  if (y1 > ClipYMax) y1 = ClipYMax;
  
  // Calculate dimensions
  S32 width = x1 - x0;
  if (width < 0) return;
  
  S32 height = y1 - y0;
  if (height < 0) return;
  
  // Check destination bounds
  if (xd < ClipXMin || xd > ClipXMax) return;
  if (yd < ClipYMin || yd > ClipYMax) return;
  
  // Clip destination if it would go out of bounds
  S32 destClipX = (xd + width) - ClipXMax;
  if (destClipX > 0) {
    width -= destClipX;
    if (width <= 0) return;
  }
  
  S32 destClipY = (yd + height) - ClipYMax;
  if (destClipY > 0) {
    height -= destClipY;
    if (height <= 0) return;
  }
  
  // Calculate source and destination pointers
  U8 *srcPtr = (U8 *)src + TabOffLine[y0] + x0;
  U8 *dstPtr = (U8 *)dst + TabOffLine[yd] + xd;
  
  U32 scanLine = TabOffLine[1];  // Screen width
  
  // Copy with transparency (skip pixels with value 0)
  for (S32 y = 0; y <= height; y++) {
    for (S32 x = 0; x <= width; x++) {
      U8 pixel = srcPtr[x];
      if (pixel != 0) {  // Skip transparent pixels
        dstPtr[x] = pixel;
      }
    }
    srcPtr += scanLine;
    dstPtr += scanLine;
  }
}
