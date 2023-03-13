#include        "C_EXTERN.H"

/*══════════════════════════════════════════════════════════════════════════*/
#define DEF_EFFECT              1
#define MAX_TEX_EFFECTS 	10

/*══════════════════════════════════════════════════════════════════════════*/
typedef struct
	{
	        S32     Type ;
	        void    *PtrStruct ;
        }       T_TEX_EFFECT ;

S32     	NbTexEffects = 0 ;
static	T_TEX_EFFECT    TexEffectList[MAX_TEX_EFFECTS] ;
static	S32     	LastAnimationTimer ;

/*══════════════════════════════════════════════════════════════════════════*/
// plasma struct

typedef struct  {

        S16     *TabVirgule ;
        S16     *TabSpeed   ;
        S32     *TabAcc     ;
        U8      *TabColors  ;

        U8      *TexOffset  ;           // devenu ptr sur effet dans tex

        U8      Interleave ;
        U8      NbActivePoints ;
        U8      NbColors ;
        U8      Speed ;

        U8      data_start ;

        }       T_PLASMA ;

/*══════════════════════════════════════════════════════════════════════════*/

void    ScanTextureAnimation( U8 *ptrtex, S32 nbscanlines )
{
	S32	x ;

	nbscanlines*=256	;
        for(x=0; x<nbscanlines; x++, ptrtex++ )
        {
                if( ptrtex[0] == DEF_EFFECT )
                {
		        U8      *ptr	;
		        S32     width	;
		        S32     effectnum;
			S32	x1, x2	;
			S32	n	;

                        // effet trouvé
                        effectnum = ptrtex[1+2*256] ;

                        TexEffectList[NbTexEffects].Type = effectnum ;

                        // width scan
                        ptr = &ptrtex[1+256]	;
                        x1 = x2 = x&0xFF	;
                        while( *ptr++ == 0 )
                        {
                                x2++ ;
                                if( x2 == 256 )  break ;
                        }
                        width = 2 + x2 - x1 ;

                        if(width & (width-1))
                        {
                                // erreur taille texture animee pas exposant de 2
                                return ;
                        }


                        switch( effectnum )
                        {
//------------------------------------------------------------------
                        case 0: // plasma
                        {
			        T_PLASMA	*PtrPlasma ;
			        U8      	*ptrcol ;
			        U8      	*ptrc ; // tabcouls
			        S16     	*ptrv ; // tabvirgule
			        S16     	*ptrs ; // tabspeed
			        S32     	*ptra ; // tabacc
			        S32     	speed ;
			        S32     	nbactivepoints ;
			        S32     	interleave ;
			        S32     	delta ;
			        S32     	nbcols ;
			        S32		os, ov ;

	                        // lecture des parametres

	                        // nb colors
	                        nbcols = 0 ;
	                        ptrcol = &ptrtex[1+3*256] ;
	                        while( ptrcol[nbcols] != DEF_EFFECT )	nbcols++ ;

	                        nbactivepoints = ptrtex[2+2*256] ;
	                        if( !nbactivepoints )   nbactivepoints = 4 ;

	                        speed = ptrtex[3+2*256] ;
	                        if( !speed )    speed = 5 ;

	                        delta = width/nbactivepoints ;
	                        interleave = 1 ;
	                        while( !((delta>>=1)&1)  )  interleave++ ;
	                        delta = 1<<interleave	;

	                        // init structures
	                        PtrPlasma = (T_PLASMA*)Malloc( sizeof( T_PLASMA )
	                        			       + (nbactivepoints*nbactivepoints*delta*2)
	                        			       + (nbactivepoints*nbactivepoints*2)
	                        			       + (nbactivepoints*nbactivepoints*4)
	                        			       + nbcols
	                        			     );



	                        if( !PtrPlasma )  return ;

	                        TexEffectList[NbTexEffects].PtrStruct = (U8*)PtrPlasma ;

	                	PtrPlasma->TexOffset = ptrtex	;
	                        PtrPlasma->Interleave = (U8)interleave ;
	                        PtrPlasma->NbActivePoints = (U8)nbactivepoints ;
	                        PtrPlasma->NbColors = (U8)nbcols ;
	                        PtrPlasma->Speed = 0 ;

	                        ptrv = (S16*) &PtrPlasma->data_start ;
	                        ptrs = ptrv + (nbactivepoints*nbactivepoints*delta) ;
	                        ptra = (S32*)( ptrs + (nbactivepoints*nbactivepoints) ) ;
	                        ptrc = (U8*)( ptra + (nbactivepoints*nbactivepoints) ) ;

	                        PtrPlasma->TabVirgule= ptrv ;
	                        PtrPlasma->TabSpeed  = ptrs ;
	                        PtrPlasma->TabAcc    = ptra ;
	                        PtrPlasma->TabColors = ptrc ;

	                        // generation des sommets aleatoires
	                        nbactivepoints *= nbactivepoints;

	                        for(os=0, ov=0; os<nbactivepoints; os++, ov+=delta)
	                        {
					S32	va ;

	                                // generation des sommets aleatoires
	                                ptrv[ov] = (S16)MyRnd( (nbcols-1)*256 ) ;
	                                // generation des vitesses aleatoires
	                                va = (S16)(speed*512 + MyRnd( speed*512 )) ;
	                                if( Rnd( 2 ) ) va = -va ;

	                                ptrs[os] = (S16)va    ;
	                                ptra[os] = 500        ;
	                        }

	                        // color table
	                        ptrcol = &ptrtex[1+3*256] ;
	                        for(os=0; os<nbcols; os++ )
	                        {
	                                ptrc[os] = ptrcol[os] ;
	                        }
			}
                        break ;
//------------------------------------------------------------------
                        case 1: // fire
			{
				U8	*PtrFire	;
			        U8      *ptrcol 	;

	                        // init structures
	                        PtrFire = (U8*)Malloc( + 4     // tex ptr
	                        		       + 32*36
	                        		       + 32*36
	                        		       + 32
	                        		     );


	                        if( !PtrFire )  return ;

	                        TexEffectList[NbTexEffects].PtrStruct = PtrFire ;

	                        // nb colors = 16
	                        ptrcol = &ptrtex[1+3*256] ;

	                        *((U8**)PtrFire) = ptrtex ;

	                        // color table
	                        for(n=0; n<16; n++ )
	                        {
	                                PtrFire[4+32*36*2+n] = 0 ;
	                        }
	                        for(n=0; n<16; n++ )
	                        {
	                                PtrFire[4+32*36*2+n+16] = *ptrcol++ ;
	                        }
			}
                        break ;
//------------------------------------------------------------------
                        }

                        // clear box effect (connue ou pas)
			ptr = ptrtex	;
                        for(n=0; n<width; n++, ptr+=256 )
                        {
                                memset( ptr, 0, width ) ;
                        }

                        NbTexEffects++ ;
                }
        }
}

/*══════════════════════════════════════════════════════════════════════════*/

// boucle pour initialiser les plasmas,fire ...

void    InitTextureAnimation( void )
{
        ManageTime() ;
        LastAnimationTimer = TimerSystemHR ;// init 1st boucle DoTextureAnimation()

        DoTextureAnimation() ;
}

/*══════════════════════════════════════════════════════════════════════════*/

void    DoTextureAnimation()
{
        S32     	n ;
        S32     	deltatimer, timer ;

	ManageTime() ;

        timer = TimerSystemHR ;
        deltatimer = timer - LastAnimationTimer ;

        if(deltatimer<20)	return	;

        LastAnimationTimer = timer ;

        for( n=0; n<NbTexEffects; n++ )
        {
                switch( TexEffectList[n].Type )
                {
//--------------------------------------------------------------------
                case 0: // plasma
                {
		        S16     	*ptrs, *ptrv ;
		        S32     	*ptra ;
		        T_PLASMA        *ptreffect ;
		        S32     	nbactivepoints, delta ;
		        S32     	nbcols ;
                        S32		ov, os ;

                        ptreffect = (T_PLASMA*)TexEffectList[n].PtrStruct ;

                        ptrv = ptreffect->TabVirgule ;
                        ptrs = ptreffect->TabSpeed ;
                        ptra = ptreffect->TabAcc ;

                        delta = 1<<ptreffect->Interleave ;
                        nbactivepoints = ptreffect->NbActivePoints ;
                        nbactivepoints *= nbactivepoints;
                        nbcols = ptreffect->NbColors<<8 ;

                        // mouvements des sommets du plasma
                        for(os=0, ov=0; os<nbactivepoints; os++, ov+=delta )
                        {
                                S32	sv	;

                                ptra[os] += ptrs[os] * deltatimer;

                                sv = GetDeltaAccMove(&ptra[os])  ;

                                if(!sv)	continue ;

                                sv += ptrv[ov] 	;

                                if( sv <= 0 )
                                {
                                        sv = 0 ;
                                        ptrs[os] = (S16)-ptrs[os];
                                }
                                else if( sv >= nbcols )
                                {
                                        sv = nbcols - 1 ;
                                        ptrs[os] = (S16)-ptrs[os];
                                }

                                ptrv[ov] = (S16)sv ;
                        }

                        Do_Plasma( (U8*)TexEffectList[n].PtrStruct ) ;
		}
		break ;
//--------------------------------------------------------------------
                case 1: // fire
                {
                	U8	*ptrb	;

                        ptrb = (U8*)TexEffectList[n].PtrStruct ;
                        Do_Fire( ptrb+4, ptrb+32*36+4, ptrb+32*36*2+4, *(U8**)ptrb ) ;
                }
		break ;
//--------------------------------------------------------------------
                }
        }
}

/*══════════════════════════════════════════════════════════════════════════*/
//
//      ATTENTION: Routines dédiées aux menus de LBA2
//
/*══════════════════════════════════════════════════════════════════════════*/
S32     PlasmaMenuIndex ;

void    InitPlasmaMenu( void )
{
        S32     n ;
        U8      *ptr ;
        U8      *texdest ;
        U8      *ptrorg ;

	if( !TexEffectList[NbTexEffects].PtrStruct )
        {
                ptrorg = (U8*)(SkySeaTexture+256*(128+64)) ;

                texdest = ptrorg ;

		memset( ptrorg, 0, 256 ) ;

		////////////////////////////////////////////////////////

                memset( texdest, 1, 64 ) ;
                texdest += 256 ;
                memset( texdest, 0, 64 ) ;
                texdest[0] = 1 ;
                texdest[63] = 1 ;
                texdest += 256 ;
                texdest[0] = 1 ;
                texdest[1] = 0 ;
                texdest[2] = 4 ;        // nb control point
                texdest[3] = 2 ;        // speed
                ptr = texdest + 256 ;
                *ptr++ = 1 ;
                for( n=10; n>=0; n-- )  *ptr++ = (U8)(n+12*16) ;
                for( n=0; n<2; n++ )    *ptr++ = 0 ;
                *ptr++ = 1 ;
/*
                memset( texdest, 1, 32 ) ;
                texdest += 256 ;
                memset( texdest, 0, 32 ) ;
                texdest[0] = 1 ;
                texdest[31] = 1 ;
                texdest += 256 ;
                texdest[0] = 1 ;
                texdest[1] = 0 ;
                texdest[2] = 4 ;        // nb control point
                texdest[3] = 1 ;        // speed
                ptr = texdest + 256 ;
                *ptr++ = 1 ;
                for( n=0; n<6; n++ )    *ptr++ = (U8)(n+12*16) ;
                *ptr++ = 1 ;
*/

                PlasmaMenuIndex = NbTexEffects ;

                ScanTextureAnimation( ptrorg, 1 ) ;
                InitTextureAnimation() ;
        }
        else
        {
                NbTexEffects = PlasmaMenuIndex+1 ;
        }
}

/*══════════════════════════════════════════════════════════════════════════*/

void    SelectPlasmaBank( U8 bank )
{
        S32     n ;
        U8      *ptr ;

        ptr = ((T_PLASMA *)TexEffectList[PlasmaMenuIndex].PtrStruct)->TabColors ;

        for( n=10; n>=0; n-- )  *ptr++ = (U8)(n+bank*16) ;
}

/*══════════════════════════════════════════════════════════════════════════*/

void    ClearPlasmaMenu( void )
{
        NbTexEffects = PlasmaMenuIndex ;
}

/*══════════════════════════════════════════════════════════════════════════*/

void    FixePlasmaPtrMap( void )
{
        PtrMap = ((T_PLASMA *)TexEffectList[PlasmaMenuIndex].PtrStruct)->TexOffset ;
}

#include <stdio.h>

void	Do_Plasma( U8 *PtrEffectStruct )
{
 // TODO: Implement ASM
 printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}

U32 ROR(U32 value, U32 shift)
{
    return (value >> shift) | (value << (32 - shift));
}

U32 ROL(U32 value, U32 shift)
{
    return (value << shift) | (value >> (32 - shift));
}

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
  __int16 v11; // ax@5
  __int16 v12; // ax@5
  int v13; // ebx@5
  __int16 v14; // ax@5
  __int16 v15; // ax@5
  __int16 v16; // ax@5
  int v17; // ebx@5
  __int16 v18; // ax@5
  U16 v19; // ax@5
  int v20; // ebx@5
  __int16 v21; // ax@5
  unsigned __int8 *v22; // esi@10
  U8 *v23; // edi@10
  int v24; // ebx@10
  int v25; // ecx@10
  int v26; // eax@10
  signed int v27; // ecx@11
  char result; // al@14
  int v29; // [sp-4h] [bp-14h]@4
  int v30; // [sp-4h] [bp-14h]@11

  // genere 2 lignes aleatoires

  v1 = ptrwork1 + 1088;
  v2 = 64;
  do
  {
    v3 = ROL(FireSeed, (FireSeed >> 8) & 0xFF);
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

      v21 = ROR(v6 + v19, 3);

      if ( !(HIBYTE(v21) & 0x60) && (_BYTE)v21 )
        LOBYTE(v21) = v21 - 1;
      *v5++ = v21;
      v9 = v20 & 0x1F;
      --v8;
    }
    while ( v8 );
    v4 += 32;
    v7 = v29 - 1;
  }
  while ( v29 != 1 );
  qmemcpy(*(void **)(a1 + 8), (const void *)(*(_DWORD *)(a1 + 12) + 32), 0x440u);
  v22 = *(unsigned __int8 **)(a1 + 8);
  v23 = *(_BYTE **)(a1 + 20);
  v24 = *(_DWORD *)(a1 + 16);
  v25 = 32;
  v26 = 0;
  do
  {
    v30 = v25;
    v27 = 32;
    do
    {
      LOBYTE(v26) = *v22 >> 1;
      if ( (unsigned __int8)v26 > 0x1Fu )
        LOBYTE(v26) = 31;
      result = *(_BYTE *)(v24 + v26);
      *v23 = result;
      ++v22;
      ++v23;
      --v27;
    }
    while ( v27 );
    v23 += 224;
    v25 = v30 - 1;
  }
  while ( v30 != 1 );
  return result;
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}
