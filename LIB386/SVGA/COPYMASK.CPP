/*
    DWORD	TabOffset[]
      Brick:
          - BYTE Delta X
          - BYTE Delta Y
        Line(Delta Y):
        - BYTE NbBlock
           Block 0:	Nb Zero to Jump
           Block 1:	Nb Zero to Write
           Block 2:	Nb Zero to Jump
          etc...
*/

#include <SVGA/COPYMASK.H>

#include <string.h>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

typedef struct {
	U8 DeltaX;
	U8 DeltaY;
	U8 HotX;
	U8 HotY;
} Struc_Mask_Header;

void CopyMask(S32 nummask, S32 x, S32 y, U8 *bankmask, void *src)
{
  /*
;#pragma aux CopyMask	"*"			\
;	parm	[eax] [ebx] [ecx] [esi] [edx]	\
;	modify	[edi]

CopyMask		PROC \
			uses ebx ecx edx edi esi ebp\
			nummask: DWORD, x: DWORD, y: DWORD, bankmask: DWORD, src: DWORD
			mov eax, nummask
			mov ebx, x
			mov ecx, y
			mov esi, bankmask
			mov edx, src

			add	esi, [esi+eax*4]	; ESI = Begin Data

			mov	ebp, edx
			xor	eax, eax

			xor	edx, edx
			mov	al, [esi].STRUC_MASK.Mask_HotX	; Hot X

			mov	dl, [esi].STRUC_MASK.Mask_HotY	; Hot Y
			add	ebx, eax

			add	ecx, edx
;-----------------------------------------------
			mov	dl, [esi].STRUC_MASK.Mask_DeltaX	; Delta X

			mov	al, [esi].STRUC_MASK.Mask_DeltaY	; Nb Line ( Delta Y )
			add	esi, Size STRUC_MASK	; Skip header

;----------------------------------------------- Test Clipping
			lea	edx, [edx+ebx-1]
			lea	eax, [eax+ecx-1]

			cmp	ebx, [ClipXMin]
			jl	ClippingMask
			cmp	ecx, [ClipYMin]
			jl	ClippingMask
			cmp	edx, [ClipXMax]
			jg	ClippingMask
			cmp	eax, [ClipYMax]
			jg	ClippingMask

			sub	edx, ebx
			sub 	eax, ecx

			inc	edx
			inc	eax

;----------------------------------------------- Calcul Offset Ecran
			add	ebx, TabOffLine[ecx*4]
			mov	edi, [Log]

			add	edi, ebx
			add	ebp, ebx

			mov	bh, al			; BH = NbLine
			sub	edx, TabOffLine[1*4]	; EDX = Offset Screen
			neg	edx			; EDX = Screen_X-edx
			xor	ecx, ecx		; Maz Compteur
			xor	eax,eax
;----------------------------------------------- Init NbBlock for this line
NextLine:		mov	bl, [esi]	; Nb Block for this line
			inc	esi


;----------------------------------------------- Manage One Line
SameLine:		mov	al, [esi]       ; Nb Zero to Jump
			inc	esi

			add	edi, eax	; Incrust on Log
			add	ebp, eax	; And on PtSrc

			dec	bl
			jz	EndBlock

			mov	cl, [esi]	; Nb Zero to Write
			inc	esi

			push	ecx
			push	edi
@@LoopLoadCache:
			mov	al,[edi]
			add	edi,32
			sub	ecx,32
			jg	@@LoopLoadCache
			pop	edi
			pop	ecx

loopb:			mov	al, [ebp]
			inc	ebp
			mov	[edi], al
			inc	edi
			dec	ecx
			jnz	loopb
nob:
			dec	bl		; Nb Block--
			jne	SameLine	; Continue Same Line
EndBlock:

			add	edi, edx	; EDI += Offset Screen
			add	ebp, edx

			dec	bh		; NbLine--
			jne	NextLine	; Next Line
;----------------------------------------------- Incrust
			ret
;-------------------------------------------------------------------- Clipping
;		Graph : ( EBX, ECX ) ( EDX, EAX )
ClippingMask:
			cmp	ebx, ClipXMax
			jg      EndMask
			cmp	ecx, ClipYMax
			jg	EndMask
			cmp	edx, ClipXMin
			jl	EndMask
			cmp	eax, ClipYMin
			jl	EndMask

			cmp	ecx, [ClipYMin]
			jge	PasHaut

;---------------------- Clipping Haut, Saute ClipYMin-ECX Line(s)
			sub	ecx, [ClipYMin]
			neg	ecx
			mov	edi, eax
			xor	eax, eax

NextH:			mov	al, [esi]
			lea	esi, [esi+eax+1]
			dec	ecx
			jnz	NextH

			mov	ecx, [ClipYMin]
			mov	eax, edi

;---------------------- Clipping Bas
PasHaut:		cmp	eax, [ClipYMax]
			jle	PasBas
			mov	eax, [ClipYMax]

;---------------------- Clipping Gauche
PasBas:                 mov	[OffsetBegin], 0
			cmp     ebx, [ClipXMin]
			jge	PasGauche

			mov	edi, [ClipXMin]
			sub	edi, ebx
			mov	[OffsetBegin], edi

;----------------------	Clipping Droit
PasGauche:		mov	edi, edx
			sub	edi, ebx
			sub	edi, [OffsetBegin]
			inc	edi

			cmp	edx, [ClipXMax]
			jle	PasDroit

			sub	edx, [ClipXMax]
			sub	edi, edx
			mov	edx, [ClipXMax]
PasDroit:
			mov	[NbPix], edi
;----------------------

			; ESI debut data Y ok pas X
			; EBX not clipped X0
			; ECX good Y0
			; EAX clipped Y1
			; NbPix real X nb pix
			; OffsetBegin start X  ( !!!!! < 128 )

			mov	edi,esi
			mov	esi,ebp
			mov	ebp,edi
			mov	edi, TabOffLine[ecx*4]
			add	edi, ebx
			add	esi,edi

			add	edi, [Log]

			mov	edx, eax
			sub	edx, ecx
			inc	edx

			xor	ecx, ecx

l1:     		push	esi
			push	edi

			mov	bl, byte ptr[ebp]	; nb blocs
			inc	ebp

			mov	bh, byte ptr[NbPix]	; nb point à copier

			mov	ecx, [OffsetBegin]	; start x
			mov	ah, cl			; nb pix count
			add	edi, ecx		; new pos screen
			add	esi, ecx

l0:			mov	al, byte ptr[ebp]	; nb pix to jump
			inc	ebp
			dec	bl
			sub	ah, al
			jz	okdraw			; tombe pile
			jc	rptj			; reste pix to jump
			test	bl, bl
			jz	aplus

			mov	al, byte ptr[ebp]	; nb copy point
			inc	ebp
			dec	bl
			sub	ah, al
			jz	okjump
			jc	rgtc			; reste graph to copy

			test	bl, bl		; jmp l0
			jnz	l0
			jmp	aplus

rptj:			neg	ah			; ah nb pix to jump
			mov	cl, ah
			sub	bh, ah			; reste à ecrire
			jle	aplus
			add	edi, ecx
			add	esi, ecx
			jmp	okdraw

rgtc:			neg	ah			; al nb pix to copy
			mov	cl, ah
			sub	bh, ah			; reste à ecrire
			jc	cpt			; copy pas tout
			rep	movsb			; copy
			jz	aplus			; tombe pile
			jmp	okjump

cpt:			add	cl, bh
			rep	movsb
			jmp	aplus

okjump:			mov	cl, byte ptr[ebp]	; al nb pix to jump
			inc	ebp
			dec	bl
			jz	aplus
			sub	bh, cl			; reste à ecrire
			jle	aplus			; fini
			add	edi, ecx
			add	esi, ecx

okdraw:			mov	cl, byte ptr[ebp]	; nb pix to copy
			inc	ebp
			dec	bl
			sub	bh, cl			; reste à ecrire
			jc	cpt			; copy pas tout
			rep	movsb			; copy
			jz	aplus			; tombe pile
			test	bl,bl			; nb bloc--
			jz	aplus
			jmp	short okjump

aplus:			; ligne finie

			pop	edi
			pop	esi

			movzx	eax, bl

			add	edi, TabOffLine[1*4]
			add	ebp, eax

			add	esi, TabOffLine[1*4]

			dec	dl			; nb lig--
			jnz	l1

EndMask:		ret

CopyMask		endp
  */
  // TODO: revisit and clean this up when characters are rendered
	// This function is supposed (I think) to make items in indoor levels to
	// be drawn on top of the character.
	int v1; // ebx@1
  int v2; // ecx@1
  Struc_Mask_Header *v3; // esi@1
  U8 *v4; // ebp@1
  int v5; // eax@1
  int v6; // edx@1
  int v7; // ebx@1
  int v8; // ecx@1
  U8 *v9; // esi@1
  int v10; // edx@1
  int v11; // eax@1
  int v12; // edx@5
  int v13; // ebx@5
  U8 *v14; // edi@5
  U8 *v15; // ebp@5
  char v16; // bh@5
  int v17; // edx@5
  int v18; // ecx@5
  char v19; // bl@6
  char v21; // al@9
  U8 v22; // of@9
  int v23; // ecx@20
  int v24; // edi@20
  int v25; // eax@20
  int v26; // edi@27
  U8 *v27; // edi@29
  U8 *v28; // esi@29
  U8 *v29; // ebp@29
  int v30; // edi@29
  U8 *v31; // esi@29
  U8 *v32; // edi@29
  int v33; // edx@29
  U8 v34; // bl@30
  U8 *v35; // ebp@30
  U8 v36; // bh@30
  unsigned int v37; // ecx@30
  U8 v38; // ah@30
  U8 *v39; // edi@30
  U8 *v40; // esi@30
  U8 v41; // al@31
  bool v42; // cf@31
  U8 v43; // ah@31
  U8 v44; // al@34
  bool v45; // cf@34
  U8 *v47; // [sp-1Ch] [bp-20h]@8
  U8 *v48; // [sp-1Ch] [bp-20h]@30
  int v49; // [sp-18h] [bp-1Ch]@8
  U8 *v50; // [sp-18h] [bp-1Ch]@30

	U8 something1 = 0;
	U8 something2 = 0;
	U32 screenWidth = TabOffLine[1];

  v1 = x;
  v2 = y;
  v3 = (Struc_Mask_Header *)(bankmask + ((U32 *)bankmask)[nummask]);
  v4 = (U8 *)src;
  v5 = v3->HotX;
  v6 = v3->HotY;
  v7 = v5 + v1;
  v8 = v6 + v2;
  v6 = v3->DeltaX;
  v5 = v3->DeltaY;
  v9 = (U8 *)v3 + sizeof(Struc_Mask_Header); // Skip header

	// Test Clipping
  v10 = v6 + v7 - 1;
  v11 = v5 + v8 - 1;
  if ( v7 < ClipXMin || v8 < ClipYMin || v10 > ClipXMax || v11 > ClipYMax )
  {
		// ClippingMask:
    if ( v7 > ClipXMax || v8 > ClipYMax || v10 < ClipXMin || v11 < ClipYMin )
    {
			return;
		}

		if ( v8 < ClipYMin )
		{
			// Clipping top
			v23 = ClipYMin - v8;
			v24 = v11;
			v25 = 0;
			do
			{
				// NextH:
				v25 = *v9;
				v9 += v25 + 1;
				--v23;
			}
			while ( v23 );
			v8 = ClipYMin;
			v11 = v24;
		}
		// Clipping bottom
		// PasHaut:
		if ( v11 > ClipYMax )
		{
			v11 = ClipYMax;
		}
		// Clipping left
		// PasBas:
		S32 OffsetBegin = 0;
		if ( v7 < ClipXMin )
		{
			OffsetBegin = ClipXMin - v7;
		}
		// Clipping right
		// PasGauche:
		v26 = v10 - v7 - OffsetBegin + 1;
		if ( v10 > ClipXMax )
		{
			v26 -= v10 - ClipXMax;
		}
		// PasDroit:
		U32 NbPix = v26;
		v27 = v9;
		v28 = v4;
		v29 = v27;
		v30 = v7 + TabOffLine[v8];
		v31 = v30 + v28;
		v32 = (U8 *)Log + v30;
		v33 = v11 - v8 + 1;
		do
		{
			// l1:
			v50 = v31;
			v48 = v32;
			v34 = *v29; // nb blocks
			v35 = v29 + 1;
			v36 = NbPix; // nb points to copy
			v37 = OffsetBegin; // start x
			v38 = OffsetBegin; // nb pix count
			v39 = v32 + OffsetBegin; // new pos screen
			v40 = v31 + OffsetBegin;
			while ( 1 )
			{
				// l0:
				v41 = *v35; // nb pix to jump
				v35++;
				--v34;
				v42 = v38 < v41;
				v43 = v38 - v41;
				if ( v43 == 0 )
				{
					// falls flat
					goto LABEL_OKDRAW;
				}

				if (v42)
				{
					// rptj:
					v43 = v43;
					v37 = v43; // nb pix to jump
					something1 = v36 <= v43;
					v36 -= v43; // remains to write
					if (something1)
					{
						v39 += v37;
						v40 += v37;
						goto LABEL_OKDRAW;
					}
					goto LABEL_APLUS;
				}
				if (v34 == 0)
				{
					goto LABEL_APLUS;
				}

				v44 = *v35; // nb copy point
				v35++;
				--v34;
				v45 = v43 < v44;
				v38 = v43 - v44;
				if (v38 == 0)
				{
					goto LABEL_OKJUMP;
				}
				if (v45)
				{
					break;
				}
				if (v34 == 0)
				{
					goto LABEL_APLUS;
				}
			}
			// rgtc:
			v38 = -v38; // nb pix to copy
			v37 = v38;
			something2 = v36 < v38;
			v36 -= v38;
			if (something2)
			{
				// don't copy everything
				goto LABEL_CPT;
			}
			memcpy(v39, v40, v37);
			v40 += v37;
			v39 += v37;
			v37 = 0;
			if (v36 == 0)
			{
				goto LABEL_APLUS;
			}
LABEL_OKJUMP:
			while ( 2 )
			{
				v37 = *v35; // nb pix to jump
				v35++;
				--v34;
				if (v34 == 0)
				{
					break;
				}
				v22 = v36 <= v37;
				v36 -= v37;
				if (v22)
				{
					break;
				}
				v39 += v37;
				v40 += v37;
LABEL_OKDRAW:
				v37 = *v35; // nb pix to copy
				v35++;
				--v34;
				v42 = v36 < v37;
				v36 -= v37;
				if (v42)
				{
LABEL_CPT:
					v37 += v36;
					memcpy(v39, v40, v37);
					// No need to advance v39 and v40
					break;
				}
				memcpy(v39, v40, v37);
				v40 += v37;
				v39 += v37;
				v37 = 0;
				if (v36 != 0 && v34)
				{
					continue;
				}
				break;
			}
LABEL_APLUS:
			v11 = v34;
			v32 = screenWidth + v48;
			v29 = &v35[v34];
			v31 = screenWidth + v50;
			v33--;
		}
		while (v33);
  }
  else
  {
		// Calculate Offset Screen
    v12 = v10 - v7 + 1; // (deltaX)
    v13 = TabOffLine[v8] + v7;
    v14 = (U8 *)Log + v13;
    v15 = (U8 *)v4 + v13;
    v16 = v11 - v8 + 1; // NbLine (deltaY)
    v17 = screenWidth - v12;
    v18 = 0; // counter
    v11 = 0;
    do
    {
			// NextLine:
      v19 = *v9; // Nb Block for this line
			v9++;
      do
      {
				// SameLine:
        v11 = *v9; // Nb Zero to Jump
				v9++;
        v14 += v11; // Incrust on Log
        v15 += v11; // And on PtSrc
        v19--;
        if (v19 == 0)
				{
          break;
				}
        v18 = *v9; // Nb Zero to Write
				v9++;
				// TODO: I don't know what this "cache" part is for, but I think
				// it can be safely removed
        v49 = v18;
        v47 = v14;
        do
        {
					// @@LoopLoadCache:
          v21 = *v14;
          v14 += 32;
          v18 -= 32;
        }
        while (v18 > 0);
        v14 = v47;
        v18 = v49;
				// TODO: remove the code above until this line?
        do
        {
					// loopb:
          *v14 = *v15;
					v15++;
					v14++;
          --v18;
        }
        while ( v18 );
        v19--;
      }
      while ( v19 );
      v14 += v17;
      v15 += v17;
      --v16;
    }
    while ( v16 );
  }
}
