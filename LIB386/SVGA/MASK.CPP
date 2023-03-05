/*
;----------------------------------------------------------------------------
			.386p
			.model SMALL, C

;----------------------------------------------------------------------------
			.DATA

;----------------------------------------------------------------------------
			EXTRN   C      ClipXMin        :DWORD
			EXTRN   C      ClipYMin        :DWORD
			EXTRN   C      ClipXMax        :DWORD
			EXTRN   C      ClipYMax        :DWORD
			EXTRN   C      ScreenXMin      :DWORD
			EXTRN   C      ScreenYMin      :DWORD
			EXTRN   C      ScreenXMax      :DWORD
			EXTRN   C      ScreenYMax      :DWORD
			EXTRN   C      Log             :DWORD
			EXTRN   C      TabOffLine      :DWORD

			ASSUME DS:SEG ClipXMin

;----------------------------------------------------------------------------
comment @
		DWORD   TabOffset[]
			Brick:
			    - BYTE Delta X
			    - BYTE Delta Y
				Line(Delta Y):
				- BYTE NbBlock
				   Block 0:     Nb Zero to Jump
				   Block 1:     Nb Zero to Write
				   Block 2:     Nb Zero to Jump
					etc...
@

;----------------------------------------------------------------------------
			PUBLIC	C	ColMask

OffsetBegin     dd      0

ColMask         db      0       ; Noir par Defaut

;----------------------------------------------------------------------------
			.CODE

;----------------------------------------------------------------------------
			PUBLIC  C      AffMask

;----------------------------------------------------------------------------
;#define	ColorMask(color)	ColMask = (U8)(color)

;----------------------------------------------------------------------------

;----------------------------------------------------------------------------
;			The
			End
*/

#include <SVGA/MASK.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>
#include <stdio.h>

U8 ColMask = 0;

S32 ClippingMask(S32 x0, S32 y0, S32 x1, S32 y1, U8 *graphics)
{
	/*
;-------------------------------------------------------------------- Clipping
;               Graph : ( EBX, ECX ) ( EDX, EAX )
ClippingMask:
			cmp     ebx, ClipXMax
			jg      EndMask
			cmp     ecx, ClipYMax
			jg      EndMask
			cmp     edx, ClipXMin
			jl      EndMask
			cmp     eax, ClipYMin
			jl      EndMask

			cmp     ecx, ClipYMin
			jge     PasHaut

;---------------------- Clipping Haut, Saute ClipYMin-ECX Line(s)

			sub     ecx, ClipYMin
			mov     ebp, eax
			neg     ecx
			xor     eax, eax

NextH:                  mov     al, [esi]               ; NbBlock
			dec     ecx
			lea     esi, [esi+1+eax]        ; Jump Data
			jnz     NextH

			mov     eax, ebp

			mov     ecx, ClipYMin           ; New Y Haut

;---------------------- Clipping Bas
PasHaut:                cmp     eax, ClipYMax
			jle     PasBas
			mov     eax, ClipYMax
;---------------------- Clipping Gauche
PasBas:                 mov     OffsetBegin, 0
			cmp     ebx, ClipXMin
			jge     PasGauche
			push    eax
			mov     eax, ClipXMin
			sub     eax, ebx
			mov     OffsetBegin, eax
			pop     eax
;---------------------- Clipping Droit
PasGauche:              mov     ebp, edx
			sub     ebp, ebx
			sub     ebp, OffsetBegin
			inc     ebp

			cmp     edx, ClipXMax
			jle     PasDroit

			sub     edx, ClipXMax
			sub     ebp, edx
			mov     edx, ClipXMax
;----------------------
PasDroit:
			mov     [ScreenXMax],edx
			mov	edx,[OffsetBegin]

			add	edx,ebx
			mov     [ScreenYMin],ecx

			mov     [ScreenXMin],edx
			mov     [ScreenYMax],eax


			mov     edi, Log
			add     edi, TabOffLine[ecx*4]
			add     edi, ebx		; EDI = Buffer dest
			add	edi,[OffsetBegin]

			sub     eax, ecx
			inc     al
			mov     bh, al                  ; BH NbLine

;----------------------------------------------
			mov     al, byte ptr [ColMask]
			xor     ecx, ecx
			mov     ah, al			; AX = Col:Col

							; ESI = DATAS LINE
NextL:
			push	edi
			push	ebp
			mov     bl,[esi]		; NbBlock
			inc     esi
			mov	edx,[OffsetBegin]	; edx = Nb d'octets a sauter
Clipper:
			mov	cl,[esi]		; Nb transp
			inc	esi
			dec	bl
			je	End_Line
			sub	edx,ecx
			js	Start_Transp

			mov	cl,[esi]		; Nb Fill
			inc	esi
			sub	edx,ecx
			js	Start_Fill
			dec	bl
			jne	Clipper
			jmp	End_Line
Start_Transp:
			add	ebp,edx
			jle	End_Line
			sub	edi,edx
			jmp	Do_Fill
Start_Fill:
			neg	edx
			cmp	ebp,edx
			jge	ok
			mov	edx,ebp
ok:			sub	ebp,edx
			mov	ecx,edx
			jmp	Do_Fill_1

SameL:                  mov     cl,[esi]
			inc	esi
			dec     bl
			je      End_Line
			sub	ebp,ecx
			jle	End_Line
			add	edi,ecx
Do_Fill:
			mov     cl,[esi]
			inc	esi
			cmp	ebp,ecx
			jge	ok2
			mov	ecx,ebp
ok2:			sub	ebp,ecx
Do_Fill_1:
;			shr     ecx,1
			db	0C1h, 0E9h, 001h
			rep     stosw
			jnc     nobyte2
			mov	[edi],al
			inc	edi
nobyte2:                dec     bl
			jne     SameL

End_Line:		mov	cl,bl
			pop	ebp
			pop	edi
			add     edi,TabOffLine[1*4]
			add	esi,ecx
			dec     bh
			jne     NextL

;----------------------
EndMask:                ret
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
			graphics += nbBlock + 1; // Jump data
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
	U32 OffsetBegin = 0;
	if (x0 < ClipXMin)
	{
		OffsetBegin = ClipXMin - x0;
	}

	// PasGauche:
	// ;---------------------- Clipping Right
	S32 offsetEnd = x1 - x0 - OffsetBegin + 1;

	if (x1 > ClipXMax)
	{
		x1 -= ClipXMax;
		offsetEnd -= x1;
		x1 = ClipXMax;
	}

	// PasDroit:
	ScreenXMax = x1;
	x1 = OffsetBegin;

	x1 += x0;
	ScreenYMin = y0;
	ScreenXMin = x1;
	ScreenYMax = y1;

	U8 *screen = (U8 *)Log + TabOffLine[y0] + x0 + OffsetBegin;

	S32 deltaY = y1 - y0;
	for (int i = 0; i <= deltaY; i++)
	{
		U8 color = ColMask;
		U8 nbBlock = graphics[0];
		graphics++;
		U32 bytesToSkip = OffsetBegin;
		S32 offset = offsetEnd;

		while (nbBlock > 0)
		{
			// Clipper:
			U8 nbTransp = graphics[0];
			graphics++;
			nbBlock--;
			if (nbBlock == 0)
			{
				break;
			}

			bytesToSkip -= nbTransp;
			if (bytesToSkip < 0)
			{
				// Start_Transp:
				offset += bytesToSkip;
				if (offset <= 0)
				{
					break;
				}
				screen -= bytesToSkip;
				// Do_Fill:
				U8 counter = graphics[0];
				graphics++;
				if (offset < nbTransp)
				{
					nbTransp = offset;
				}
				offset -= nbTransp;

				// Do_Fill_1:
				for (int j = 0; j < nbTransp; j++)
				{
					*screen = color;
					screen++;
				}

				nbBlock--;
				if (nbBlock == 0)
				{
					break;
				}
			}

			U8 nbFill = graphics[0];
			graphics++;
			bytesToSkip -= nbFill;
			if (bytesToSkip < 0)
			{
				// Start_Fill:
				bytesToSkip = -bytesToSkip;
				if (offset < bytesToSkip)
				{
					bytesToSkip = offset;
				}
				offset -= bytesToSkip;
				nbFill = bytesToSkip;
				// Do_Fill_1:
				for (int j = 0; j < nbFill; j++)
				{
					*screen = color;
					screen++;
				}

				nbBlock--;
				if (nbBlock == 0)
				{
					break;
				}
			}

			nbBlock--;
			if (nbBlock == 0)
			{
				break;
			}
		}

		screen += TabOffLine[1];
		graphics += nbBlock;
	}

	return 0; // TODO: Remove, make void
}

S32 AffMask(S32 nummask, S32 x, S32 y, void *bankmask)
{
	/*
;S32	AffMask(S32 nummask, S32 x, S32 y, void *bankmask)	;

;#pragma aux AffMask	"*"		\
;	parm	[eax] [ebx] [ecx] [esi]	\
;	modify	[edx edi]

AffMask                 proc    uses ebp

			add     esi, [esi+eax*4]; ESI = Begin Data

			xor     eax, eax
			xor	edx, edx
			mov     al, [esi+2]
			mov     dl, [esi+3]
			add     ebx, eax        ; Hot X
			add     ecx, edx        ; Hot Y

;-----------------------------------------------
			mov     dl, [esi]       ; Delta X
			mov     al, [esi+1]     ; Nb Lines ( Delta Y )
			add     esi, 4          ; Jump HotX & HotY

;----------------------------------------------- Test Clipping
			add     edx, ebx
			add     eax, ecx
			dec     edx
			dec     eax

			cmp     ebx, ClipXMin
			jl      ClippingMask
			cmp     ecx, ClipYMin
			jl      ClippingMask
			cmp     edx, ClipXMax
			jg      ClippingMask
			cmp     eax, ClipYMax
			jg      ClippingMask

			mov     [ScreenXMin],ebx
			mov     [ScreenXMax],edx
			mov     [ScreenYMin],ecx
			mov     [ScreenYMax],eax

			inc     edx
			inc     eax
			sub     edx, ebx
			sub     eax, ecx
			; edx = delta X
			; eax = delta Y

;----------------------------------------------- Compute Offset Ecran
			mov     edi, Log
			sub     edx, TabOffLine[1*4]; EDX = Offset Screen

			add     edi, TabOffLine[ecx*4]
			xor     ecx, ecx        ; Raz Compteur

			add     edi, ebx
			mov     bh, al          ; BH = NbLine

			neg     edx             ; EDX = width-edx

			mov     al, byte ptr [ColMask]; mask color
			mov     ebp, edx        ; EBP Offset Next Line

			mov     ah, al

;----------------------------------------------- Init NbBlock for this line
NextLine:               mov     bl, [esi]       ; BL = Nb Block for this line
			inc     esi

;----------------------------------------------- Manage One Line
SameLine:               mov     cl, [esi]       ; Nb Zero to Jump
			add     edi, ecx        ; Incrust
			dec     bl
			je      EndBlock
			mov     cl, [esi+1]     ; Nb Zero to Write
			add     esi, 2
;			shr     ecx,1
			db	0C1h, 0E9h, 001h
			rep     stosw           ; Write Zero
			jnc     nobyte
			mov	[edi],al
			inc	edi
nobyte:                 dec     bl              ; Nb Block--
			jne     SameLine        ; Continue Same Line
			add     edi, ebp        ; EDI += Offset Next Line
			dec     bh              ; NbLine--
			jne     NextLine        ; Next Line
			ret
EndBlock:
			inc     esi
			add     edi, ebp        ; EDI += Offset Next Line
			dec     bh              ; NbLine--
			jne     NextLine        ; Next Line
			ret
AffMask			endp
	*/
  U8 *data = (U8 *)bankmask + ((U32 *)bankmask)[nummask];
	U8 deltaX = data[0];
	U8 nbLine = data[1]; // delta Y
	S32 hotX = data[2];
	S32 hotY = data[3];

  U8 *graphics = data + 4;

	x += hotX;
	y += hotY;

	S32 x1 = x + deltaX - 1;
	S32 y1 = y + nbLine - 1;

	// ----------------------------------------------- Test Clipping
	if (x < ClipXMin || y < ClipYMin || x1 > ClipXMax || y1 > ClipYMax)
	{
		return ClippingMask(x, y, x1, y1, graphics);
	}

	ScreenXMin = x;
	ScreenXMax = x1;
	ScreenYMin = y;
	ScreenYMax = y1;

	// ----------------------------------------------- Compute Offset Ecran
	U8 *screen = (U8 *)Log + TabOffLine[y] + x;
	U32 offsetScreen = TabOffLine[1] - deltaX;
	U8 color = ColMask;

	for (int i = 0; i < nbLine; i++)
	{
		// ;-------------------- Init NbBlock for this line
		// NextLine:
		U8 nbBlock = graphics[0];
		graphics++;

		while (nbBlock > 0)
		{
			// ;-------------------- Manage One Line
			// SameLine:
			U8 toJump = graphics[0]; // Nb Zero to jump
			screen += toJump;
			nbBlock--;
			if (nbBlock == 0)
			{
				// EndBlock:
				graphics++;
				break;
			}

			U8 toWrite = graphics[1]; // Nb Zero to write
			graphics += 2;
			for (int k = 0; k < toWrite; k++)
			{
				*(screen++) = color;
			}

			nbBlock--;
		}
		screen += offsetScreen;
	}

	return 0; // TODO: make it void?
}
