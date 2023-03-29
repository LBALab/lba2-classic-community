/*
Struc_Plasma		STRUC

  Plasma_PTR_TabVirgule	dd	0
  Plasma_PTR_TabSpeed	dd	0
  Plasma_PTR_TabAcc	dd	0
  Plasma_PTR_TabCouls	dd	0
  Plasma_PTR_TexOff	dd	0

  Plasma_Interleave	db	0
  Plasma_Nb_PtsActifs	db	0
  Plasma_Nb_Couleurs	db	0
  Plasma_Speed		db	0

Struc_Plasma		ENDS

Struc_RealPlasma	STRUC
			db	Size Struc_Plasma dup (0)
  Plasma_TabControl	db	0
Struc_RealPlasma	ENDS

;-----------------------------------------------------------------------------
			.DATA

;-----------------------------------------------------------------------------
Nb_Pts_Inter		dd	0
Nb_Pts_Control		dd	0

*/

#include "PLASMA.H"

// typedef struct {
//   Struc_Plasma plasma;

//   U8 Plasma_TabControl;
// } Struc_RealPlasma;

U32 Nb_Pts_Inter = 0;
U32 Nb_Pts_Control = 0;

void	Do_Plasma( T_PLASMA *PtrEffectStruct )
{
  /*
Do_Plasma		PROC \
			uses eax ebx ecx edx edi esi ebp\
			PtrEffectStruct: DWORD
			mov esi, PtrEffectStruct

			push	ebp
			mov	edi, dword ptr [esi].Struc_Plasma.Plasma_PTR_TexOff

			; *** Calcul des coeff de Gouraud ***
			push	esi
			push	edi

			mov	ecx,dword ptr [esi].Struc_Plasma.Plasma_Interleave
			mov	edx,1
			shl	edx,cl
			mov	[ds:Nb_Pts_Inter],edx
			mov	ebp, edx
			dec	ebp
			mov	byte ptr [ds:Nb_Pts_Control],ch

			mov	dl,ch
			mov	esi,dword ptr [esi].Struc_Plasma.Plasma_PTR_TabVirgule
@@Lines:
			push	edx			; Chaque ligne
			push	ecx

			mov	edi,esi			; Memo le ptr de controle
			dec	ch			; si 4 pts de controle, alors 3 iterations plus le cas particulier
							; du dernier point, qui doit boucler avec le premier
@@Col:
			mov	edx,ebp
			xor	ebx,ebx

			mov	bx,[esi]
			xor	eax,eax

			mov	ax,[esi+ebp*2+2]
			add	esi,2			; Le premier point existe deja

			sub	eax,ebx

			sar	eax,cl

			add	eax,eax

			adc	eax,1

			sar	eax,1
@@Fill:
			add	ebx,eax
			dec	edx

			mov	[esi],bx
			lea	esi,[esi+2]

			jne	@@Fill

			dec	ch
			jne	@@Col

							; Traite le dernier point
			mov	edx,ebp
			xor	ebx,ebx

			mov	bx,[esi]
			xor	eax,eax

			mov	ax,[edi]
			add	esi,2			; Le premier point existe deja

			sub	eax,ebx

			sar	eax,cl

			add	eax,eax

			adc	eax,1

			sar	eax,1
@@Fill_Last:
			add	ebx,eax
			dec	edx

			mov	[esi],bx
			lea	esi,[esi+2]

			jne	@@Fill_Last


			pop	ecx
			pop	edx
			dec	dl
			jne	@@Lines

			pop	edi
			pop	esi


			; *** Copy ***
			mov	ebx,[esi].Struc_Plasma.Plasma_PTR_TabCouls
			mov	esi,dword ptr [esi].Struc_Plasma.Plasma_PTR_TabVirgule
			xor	eax,eax
			mov	dl,ch

			movzx	ebp,ch
			imul	ebp,[ds:Nb_Pts_Inter]	; EBP = Offset de la ligne suivante
@@Copy_0:
			push	edx
			push	ecx
			push	edi
			push	ebp

			cmp	dl,1
			mov	edx,ebp
			jne	@@Copy_1

			mov	eax,ebp
			imul	ebp,[ds:Nb_Pts_Control]
			sub	ebp,eax
			neg	ebp
@@Copy_1:
			xor	eax, eax
			push	ebp
			push	ecx
			push	edi

			mov	ax,[esi]
			mov	dh,byte ptr [ds:Nb_Pts_Inter]

			movzx	ebp,word ptr[esi+ebp*2]

			sub	ebp,eax

			sar	ebp,cl

			add	ebp,ebp

			adc	ebp,1
			xor	ecx,ecx

			sar	ebp,1
@@Copy_2:
			mov	cl,ah
			add	eax,ebp

			mov	cl,[ebx+ecx]

			mov	[edi],cl
			inc	edi

			dec	dh
			jne	@@Copy_2

			pop	edi
			pop	ecx
			pop	ebp

			add	edi,256
			add	esi,2
			dec	dl
			jne	@@Copy_1

			pop	ebp
			pop	edi
			pop	ecx
			pop	edx

			add	edi,[ds:Nb_Pts_Inter]
			dec	dl
			jne	@@Copy_0

			pop	ebp

			ret

Do_Plasma		ENDP
  */

	// *** Calcul des coeff de Gouraud ***
  U8 interleave = PtrEffectStruct->Interleave;
  U32 nbActivePoints = PtrEffectStruct->NbActivePoints;
  Nb_Pts_Inter = 1 << interleave;
  U32 lastPtInter = Nb_Pts_Inter - 1;
  Nb_Pts_Control = nbActivePoints;

  S16 *tabVirgule = PtrEffectStruct->TabVirgule;
  for (U32 k = 0; k < nbActivePoints; ++k)
  {
    // @@Lines:
    // Each line
    // Memo le ptr de controle
    S16 *prevTabVirgule = tabVirgule;

    // If 4 control points, then 3 iterations plus the special case of the last
    // point, which must loop with the first
    for (U32 i = 0; i < nbActivePoints - 1; ++i)
    {
      // @@Col:
      U32 current = *tabVirgule;
      U32 last = tabVirgule[lastPtInter + 1];
      ++tabVirgule;
      U32 diff = ((last - current) >> interleave);
      U8 carry = ((U64)diff << 1) & 0x100000000 != 0;
      U32 step = ((diff << 1) + 1 + carry) >> 1;
      for (U32 j = 0; j < lastPtInter; ++j)
      {
        // @@Fill:
        current += step;
        *tabVirgule = current;
        ++tabVirgule;
      }
    }

    // Treat the last point
    S16 lastPoint = *tabVirgule;
    ++tabVirgule; // The first point already exists
    U32 diff = ((*prevTabVirgule - lastPoint) >> interleave);
    U8 carry = ((U64)diff << 1) & 0x100000000 != 0;
    U32 step = ((diff << 1) + 1 + carry) >> 1;

    for (U32 j = 0; j < lastPtInter; ++j)
    {
      // @@Fill_Last:
      lastPoint += step;
      *tabVirgule = lastPoint;
      ++tabVirgule;
    }
  }

  U8 *texture = PtrEffectStruct->TexOffset;
  // *** Copy ***
  S16 *tabVirguleCopy = PtrEffectStruct->TabVirgule;

  // Next line offset
  S32 nextLineOffset = Nb_Pts_Inter * nbActivePoints;

  for (U32 k = 0; k < nbActivePoints; ++k)
  {
    // @@Copy_0:
    U8 *originalTexture = texture;
    S32 originalNextLineOffset = nextLineOffset;
    if (k == nbActivePoints - 1)
    {
      nextLineOffset = -(Nb_Pts_Control * nextLineOffset - nextLineOffset);
    }

    for (U32 y = 0; y < originalNextLineOffset; y++)
    {
      // @@Copy_1:
      U8 *baseTexture = texture;
      S16 current = *tabVirguleCopy;
      S16 next = (tabVirguleCopy[nextLineOffset] - current) >> interleave;
      U8 carry = ((U64)next << 1) & 0x100000000 != 0;
      U32 step = ((next << 1) + 1 + carry) >> 1;
      for (U32 x = 0; x < Nb_Pts_Inter; x++)
      {
        // @@Copy_2:
        U32 color = (current & 0xFF00) >> 8;
        current += step;
        *texture = PtrEffectStruct->TabColors[color];
        texture++;
      }
      texture = baseTexture + 256;
      ++tabVirguleCopy;
    }
    nextLineOffset = originalNextLineOffset;
    texture = &originalTexture[Nb_Pts_Inter];
  }
}
