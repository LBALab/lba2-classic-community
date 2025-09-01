/*
;*──────────────────────────────────────────────────────────────────────────*
			.386P
			.MODEL	FLAT, C

			.DATA

;*──────────────────────────────────────────────────────────────────────────*
; Runtime code patch refactor, uses memory instead of instruction substitution
BlockSize DWORD 2
;*──────────────────────────────────────────────────────────────────────────*
			.CODE

;*──────────────────────────────────────────────────────────────────────────*
			public	C	ExpandLZ

;*──────────────────────────────────────────────────────────────────────────*
;void	ExpandLZ(void *Dst, void *Src, U32 DecompSize, U32 MinBloc)

;#pragma aux ExpandLZ			\
;	parm	[edi] [ebx] [ecx] [eax]	\
;	modify	[edx esi]

ExpandLZ		PROC \
			uses eax ebx ecx edx edi esi ebp\
			Dst: DWORD, Src: DWORD, DecompSize: DWORD, MinBloc: DWORD

			mov edi, Dst
			mov ebx, Src
			mov ecx, DecompSize
			mov eax, MinBloc

			mov	[BlockSize], eax

			push	ebp

			mov	ebp, ecx		; EBP = decomp size
			mov	dl, 8			; 1 octet info pour 8 data de 8 ou
							; 16 bits (ca depend du bit...)
boucle:
			mov	dh,[ebx]
			inc	ebx		 	; octet d'info dans bl
next:
			db	0C0h, 0EEh, 01		; SHR DH,1
							; OpCode 3 octets (gagne 1 cycle)

			jnc	copie			; chaque bit indique la nature de
							; ce qui suit

			mov	al,[ebx]		; ici un octet de data a recopier
			inc	ebx
			mov	[edi],al
			inc	edi

			dec	ebp			; 1 de moins
			je	termine			; si c'est fini, c'est fini

			dec	dl			; 1 bit de moins
			jne	next			; s'il en reste on continue
			mov	dl,8
			jmp	boucle			; sinon on lit un octet d'info de plus

copie:							; ici on a 16 bits formant un offset
							; (sur 12 bits) et une longueur
							; (sur 4 bits)

			movzx	eax, word ptr[ebx]	; On lit 16 bits

			mov	ecx, eax		; On recopie dans ECX, qui servira de
			xor	eax, -1			; bloc de longeur < 2 (d'ou gain...)

			sar	eax, 4
			and	ecx, 0Fh		; longueur; on masque nos registres

			add	ebx, 2
			add	ecx, [BlockSize]			; on ajuste car pas de copie de moins de 2 (ou 3)

							; XOR EAX,-1  <=> NOT EAX, sauf que ca
							; permet le "pairing"
							; le "NOT EAX" : on recupere l'offset
							; en fait c'est un decalage en
							; arrière - 1 (not eax = -eax - 1)
			lea	esi, [edi+eax]  	; par rapport à edi (esi = edi - eax - 1)
			sub	ebp, ecx		; MAJ du Counter

			cmp	eax,-1			; Explications:
			je	@@Fast_Stos		; A cause de la possibilite
							; d'une compression style Run Length Encoding
							; du LZ77, on ne peut pas faire de "MOVSW" (bravo a Laurent
							; pour avoir compris ce probleme!!)
							; du coup, on test si il ne s'agit que d'une recopie de bloc,
							; ou bien d'une copie du meme octet.

					; *** Copie de bloc ***

			db	0C1h, 0E9h, 001h	; shr ecx, 1
			jnc	okw

			mov	al, [esi]
			inc	esi

			mov	[edi], al
			inc	edi
okw:
			rep	movsw			; on fait la recopie

			test	ebp, ebp		; il en reste ?
			je	termine			; ...

			dec	dl			; il reste des bits d'info ?
			jne	next                    ; ...

			mov	dl,8
			jmp	boucle                  ; ...

							; *** Copie d'octet ***
@@Fast_Stos:
			mov	al,[esi]
			test	edi,1

			mov	ah,al
			je	ALIGN_Ok

			mov	[edi],al
			dec	ecx

			inc	edi
ALIGN_Ok:
			db	0C1h, 0E9h, 01h		; shr ecx, 1

			rep	stosw

			jnc	Fast_Done

			mov	[edi],al
			inc	edi
Fast_Done:
			test	ebp, ebp		; il en reste ?
			je	termine			; ...

			dec	dl			; il reste des bits d'info ?
			jne	next                    ; ...

			mov	dl,8
			jmp	boucle                  ; ...
termine:
			pop	ebp
			ret

ExpandLZ		ENDP

;*──────────────────────────────────────────────────────────────────────────*

			end
*/

#include <SYSTEM/LZ.H>
#include <stdio.h>
#include <stdlib.h>

void	ExpandLZ(void *Dst, void *Src, U32 DecompSize, U32 MinBloc)
{
	U8 *dst = (U8 *)Dst;
	U8 *src = (U8 *)Src;
	U32 srcOffset = 0;
	U32 dstOffset = 0;

	while (dstOffset < DecompSize)
	{
		// Check the next flag bit
		U8 flag = src[srcOffset++];
		for (int i = 0; i < 8; i++)
		{
			if (dstOffset >= DecompSize)
			{
				break;
			}
			if ((flag & 0x1) != 0)
			{
				// Literal byte
				dst[dstOffset++] = src[srcOffset++];
			}
			else
			{
				// Back-reference
				U32 blockLength = (src[srcOffset] & 0x0f) + MinBloc;
				U32 blockOffset = ((U32)(src[srcOffset + 1]) << 4) | (src[srcOffset] >> 4);

				for (U32 j = 0; j < blockLength; j++)
				{
					dst[dstOffset] = dst[dstOffset - blockOffset - 1];
					dstOffset++;

					if (dstOffset >= DecompSize)
					{
						break;
					}
				}
				srcOffset += 2;
			}
			flag >>= 1;
		}
	}
}
