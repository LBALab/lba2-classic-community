/*


		.686p
		.XMM

		.model SMALL, C

		.data

		extrn   C 	Log		:DWORD
		extrn	C 	TabOffLine	:DWORD

FireSeed	dd	1876351

		.code

		public	C	Do_Fire

;══════════════════════════════════════════════════════════════════════════
;                           █▀▀▀▀  █    █▀▀▀█ █▀▀▀▀
;                           ██▀▀   ██   ██▀█▀ ██▀▀
;                           ▀▀     ▀▀   ▀▀  ▀ ▀▀▀▀▀
;══════════════════════════════════════════════════════════════════════════

; grossse optimisations à faire ......

;──────────────────────────────────────────────────────────────────────────

Do_Fire		proc uses esi edi ebx ebp ,\
		ptrwork1:dword, ptrwork2:dword, colortable:DWORD, ptrtex:dword

		; genere 2 lignes aleatoires

		mov	esi, ptrwork1
		add	esi, 32*34
		mov	edx, 32*2

loopg:		mov	eax, [ds:FireSeed]
		mov	cl, ah
		rol	eax, cl
		imul	ax, dx
		add	[ds:FireSeed], 5137827
		and	eax, 128
		mov	[esi], al
gena:
		inc	esi
		dec	edx
		jnz	loopg

		; map smooth avec bouclage

		mov	esi, ptrwork1
		mov	edi, ptrwork2

		add	esi, 32
		add	edi, 32

		xor	edx, edx		; line cmpt

		mov	ecx, 32
;		sub	ecx, 2			; pas en haut, pas en bas
		add	ecx, 2
;		dec	ecx

loopy:		push	ecx

		mov	ecx, 32
		xor	ebx, ebx

loopx:		xor	eax, eax

;		push	edx

;		xor	edx, edx		; mov dl, ...

		sub	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

		add	ebx, 32
		add	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

		dec	ebx
		and	ebx, 31
		mov	dl, [esi+ebx]
		add	eax, edx

		sub	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

		add	ebx, 32
		add	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

		add	ebx, 2
		and	ebx, 31
		mov	dl, [esi+ebx]
		add	eax, edx

		sub	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

		add	ebx, 32
		add	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

;		pop	edx

		ror	ax, 3			; moyenne /8

		test	ah, 60h			; user param ?
		jnz	nodec			; masturbe le feu

dodec:		or	al, al
		jz	nodec

		dec	eax
nodec:
		mov	[edi], al
		inc	edi

		and	ebx, 31			; col 0-31

		dec	ecx
		jnz	loopx

		add	esi, 32

;		inc	edx			; lig

		pop	ecx
		dec	ecx
		jnz	loopy

		; copy scroll results

		mov	esi, ptrwork2
		add	esi, 32
		mov	edi, ptrwork1
		mov	ecx, 32 * 34 / 4
		; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
		;   is it confusing it with some other instruction? Is it not supported by 486?
		rep	movsd

		; genere texture

		mov	esi, ptrwork1
		mov	edi, ptrtex
		mov	ebx, colortable
		mov	ecx, 32			; Y
		xor	eax, eax
loopyb:
		push	ecx
		mov	ecx, 32			; X

loopxb:
		mov	al, [esi]

;		shr	al, 2
;		cmp	al, 15
;		jbe	coulok
;		mov	al, 15

		shr	al, 1
		cmp	al, 31
		jbe	coulok
		mov	al, 31
coulok:
		mov	al, [ebx+eax]
		mov	[edi], al

		inc	esi
		inc	edi

		dec	ecx
		jnz	loopxb

		add	edi, 256 - 32

		pop	ecx
		dec	ecx
		jnz	loopyb

		ret


Do_Fire		endp
*/

#include "FIRE.H"

#include <SYSTEM/UTILS.H>

#include <string.h>

U32 FireSeed = 1876351;

//void Do_Fire( U8 *buff1, U8 *buff2, U8 *colortable, U8 *TileMap )
// uses           esi            edi            ebx            ebp
void Do_Fire(U8 *ptrwork1, U8 *ptrwork2, U8 *colortable, U8 *ptrtex)
{
  /*
Do_Fire		proc uses esi edi ebx ebp ,\
		ptrwork1:dword, ptrwork2:dword, colortable:DWORD, ptrtex:dword

		; genere 2 lignes aleatoires

		mov	esi, ptrwork1
		add	esi, 32*34
		mov	edx, 32*2

loopg:		mov	eax, [ds:FireSeed]
		mov	cl, ah
		rol	eax, cl
		imul	ax, dx
		add	[ds:FireSeed], 5137827
		and	eax, 128
		mov	[esi], al
gena:
		inc	esi
		dec	edx
		jnz	loopg

		; map smooth avec bouclage

		mov	esi, ptrwork1
		mov	edi, ptrwork2

		add	esi, 32
		add	edi, 32

		xor	edx, edx		; line cmpt

		mov	ecx, 32
;		sub	ecx, 2			; pas en haut, pas en bas
		add	ecx, 2
;		dec	ecx

loopy:		push	ecx

		mov	ecx, 32
		xor	ebx, ebx

loopx:		xor	eax, eax

;		push	edx

;		xor	edx, edx		; mov dl, ...

		sub	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

		add	ebx, 32
		add	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

		dec	ebx
		and	ebx, 31
		mov	dl, [esi+ebx]
		add	eax, edx

		sub	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

		add	ebx, 32
		add	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

		add	ebx, 2
		and	ebx, 31
		mov	dl, [esi+ebx]
		add	eax, edx

		sub	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

		add	ebx, 32
		add	ebx, 32
		mov	dl, [esi+ebx]
		add	eax, edx

;		pop	edx

		ror	ax, 3			; moyenne /8

		test	ah, 60h			; user param ?
		jnz	nodec			; masturbe le feu

dodec:		or	al, al
		jz	nodec

		dec	eax
nodec:
		mov	[edi], al
		inc	edi

		and	ebx, 31			; col 0-31

		dec	ecx
		jnz	loopx

		add	esi, 32

;		inc	edx			; lig

		pop	ecx
		dec	ecx
		jnz	loopy

		; copy scroll results

		mov	esi, ptrwork2
		add	esi, 32
		mov	edi, ptrwork1
		mov	ecx, 32 * 34 / 4
		; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
		;   is it confusing it with some other instruction? Is it not supported by 486?
		rep	movsd

		; genere texture

		mov	esi, ptrwork1
		mov	edi, ptrtex
		mov	ebx, colortable
		mov	ecx, 32			; Y
		xor	eax, eax
loopyb:
		push	ecx
		mov	ecx, 32			; X

loopxb:
		mov	al, [esi]

;		shr	al, 2
;		cmp	al, 15
;		jbe	coulok
;		mov	al, 15

		shr	al, 1
		cmp	al, 31
		jbe	coulok
		mov	al, 31
coulok:
		mov	al, [ebx+eax]
		mov	[edi], al

		inc	esi
		inc	edi

		dec	ecx
		jnz	loopxb

		add	edi, 256 - 32

		pop	ecx
		dec	ecx
		jnz	loopyb

		ret
Do_Fire		endp
  */
// char __usercall Do_Fire@<al>(int a1@<ebp>)
  U8 *v1; // esi@1
  signed int v2; // edx@1
  U32 v3; // eax@2
  U8 *v4; // esi@3
  U8 *v5; // edi@3
  U16 v6; // dx@3
  int v7; // ecx@3
  signed int v8; // ecx@4
  int v9; // ebx@4
  int v10; // ebx@5
  S16 v11; // ax@5
  S16 v12; // ax@5
  int v13; // ebx@5
  S16 v14; // ax@5
  S16 v15; // ax@5
  S16 v16; // ax@5
  int v17; // ebx@5
  S16 v18; // ax@5
  U16 v19; // ax@5
  int v20; // ebx@5
  S16 v21; // ax@5
  U8 *v22; // esi@10
  U8 *v23; // edi@10
  U8 *v24; // ebx@10
  int v25; // ecx@10
  int v26; // eax@10
  signed int v27; // ecx@11
  U8 result; // al@14
  int v29; // [sp-4h] [bp-14h]@4
  int v30; // [sp-4h] [bp-14h]@11

  // genere 2 lignes aleatoires

  v1 = ptrwork1 + 1088;
  v2 = 64;
  do
  {
    v3 = ROL32(FireSeed, (FireSeed >> 8) & 0xFF);
    v3 = ((v3 & 0xFFFF0000) + (v3 & 0xFFFF) * v2) & 0x80;
    FireSeed += 5137827;
    *v1 = v3 & 0xFF;
    v1++;
    v2--;
  }
  while (v2 > 0);

  // map smooth avec bouclage

  v4 = ptrwork1 + 32;
  v5 = ptrwork2 + 32;
  v6 = 0; // line cmpt
  v7 = 34;
  do
  {
    v29 = v7;
    v8 = 32;
    v9 = 0;
    do
    {
      v10 = v9 - 32;
      v6 = *v4 + v10;
      v11 = v6;

      v10 += 64;
      v6 = *v4 + v10;
      v12 = v6 + v11;

      v13 = (v10 - 1) & 0x1F;
      v6 = *v4 + v13;
      v14 = v6 + v12;

      v13 -= 32;
      v6 = *v4 + v13;
      v15 = v6 + v14;

      v13 += 64;
      v6 = *v4 + v13;
      v16 = v6 + v15;

      v17 = (v13 + 2) & 0x1F;
      v6 = *v4 + v17;
      v18 = v6 + v16;

      v17 -= 32;
      v6 = *v4 + v17;
      v19 = v6 + v18;

      v20 = v17 + 64;
      v6 = *v4 + v20;

      v21 = ROR16(v6 + v19, 3);

      if ((v21 & 0x6000) == 0 && (v21 & 0xFF))
      {
        v21--;
      }
      *v5++ = v21 & 0xFF;
      v9 = v20 & 0x1F;
      --v8;
    }
    while ( v8 );
    v4 += 32;
    v7 = v29 - 1;
  }
  while ( v29 != 1 );

  // copy scroll results
  memcpy(ptrwork1, ptrwork2 + 32, 32 * 34);

  // genere texture
  v22 = ptrwork1;
  v23 = ptrtex;
  v24 = colortable;
  v25 = 32; // Y
  v26 = 0;
  do
  {
    v30 = v25;
    v27 = 32; // X
    do
    {
      v26 = *v22 >> 1;
      if (v26 > 31)
      {
        v26 = 31;
      }
      *v23 = *(v24 + v26);
      ++v22;
      ++v23;
      --v27;
    }
    while ( v27 );
    v23 += 224;
    v25 = v30 - 1;
  }
  while ( v30 != 1 );
}
