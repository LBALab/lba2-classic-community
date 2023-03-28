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
  T_PLASMA *v1; // esi@1
  int v2; // ecx@1
  int v3; // ebp@1
  U32 v4; // edx@1
  S16 *v5; // esi@1
  S16 *v6; // edi@2
  char v7; // ch@2
  int v8; // edx@3
  int v9; // ebx@3
  U16 v10; // ax@3
  int v11; // eax@3
  int v12; // edx@6
  int v13; // ebx@6
  int v14; // eax@6
  U8 *v15; // edi@9
  U8 *v16; // ebx@9
  S16 *v17; // esi@9
  int v18; // ebp@9
  bool v19; // zf@10
  U32 v21; // eax@12
  int v23; // ebp@12
  int v24; // ecx@12
  int v25; // ebp@12
  U8 *v27; // [sp-34h] [bp-3Ch]@12
  int v28; // [sp-30h] [bp-38h]@12
  int v29; // [sp-2Ch] [bp-34h]@12
  int v30; // [sp-28h] [bp-30h]@2
  int v31; // [sp-28h] [bp-30h]@10
  U8 *v33; // [sp-24h] [bp-2Ch]@10
  U8 *v34; // [sp-20h] [bp-28h]@1
  int v35; // [sp-20h] [bp-28h]@10
  int v38; // [sp+4h] [bp-4h]@0

  v1 = PtrEffectStruct;
	// *** Calcul des coeff de Gouraud ***
  v34 = PtrEffectStruct->TexOffset;
  v2 = PtrEffectStruct->Interleave;
  U32 v2_b1 = PtrEffectStruct->NbActivePoints;
  v4 = 1 << v2;
  Nb_Pts_Inter = v4;
  v3 = v4 - 1;
  Nb_Pts_Control = v2_b1;
  v4 = v2_b1;
  v5 = PtrEffectStruct->TabVirgule;
  do
  {
    // Each line
    v30 = v2;
    // Memo le ptr de controle
    v6 = v5;
    // If 4 control points, then 3 iterations plus the special case of the last
    // point, which must loop with the first
    v7 = v2_b1 - 1;
    do
    {
      v8 = v3;
      v9 = *v5;
      v10 = v5[v3 + 1];
      ++v5;
      U32 diff = ((v10 - v9) >> v2);
      U8 carry = ((U64)diff << 1) & 0x100000000 != 0;
      v11 = ((diff << 1) + 1 + carry) >> 1;
      do
      {
        v9 += v11;
        --v8;
        *v5 = v9;
        ++v5;
      }
      while (v8);
      --v7;
    }
    while (v7);
    // Treat the last point
    v12 = v3;
    v13 = *v5;
    ++v5; // The first point already exists
    U32 diff = ((*v6 - v13) >> v2);
    U8 carry = ((U64)diff << 1) & 0x100000000 != 0;
    v14 = ((diff << 1) + 1 + carry) >> 1;
    do
    {
      v13 += v14;
      --v12;
      *v5 = v13;
      ++v5;
    }
    while ( v12 );
    v2 = v30;
    v4--;
  }
  while (v4);
  v15 = v34;
  // *** Copy ***
  v16 = PtrEffectStruct->TabColors; // *(_DWORD *)(v36 + 12);
  v17 = PtrEffectStruct->TabVirgule; // *(_WORD **)v36;
  v4 = v2_b1;
  // v18: Next line offset
  v18 = Nb_Pts_Inter * v2_b1;
  do
  {
    v35 = v2;
    v33 = v15;
    v31 = v18;
    v19 = (v4 == 1);
    if (v19)
    {
      v18 = -(Nb_Pts_Control * v18 - v18);
    }

    for (U32 y = 0; y < v31; y++)
    {
      v29 = v18;
      v28 = v2;
      v27 = v15;
      v21 = *v17;
      v23 = (v17[v18] - v21) >> v2;
      v24 = 0;
      U8 carry = ((U64)v23 << 1) & 0x100000000 != 0;
      v25 = ((v23 << 1) + 1 + carry) >> 1;
      for (U32 x = 0; x < Nb_Pts_Inter; x++)
      {
        v24 = (v24 & 0xFFFFFF00) + ((v21 & 0xFF00) >> 8);
        v21 += v25;
        *v15 = v16[v24];
        v15++;
      }
      v2 = v28;
      v18 = v29;
      v15 = v27 + 256;
      ++v17;
    }
    v18 = v31;
    v2 = v35;
    v15 = &v33[Nb_Pts_Inter];
    v4--;
  }
  while (v4);
}
