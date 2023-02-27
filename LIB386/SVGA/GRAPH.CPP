#include <SVGA/GRAPH.H>

#include <SVGA/SCREENXY.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
S32 GetDxDyGraph(S32 numgraph, S32 *x, S32 *y, const void *bankgraph) {
  const S32 *graphMem = (const S32 *)bankgraph;
  const U8 *graphBytes = (const U8 *)bankgraph;

  // Don't know if the value is really a signed int, but works for now
  S32 offset = graphMem[numgraph];

  *x = (graphBytes[offset] & 0xFF);
  *y = (graphBytes[offset + 1] & 0xFF);

  return 0; // TODO: Remove, make void
}

#include <stdio.h>

S32 AffGraph(S32 numgraph, S32 x, S32 y, const void *bankgraph)
{
  /*
;S32	AffGraph(S32 numgraph, S32 x, S32 y, void *bankgraph)	;

;#pragma aux AffGraph	"*"		\
;	parm	[eax] [ebx] [ecx] [esi]	\
;	modify	[edx edi]

AffGraph		PROC \
			uses eax ebx ecx edx edi esi ebp\
			numgraph: DWORD, x: DWORD, y: DWORD, bankgraph: DWORD
			mov eax, numgraph
			mov ebx, x
			mov ecx, y
			mov esi, bankgraph

			add	esi, [esi+eax*4]	; ESI = Begin Data

			movsx	eax, byte ptr[esi+2]	; Hot X
			movsx	edx, byte ptr[esi+3]	; Hot Y

			add	ebx, eax
			add	ecx, edx

			xor	edx, edx
			xor	eax, eax

			mov	dl, [esi]		; Delta X
			mov	al, [esi+1]		; Nb Line ( Delta Y )

			add	esi, 4			; Jump Hot X & Hot Y

;----------------------------------------------- Test Clipping

			add	edx, ebx
			add	eax, ecx
			dec	edx
			dec	eax

			mov	[ScreenXMin], ebx
			mov	[ScreenYMin], ecx
			mov	[ScreenXMax], edx
			mov	[ScreenYMax], eax

			cmp	ebx, ClipXMin
			jl	ClippingGraph
			cmp	ecx, ClipYMin
			jl	ClippingGraph
			cmp	edx, ClipXMax
			jg	ClippingGraph
			cmp	eax, ClipYMax
			jg	ClippingGraph

			inc	edx
			inc	eax
			sub	edx, ebx
			sub 	eax, ecx


;----------------------------------------------- Calcul Offset Ecran

			mov	edi, Log
			add	edi, TabOffLine[ecx*4]
			add	edi, ebx
			mov	bh, al		; BH = NbLine
			sub	edx, TabOffLine[1*4]; EDX = Offset Screen
			neg	edx		; EDX = ScreenWidth-edx
			xor	ecx, ecx	; Maz Compteur

;----------------------------------------------- Init NbBlock for this line

NextLine:		mov	bl, [esi]	; BL = Nb Block for this line
			inc	esi

;----------------------------------------------- Manage One Line

SameLine:		mov	al, [esi]	; OpCode
			mov	cl, al          ; Sauve AL
			and	cl, 00111111b	; AH = Bit 0-5
			test	al, 11000000b	; AL = Bit 6-7
			je	JumpZero	; Incrust
			inc	cl		; One More Please...
			test	al, 01000000b	; AL = 01b
			jne	WriteDiffPix	; Pix Differents

;----------------------------------------------- Repeat Same Color

			mov	al, [esi+1]	; Color to Repeat
			add	esi, 2
			shr	ecx, 1
			mov	ah, al
			rep	stosw		; Fill
			jnc	nobytec
			mov	[edi], al
			inc	edi
nobytec:		dec	bl		; Nb Block--
			jne	SameLine	; Continue Same Line
			add	edi, edx	; EDI += Offset Screen
			dec	bh		; NbLine--
			jne	NextLine	; Next Line
			ret

;----------------------------------------------- Copy Diff Pix

WriteDiffPix:		inc	esi
			shr	ecx, 1
			rep	movsw		; Copy Pix
			jnc	nobyted
			mov	al, [esi]
			mov	[edi], al
			inc	esi
			inc	edi
nobyted:		dec	bl		; Nb Block--
			jne	SameLine	; Continue Same Line
			add	edi, edx	; EDI += Offset Screen
			dec	bh		; NbLine--
			jne	NextLine	; Next Line
			ret

;----------------------------------------------- Incrust

JumpZero:		inc	esi
			lea	edi, [edi+ecx+1]; Jump Zero (incrust)
			dec	bl		; Nb Block--
			jne	SameLine	; Continue Same Line
			add	edi, edx	; EDI += Offset Screen
			dec	bh		; NbLine--
			jne	NextLine	; Next Line
			ret

;-------------------------------------------------------------------- Clipping
;		Graph : ( EBX, ECX ) ( EDX, EAX )
ClippingGraph:
			cmp	ebx, ClipXMax
			jg      EndGraph
			cmp	ecx, ClipYMax
			jg	EndGraph
			cmp	edx, ClipXMin
			jl	EndGraph
			cmp	eax, ClipYMin
			jl	EndGraph

			cmp	ecx, ClipYMin
			jge	PasHaut

;---------------------- Clipping Haut, Saute ClipYMin-ECX Line(s)

			mov	ebp, eax
			mov	edi, ebx

			mov	ebx, ClipYMin
			sub	ebx, ecx

NextH:			mov	bh, [esi]
			inc	esi
SameH:			mov	al, [esi]
			inc	esi
			test	al, 11000000b
			je	JumpzH
			xor	ecx, ecx
			test	al, 01000000b
			je	AquiH
			and	eax, 00111111b
			mov	ecx, eax
AquiH:			lea	esi, [esi+ecx+1]
JumpzH:			dec	bh
			jne	SameH
			dec	bl
			jne	NextH

			mov	ecx, ClipYMin		; New Y Haut

			mov	eax, ebp
			mov	ebx, edi

;---------------------- Clipping Bas
PasHaut:		cmp	eax, ClipYMax
			jle	PasBas

			mov	eax, ClipYMax

;---------------------- Clipping Gauche
PasBas:                 xor	edi, edi

			cmp     ebx, ClipXMin
			jge	PasGauche

			mov	edi, ClipXMin

			sub	edi, ebx
PasGauche:		mov	OffsetBegin, edi
;----------------------	Clipping Droit
			lea	edi, [edi+ebx-1]
			sub	edi, edx
			neg	edi
			mov	NbPix, edi

			sub	edx, ClipXMax
			jle	PasDroit
			sub	NbPix, edx
;----------------------
PasDroit:
			mov	edi, Log
			add	edi, TabOffLine[ecx*4]
			add	edi, ebx


			sub	eax, ecx
			inc	al
			mov	bh, al			; BH NbLine
			xor	ecx, ecx
			mov	edx, edi
;----------------------------------------------
							; ESI = DATAS LINE
NextL:			lea	edi, BufferClip		; EDI = BUFFERCLIP
			mov	bl, [esi]
			inc	esi
SameL:			mov	al, [esi]
			inc	esi
			mov	cl, al
			and	cl, 00111111b
			inc	cl
			test	al, 11000000b
			je	JumpL
			test	al, 01000000b
			jne	WriteDiffL
;----------------------
			mov	al, [esi]
			inc	esi
			shr	ecx, 1
			mov	ah, al
			rep	stosw
			jnc	nobyte
			mov	[edi], al
			inc	edi
			dec	bl
			jne	SameL
			jmp	NextLine2
;----------------------
JumpL:			xor	ax, ax
			shr	ecx, 1
			rep	stosw
			jnc	nobyte
			mov	[edi], al
			inc	edi
			dec	bl
			jne	SameL
			jmp	NextLine2
;----------------------
WriteDiffL:		shr	ecx, 1
			rep	movsw
			jnc	nobyte
			mov	al, [esi]
			mov	[edi], al
			inc	esi
			inc	edi
nobyte:			dec	bl
			jne	SameL
;----------------------
NextLine2:		mov	ebp, esi		; SAVE ESI
			mov	ecx, OffsetBegin
			lea	esi, [BufferClip+ecx]	; ESI = BUFFERCLIP
			lea	edi, [edx+ecx]		; EDI = SCREEN
			mov	ecx, NbPix
Again:			mov	al, [esi]
			inc	esi
			or	al, al
			je	Incrust
			mov	[edi], al
Incrust:		inc	edi
			dec	ecx
			jnz	Again
			add	edx, TabOffLine[1*4]
			mov	esi, ebp
			dec	bh
			jne	NextL
;----------------------
EndGraph:		ret

AffGraph		endp
  */
  // TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);

  U8 *data = (U8 *)bankgraph + numgraph * 4;
  U8 deltaX = data[0];
  U8 nbLine = data[1]; // delta Y
  S32 hotX = data[2];
  S32 hotY = data[3];

  U8 *graphics = data + 4;

// ;-------------------- Test Clipping
  S32 x0 = ScreenXMin = x + hotX;
  S32 y0 = ScreenYMin = y + hotY;
  S32 x1 = ScreenXMax = (x + hotX) + deltaX - 1;
  S32 y1 = ScreenYMax = (y + hotY) + nbLine - 1;

  if (ScreenXMin < ClipXMin || ScreenYMin < ClipYMin ||
      ScreenXMax > ClipXMax || ScreenYMax > ClipYMax)
  {
    // TODO: jmp ClippingGraph
    return 1;
  }

  x1++;
  y1++;
  x1 -= x0;
  y1 -= y0;

// ;-------------------- Calcul Offset Ecran



  return 0;
}

S32 GetBoxGraph(S32 numgraph, S32 *x0, S32 *y0, S32 *x1, S32 *y1,
                const void *bankgraph)
{
  // TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
