#include <SVGA/GRAPH.H>

#include <SVGA/SCREENXY.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

/*
;----------------------------------------------------------------------------
;               DWORD   TabOffset[]     ( Un de plus pointe fin de fichier )
;
;                       Brick:
;
;                           - BYTE Delta X
;                           - BYTE Delta Y
;                           - BYTE HotX (Signed)
;                           - BYTE HotY (Signed)
;
;                               Pour chaque Ligne ( Delta Y ):
;
;                               - BYTE NbBlock
;
;                                  Par Block:
;                                   - 00xxxxxxb  xxxxxx zeros to jump
;                                   - 01xxxxxxb  xxxxxx Copy Pix
;                                   - 10xxxxxxb  xxxxxx Repeat Pix
;                                   - BYTE datas[]

;----------------------------------------------------------------------------
*/
// OffsetBegin	dd	0
// NbPix		dd	0
U8 BufferClip[512];

// -----------------------------------------------------------------------------
S32 GetDxDyGraph(U8 numgraph, S32 *x, S32 *y, const void *bankgraph) {
  const U32 *graphMem = (const U32 *)bankgraph;
  const U8 *graphBytes = (const U8 *)bankgraph;

  // Don't know if the value is really a signed int, but works for now
  U32 offset = graphMem[numgraph];

  *x = (graphBytes[offset] & 0xFF);
  *y = (graphBytes[offset + 1] & 0xFF);

  return 0; // TODO: Remove, make void
}

#include <stdio.h>

S32 ClippingGraph(S32 x0, S32 y0, S32 x1, S32 y1, U8 *graphics);

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

EndGraph:		ret

AffGraph		endp
  */
  U8 *data = (U8 *)bankgraph + ((U32 *)bankgraph)[numgraph];
  U8 deltaX = data[0];
  U8 nbLine = data[1]; // delta Y
  S8 hotX = data[2];
  S8 hotY = data[3];

  U8 *graphics = data + 4;

// ;-------------------- Test Clipping
  S32 x0 = ScreenXMin = x + hotX;
  S32 y0 = ScreenYMin = y + hotY;
  S32 x1 = ScreenXMax = (x + hotX) + deltaX - 1;
  S32 y1 = ScreenYMax = (y + hotY) + nbLine - 1;

  if (x0 < ClipXMin || y0 < ClipYMin ||
      x1 > ClipXMax || y1 > ClipYMax)
  {
    return ClippingGraph(x0, y0, x1, y1, graphics);
  }

  x1++;
  y1++;
  x1 -= x0;
  y1 -= y0;

// ;-------------------- Calcul Offset Ecran
	U8 *screen = (U8 *)Log + TabOffLine[y0] + x0;
	U32 offsetScreen = TabOffLine[1] - x1;

	for (int i = 0; i < nbLine; i++)
	{
// ;-------------------- Init NbBlock for this line
		// NextLine:
		U8 nbBlock = graphics[0];
		graphics++;

		// ;-------------------- Manage One Line
		for (int j = 0; j < nbBlock; j++)
		{
			// SameLine:
			U8 control = graphics[0];
			U8 opCode = (control & 0b11000000) >> 6;
			U32 counter = control & 0b00111111;
			if (opCode == 0) // Incrust
			{
				// JumpZero:
				graphics++;
				screen = screen + counter + 1;
			}
			else
			{
				counter++; // One More Please...
				if (opCode == 1)
				{
					// jmp	WriteDiffPix	; Pix Differents
					graphics++;
					while (counter > 0)
					{
						*(screen++) = *(graphics++);
						counter--;
					}
				}
				else
				{
					// ;-------------------- Repeat Same Color
					U8 color = graphics[1]; // Color to Repeat
					graphics += 2;
					while (counter > 0)
					{
						*(screen++) = color;
						counter--;
					}
				}
			}
		}
		screen += offsetScreen;
	}

  return 0;
}

S32 ClippingGraph(S32 x0, S32 y0, S32 x1, S32 y1, U8 *graphics)
{
	/*
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
	*/
	if (x0 > ClipXMax || y0 > ClipYMax ||
		x1 < ClipXMin || y1 < ClipYMin)
	{
		return 0;
	}

	if (y0 < ClipYMin)
	{
		// ---------------------- Clipping Top, Skip ClipYMin-ECX Line(s)

		S32 yDiff = ClipYMin - y0;
		for (int i = 0; i < yDiff; i++)
		{
			// NextH:
			U8 nbBlock = graphics[0];
			graphics++;
			for (int j = 0; j < nbBlock; j++)
			{
				// SameH:
				U8 control = graphics[0];
				graphics++;
				U8 opCode = (control & 0b11000000) >> 6;
				if (opCode != 0)
				{
					U32 jump = 0;
					if (opCode == 1)
					{
						jump = control & 0b00111111;
					}
					graphics += jump + 1;
				}
			}
		}

		y0 = ClipYMin;
	}

	// ;---------------------- Clipping Bottom
	// PasHaut:
	if (y1 > ClipYMax)
	{
		y1 = ClipYMax;
	}

	// ;---------------------- Clipping Left
	// PasBas:
	U32 offset = 0;
	if (x0 < ClipXMin)
	{
		offset = ClipXMin - x0;
	}

	// PasGauche:
	U32 OffsetBegin = offset;
	// ;---------------------- Clipping Right
	offset += x0 - 1;
	offset -= x1;
	offset = -offset;
	U32 NbPix = offset;
	x1 -= ClipXMax;

	if (x1 > 0)
	{
		NbPix -= x1;
	}

	// PasDroit:
	U8 *screen = (U8 *)Log + TabOffLine[y0] + x0;
	S32 deltaY = y1 - y0;
	for (int i = 0; i <= deltaY; i++)
	{
		U8 *buffer = BufferClip;
		U8 nbBlock = graphics[0];
		graphics++;

		for (int j = 0; j < nbBlock; j++)
		{
			U8 control = graphics[0];
			graphics++;
			U8 opCode = (control & 0b11000000) >> 6;
			U32 counter = control & 0b00111111;
			counter++;
			if (opCode == 0)
			{
				// jmp JumpL
				while (counter > 0)
				{
					*(buffer++) = 0;
					counter--;
				}
			}
			else if (opCode == 1)
			{
				// jne	WriteDiffL
				while (counter > 0)
				{
					*(buffer++) = *(graphics++);
					counter--;
				}
			}
			else
			{
				U8 color = graphics[0];
				graphics++;
				while (counter > 0)
				{
					*(buffer++) = color;
					counter--;
				}
			}
		}

		U8 *src = BufferClip + OffsetBegin;
		U8 *dst = screen + OffsetBegin;
		for (int j = 0; j < NbPix; j++)
		{
			U8 value = *(src++);
			if (value != 0)
			{
				*dst = value;
			}
			dst++;
		}

		screen += TabOffLine[1];
	}

	return 0; // TODO: Remove, make void
}

S32 GetBoxGraph(S32 numgraph, S32 *x0, S32 *y0, S32 *x1, S32 *y1,
                const void *bankgraph)
{
	/*
;----------------------------------------------------------------------------
;S32	GetBoxGraph(S32 numgraph, S32 *x0, S32 *y0, S32 *x1, S32 *y1, void *bankgraph);

;#pragma aux GetBoxGraph	"*"				\
;	parm		[eax] [ebx] [ecx] [edx] [edi] [esi]	\
;	modify exact	[eax ebx ecx esi]

GetBoxGraph	proc \
			uses eax ebx ecx edx edi esi ebp,
			numgraph:DWORD,\
			x0:DWORD, y0:DWORD,\
			x1:DWORD, y1:DWORD,\
			bankgraph:DWORD

			mov eax, numgraph
			mov ebx, x0
			mov ecx, y0
			mov edx, x1
			mov edi, y1
			mov esi, bankgraph

			add	esi, [esi+eax*4]

			movsx	eax, byte ptr[esi+2]		; Hot X

			movsx	ebp, byte ptr[esi+3]		; Hot Y

			mov	dword ptr[ebx], eax
			mov	dword ptr[ecx], ebp

			xor	ebx, ebx
			xor	ecx, ecx

			mov	bl, byte ptr[esi]		; Dx
			mov	cl, byte ptr[esi+1]		; Dy

			add	eax, ebx
			add	ebp, ecx

			mov	dword ptr[edx], eax
			mov	dword ptr[edi], ebp

			ret

GetBoxGraph		endp
	*/
	U8 *bank = (U8 *)bankgraph + ((U32 *)bankgraph)[numgraph];

	U8 dx = bank[0];
	U8 dy = bank[1];
	U8 hotX = bank[2];
	U8 hotY = bank[3];

	*x0 = hotX;
	*y0 = hotY;
	*x1 = hotX + dx;
	*y1 = hotY + dy;

  return 0;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
