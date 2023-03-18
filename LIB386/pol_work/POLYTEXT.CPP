/*
; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                           (Adeline Software Intl.)                 ***
; ***                           1995-96-97                               ***
; ***                                                                    ***
; **************************************************************************
; **************************************************************************
; **************************************************************************
*/

#include <POLYGON/POLYTEXT.H>

#include <stdio.h>

/*
			OPTION	PROC:PRIVATE
			OPTION	SCOPED
			OPTION	LANGUAGE:C



;			**************
;			*** PUBLIC ***
;			**************

;					******************
;					*** PROCEDURES ***
;					******************
PUBLIC	C		Filler_Texture
PUBLIC	C		Filler_TextureFlat
PUBLIC	C		Filler_TextureChromaKey
PUBLIC	C		Filler_TextureFlatChromaKey
PUBLIC	C		Filler_TextureFog
PUBLIC	C		Filler_TextureChromaKeyFog
PUBLIC	C		Filler_TextureZBuf
PUBLIC	C		Filler_TextureChromaKeyZBuf
PUBLIC	C		Filler_TextureFlatZBuf
PUBLIC	C		Filler_TextureFlatChromaKeyZBuf
PUBLIC	C		Filler_TextureFogZBuf
PUBLIC	C		Filler_TextureChromaKeyFogZBuf
PUBLIC	C		Filler_TextureNZW
PUBLIC	C		Filler_TextureChromaKeyNZW
PUBLIC	C		Filler_TextureFlatNZW
PUBLIC	C		Filler_TextureFlatChromaKeyNZW
PUBLIC	C		Filler_TextureFogNZW
PUBLIC	C		Filler_TextureChromaKeyFogNZW

;					*****************
;					*** VARIABLES ***
;					*****************

;			***************
;			*** INCLUDE ***
;			***************

INCLUDE			FILLER.INC


;			**************
;			*** MACROS ***
;			**************


;			***************
;			*** EQUATES ***
;			***************




;			**************************
;			*** SEGMENTATION MODEL ***
;			**************************

			.386
;			.MODEL	FLAT, C



;			************
;			*** DATA ***
;			************
;			.DATA
_DATA			SEGMENT	USE32 PUBLIC PARA 'DATA'

;				******************
;				*** Extrn data ***
;				******************
Extrn	C		Fill_Restart		:	DWORD
Extrn	C		Fill_Patch		:	DWORD
Extrn	C		Fill_Color		:	DWORD
Extrn	C		Fill_Logical_Palette	:	BYTE

Extrn	C		PtrMap			:	DWORD
Extrn	C		RepMask			:	DWORD
Extrn	C		PtrCLUTGouraud		:	DWORD
Extrn	C		PtrZBuffer		:	DWORD

Extrn	C		Fill_LeftSlope		:	DWORD
Extrn	C		Fill_RightSlope		:	DWORD

Extrn	C		Fill_CurY		:	DWORD
Extrn	C		Fill_CurXMin		:	DWORD
Extrn	C		Fill_CurXMax		:	DWORD

Extrn	C		PTR_TabOffLine		:	DWORD
Extrn	C		Fill_CurOffLine		:	DWORD
Extrn	C		Log			:	DWORD
Extrn	C		ScreenPitch		:	DWORD

Extrn	C		Fill_CurMapUMin		:	DWORD
Extrn	C		Fill_MapU_LeftSlope	:	DWORD
Extrn	C		Fill_MapU_XSlope	:	DWORD

Extrn	C		Fill_CurMapVMin		:	DWORD
Extrn	C		Fill_MapV_LeftSlope	:	DWORD
Extrn	C		Fill_MapV_XSlope	:	DWORD

Extrn	C		Fill_CurZBufMin		:	DWORD
Extrn	C		Fill_ZBuf_LeftSlope	:	DWORD
Extrn	C		Fill_ZBuf_XSlope	:	DWORD




;				*******************
;				*** Global data ***
;				*******************

;					*******************
;					*** PUBLIC data ***
;					*******************



;					*******************
;					*** MODULE data ***
;					*******************
			ALIGN	4







;				******************
;				*** Local data ***
;				******************
			ALIGN	4
Precalc_Inc		dd	0		; Precalc value

; Runtime code patch refactor, uses memory instead of instruction substitution
PtrZBuffer1         DWORD 12345678h
PtrZBufferPatch     DWORD 12345678h
PtrMapPatch         DWORD 12345678h
PtrCLUT             DWORD 12345678h
PtrLog              DWORD 12345678h
RepMaskPatch        DWORD 12345678h
DecMapUIntZBuf      DWORD 12345678h
DecMapV             DWORD 12345678h
DecMapU             DWORD 12345678h
DecZBuf             DWORD 12345678h
IntZBuf             DWORD 12345678h
IntMapV             BYTE  12h
IntMapU             BYTE  12h

_DATA			ENDS


;			************
;			*** CODE ***
;			************
;			.CODE
_TEXT			SEGMENT	USE32 PARA PUBLIC 'CODE'
			ASSUME	CS:FLAT, DS:FLAT, ES:FLAT, SS:FLAT


;				******************
;				*** Extrn proc ***
;				******************
Extrn	C		Triangle_ReadNextEdge	:	PROC	; Entry point that reads the next point in the triangle


;				*******************
;				*** Global proc ***
;				*******************

*/


S32 Filler_Texture(U32 nbPoints, U32 fillCurXMin, U32 fillCurXMax)
// void __usercall Filler_Texture(unsigned int a1@<edx>, int a2@<ecx>, unsigned int a3@<ebx>)
{
/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_Texture		PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			push	ebx

			mov	ebx,[Fill_MapV_XSlope]
			mov	[DecMapU],eax

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	eax,0FFh

			or	eax,ebx
			pop	ebx

			mov	[Precalc_Inc],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	eax,ebp
			mov	edx,esi

			shr	eax,8
			and	edx,0FF0000h

			shr	edx,16
			and	eax,0FF00h

			shl	ebp,16		; EBP = DecMapV
			or	eax,edx

			mov	edx,esi
			mov	esi,[RepMask]

			and	eax,esi
			mov	esi,[PtrMap]

			shl	edx,16		; EDX = DecMapU
			mov	ebx,[Precalc_Inc]
@@Each_X:
			mov	dl,[eax+esi]
		add	ebp,[DecMapV]

			adc	ah,bh
		add	edx,[DecMapU]

			adc	al,bl
			mov	[edi],dl

			inc	edi
		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X
@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_Texture		ENDP
	*/
  int v3; // ecx@1
  int v4; // edi@1
  int v5; // esi@1
  unsigned int v6; // ebp@1
  unsigned int v7; // ecx@4
  int v8; // edi@5
  char v9; // dl@5
  int v10; // ebx@5
  char *v11; // edi@6
  unsigned int v12; // edx@6
  char v13; // al@7
  int v14; // eax@9
  unsigned int v15; // ebp@9
  int v16; // edx@9
  unsigned int v17; // eax@9
  int v18; // esi@9
  int v19; // edx@9
  __int16 v20; // bx@9
  unsigned __int8 v21; // cf@10
  int v22; // [sp-1Ch] [bp-1Ch]@6
  unsigned int v23; // [sp-18h] [bp-18h]@5
  unsigned int v24; // [sp-14h] [bp-14h]@5
  int v25; // [sp-10h] [bp-10h]@5
  unsigned int v26; // [sp-Ch] [bp-Ch]@5
  int v27; // [sp-8h] [bp-8h]@4
  int v28; // [sp-4h] [bp-4h]@4

  v3 = a2 + 1;
  v4 = Fill_CurOffLine;
  Fill_CurY += v3;
  v5 = Fill_CurMapUMin;
  v6 = Fill_CurMapVMin;
  if ( Fill_Patch != 1 )
    goto LABEL_3;
  --Fill_Patch;
  DecMapU_0 = Fill_MapU_XSlope << 16;
  DecMapV = Fill_MapV_XSlope << 16;
  RepMaskPatch_0 = RepMask;
  Precalc_Inc = ((Fill_MapV_XSlope & 0xFF0000u) >> 8) | ((unsigned int)Fill_MapU_XSlope >> 16) & 0xFF;
  while ( 1 )
  {
    v28 = v3;
    v27 = v4;
    v7 = (a1 >> 16) - (a3 >> 16);
    if ( (signed int)(a1 >> 16) > (signed int)(a3 >> 16) )
    {
      v8 = (a3 >> 16) + v4;
      v26 = v6;
      v25 = v5;
      v24 = a3;
      v23 = a1;
      v9 = *(_BYTE *)v8;
      v10 = (unsigned __int16)a3 ^ 0xFFFF;
      if ( v7 > 0x10 )
      {
        v5 += v10 * (signed __int64)Fill_MapU_XSlope >> 16;
        v22 = v8;
        v11 = (char *)(v8 & 0xFFFFFFE0);
        v6 += v10 * (signed __int64)Fill_MapV_XSlope >> 16;
        v12 = (unsigned int)&v11[v7] & 0xFFFFFFE0;
        do
        {
          v13 = *v11;
          v11 += 32;
        }
        while ( (unsigned int)v11 <= v12 );
        v8 = v22;
      }
      v14 = (v6 >> 8) & 0xFF00;
      v15 = v6 << 16;
      v16 = v5;
      v17 = RepMask & (((v5 & 0xFF0000u) >> 16) | v14);
      v18 = PtrMap;
      v19 = v16 << 16;
      v20 = Precalc_Inc;
      do
      {
        LOBYTE(v19) = *(_BYTE *)(v17 + v18);
        v21 = __CFADD__(DecMapV, v15);
        v15 += DecMapV;
        BYTE1(v17) += HIBYTE(v20) + v21;
        v21 = __CFADD__(DecMapU_0, v19);
        v19 += DecMapU_0;
        LOBYTE(v17) = v20 + v21 + v17;
        *(_BYTE *)v8++ = v19;
        v17 &= RepMaskPatch_0;
        --v7;
      }
      while ( v7 );
      a1 = v23;
      a3 = v24;
      v5 = v25;
      v6 = v26;
    }
    v4 = v27;
    v3 = v28 - 1;
    if ( v28 == 1 )
      break;
LABEL_3:
    v4 += ScreenPitch;
    a3 += Fill_LeftSlope;
    a1 += Fill_RightSlope;
    v5 += Fill_MapU_LeftSlope;
    v6 += Fill_MapV_LeftSlope;
  }
  Fill_CurXMin = a3;
  Fill_CurXMax = a1;
  Fill_CurOffLine = v27;
  Fill_CurMapUMin = v5;
  Fill_CurMapVMin = v6;
  return Triangle_ReadNextEdge();
}


/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureChromaKey	PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			push	ebx

			mov	ebx,[Fill_MapV_XSlope]
			mov	[DecMapU],eax

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	eax,0FFh

			or	eax,ebx
			pop	ebx

			mov	[Precalc_Inc],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	eax,ebp
			mov	edx,esi

			shr	eax,8
			and	edx,0FF0000h

			shr	edx,16
			and	eax,0FF00h

			shl	ebp,16		; EBP = DecMapV
			or	eax,edx

			mov	edx,esi
			mov	esi,[RepMask]

			and	eax,esi
			mov	esi,[PtrMap]

			shl	edx,16		; EDX = DecMapU
			mov	ebx,[Precalc_Inc]
@@Each_X:
			mov	dl,[eax+esi]
		add	ebp,[DecMapV]

			adc	ah,bh
		add	edx,[DecMapU]

			adc	al,bl
			inc	edi

			test	dl,dl
			je	@@NoDisp

			mov	[edi-1],dl
@@NoDisp:
		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureChromaKey	ENDP









; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureFlat	PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[PtrCLUTGouraud]
			mov	ebx,[PtrMap]

			mov	[PtrCLUT],eax
			mov	[PtrMapPatch],ebx

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shl	eax,16

			mov	[DecMapU],eax

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	eax,0FFh

			or	eax,ebx
			pop	ebx

			mov	[Precalc_Inc],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	eax,ebp
			mov	edx,esi

			shr	eax,8
			and	edx,0FF0000h

			shr	edx,16
			and	eax,0FF00h

			shl	ebp,16		; EBP = DecMapV
			or	eax,edx

			shl	esi,16		; ESI = DecMapU
			mov	edx,[RepMask]

			and	eax,edx
			mov	ebx,[Precalc_Inc]

			mov	edx,[Fill_Color]
@@Each_X:
		push ebx
		mov ebx,[PtrMapPatch]
		mov	dl,[eax+ebx]
		pop ebx

		add	ebp,[DecMapV]

			adc	ah,bh
		add	esi,[DecMapU]

			adc	al,bl

		push eax
		mov eax,[PtrCLUT]
		mov	dl,[edx+eax]
		pop eax

			mov	[edi],dl
			inc	edi

		and	eax,[RepMaskPatch]
			dec	ecx

			jne	@@Each_X

@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureFlat	ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureFlatChromaKey PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[PtrCLUTGouraud]
			mov	ebx,[PtrMap]

			mov	[PtrCLUT],eax
			mov	[PtrMapPatch],ebx

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shl	eax,16

			mov	[DecMapU],eax

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	eax,0FFh

			or	eax,ebx
			pop	ebx

			mov	[Precalc_Inc],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	eax,ebp
			mov	edx,esi

			shr	eax,8
			and	edx,0FF0000h

			shr	edx,16
			and	eax,0FF00h

			shl	ebp,16		; EBP = DecMapV
			or	eax,edx

			shl	esi,16		; ESI = DecMapU
			mov	edx,[RepMask]

			and	eax,edx
			mov	ebx,[Precalc_Inc]

			mov	edx,[Fill_Color]
@@Each_X:
		push ebx
		mov ebx,[PtrMapPatch]
		mov	dl,[eax+ebx]
		pop ebx

		add	ebp,[DecMapV]

			adc	ah,bh
		add	esi,[DecMapU]

			adc	al,bl
			inc	edi

			test	dl,dl
			je	@@NoDisp

		push eax
		mov eax,[PtrCLUT]
		mov	dl,[edx+eax]
		pop eax

			mov	[edi-1],dl
@@NoDisp:
		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X

@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureFlatChromaKey ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureFog	PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	ebx,[PtrMap]
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			mov	ebx,[Fill_MapV_XSlope]
			mov	[DecMapU],eax

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	eax,0FFh

			or	eax,ebx
			pop	ebx

			mov	[Precalc_Inc],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	eax,ebp
			mov	edx,esi

			shr	eax,8
			and	edx,0FF0000h

			shr	edx,16
			and	eax,0FF00h

			shl	ebp,16		; EBP = DecMapV
			or	eax,edx

			shl	esi,16		; ESI = DecMapU
			mov	edx,[RepMask]

			and	eax,edx
			mov	ebx,[Precalc_Inc]

			xor	edx,edx
@@Each_X:
		push ebx
		mov ebx,[PtrMapPatch]
		mov	dl,[eax+ebx]
		pop ebx

		add	ebp,[DecMapV]

			adc	ah,bh
		add	esi,[DecMapU]

			adc	al,bl
			mov	dl,[edx+Fill_Logical_Palette]

			mov	[edi],dl
			inc	edi

		and	eax,[RepMaskPatch]
			dec	ecx

			jne	@@Each_X

@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureFog	ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureChromaKeyFog PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	ebx,[PtrMap]
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			mov	ebx,[Fill_MapV_XSlope]
			mov	[DecMapU],eax

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	eax,0FFh

			or	eax,ebx
			pop	ebx

			mov	[Precalc_Inc],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	eax,ebp
			mov	edx,esi

			shr	eax,8
			and	edx,0FF0000h

			shr	edx,16
			and	eax,0FF00h

			shl	ebp,16		; EBP = DecMapV
			or	eax,edx

			shl	esi,16		; ESI = DecMapU
			mov	edx,[RepMask]

			and	eax,edx
			mov	ebx,[Precalc_Inc]

			xor	edx,edx
@@Each_X:
		push ebx
		mov ebx,[PtrMapPatch]
		mov	dl,[eax+ebx]
		pop ebx

		add	ebp,[DecMapV]

			adc	ah,bh
		add	esi,[DecMapU]

			adc	al,bl
			inc	edi

			test	dl,dl
			je	@@NoDisp

			mov	dl,[edx+Fill_Logical_Palette]

			mov	[edi-1],dl
@@NoDisp:
		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X

@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureChromaKeyFog ENDP




; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZBuf	PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	ebx,[PtrMap]
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			mov	[DecMapU],eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	eax,0FFh

			or	eax,ebx
			mov	ebx,[PtrZBuffer]

			mov	[Precalc_Inc],eax
			mov	[IntMapU],al

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	[PtrZBuffer1],ebx

			mov	ebx,[Fill_ZBuf_XSlope]
			mov	[DecZBuf],eax

			shr	ebx,8
			mov	eax,[Log]

			mov	[IntZBuf],ebx
			pop	ebx

			mov	[PtrLog],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@FastInit:

			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	eax,[Log]
			mov	ebx,[Precalc_Inc]

			shl	ebp,16
			sub	edi,eax

			or	ebx,esi
			mov	eax,[RepMask]

			and	eax,edx
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push eax
		mov eax,[PtrZBuffer1]
		mov	word ptr [edi*2+eax],dx
		pop eax

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
			inc	edi
		add	ebx,[DecMapU]

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

			adc	ah,bh
		add	esi,[DecZBuf]

		adc	edx,[IntZBuf]
		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureZBuf	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureChromaKeyZBuf PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	ebx,[PtrMap]
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			mov	[DecMapU],eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	eax,0FFh

			or	eax,ebx
			mov	ebx,[PtrZBuffer]

			mov	[Precalc_Inc],eax
			mov	[IntMapU],al

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	[PtrZBuffer1],ebx

			mov	ebx,[Fill_ZBuf_XSlope]
			mov	[DecZBuf],eax

			shr	ebx,8
			mov	eax,[Log]

			mov	[IntZBuf],ebx
			pop	ebx

			mov	[PtrLog],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@FastInit:

			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	eax,[Log]
			mov	ebx,[Precalc_Inc]

			shl	ebp,16
			sub	edi,eax

			or	ebx,esi
			mov	eax,[RepMask]

			and	eax,edx
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push eax
		mov eax,[PtrZBuffer1]
		mov	word ptr [edi*2+eax],dx
		pop eax

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

			test	bl,bl
			je	@@NoDisp

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
			inc	edi
		add	ebx,[DecMapU]

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

			adc	ah,bh
		add	esi,[DecZBuf]

		adc	edx,[IntZBuf]
		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureChromaKeyZBuf ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureFlatZBuf	PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[Fill_Color]
			mov	ebx,[PtrMap]

			mov	[PtrCLUT],eax
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			push	eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16

			shr	ebx,16
			mov	[IntMapU],al

			mov	[IntMapV],bl
			mov	ebx,[PtrZBuffer]

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	[PtrZBuffer1],ebx

			mov	ebx,[Fill_ZBuf_XSlope]
			mov	[DecZBuf],eax

			shr	ebx,8
			mov	eax,[Log]

			and	ebx,0FFFFh
			mov	[PtrLog],eax

			pop	eax

			or	eax,ebx
			pop	ebx

			mov	[DecMapUIntZBuf],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	ebx,[Log]
			mov	eax,[RepMask]

			sub	edi,ebx
			and	eax,edx

			shl	ebp,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			xor	ebx,ebx

			or	edx,esi
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8



@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

		push eax
		mov eax,[PtrZBuffer1]
		mov	word ptr [edi*2+eax],dx
		pop eax

		push eax
		mov eax,[PtrCLUT]
		mov	bl,[ebx+eax]
		pop eax

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
		add	esi,[DecZBuf]

	adc	edx,[DecMapUIntZBuf]
			inc	edi

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

		adc	ah,[IntMapV]

		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureFlatZBuf	ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureFlatChromaKeyZBuf PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[Fill_Color]
			mov	ebx,[PtrMap]

			mov	[PtrCLUT],eax
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			push	eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16

			shr	ebx,16
			mov	[IntMapU],al

			mov	[IntMapV],bl
			mov	ebx,[PtrZBuffer]

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	[PtrZBuffer1],ebx

			mov	ebx,[Fill_ZBuf_XSlope]
			mov	[DecZBuf],eax

			shr	ebx,8
			mov	eax,[Log]

			and	ebx,0FFFFh
			mov	[PtrLog],eax

			pop	eax

			or	eax,ebx
			pop	ebx

			mov	[DecMapUIntZBuf],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	ebx,[Log]
			mov	eax,[RepMask]

			sub	edi,ebx
			and	eax,edx

			shl	ebp,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			xor	ebx,ebx

			or	edx,esi
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8



@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

			test	bl,bl
			je	@@NoDisp

		push eax
		mov eax,[PtrZBuffer1]
		mov	word ptr [edi*2+eax],dx
		pop eax

		push eax
		mov eax,[PtrCLUT]
		mov	bl,[ebx+eax]
		pop eax

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
		add	esi,[DecZBuf]

	adc	edx,[DecMapUIntZBuf]
			inc	edi

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

		adc	ah,[IntMapV]

		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureFlatChromaKeyZBuf ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureFogZBuf	PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	ebx,[PtrMap]
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			push	eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16

			shr	ebx,16
			mov	[IntMapU],al

			mov	[IntMapV],bl
			mov	ebx,[PtrZBuffer]

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	[PtrZBuffer1],ebx

			mov	ebx,[Fill_ZBuf_XSlope]
			mov	[DecZBuf],eax

			shr	ebx,8
			mov	eax,[Log]

			and	ebx,0FFFFh
			mov	[PtrLog],eax

			pop	eax

			or	eax,ebx
			pop	ebx

			mov	[DecMapUIntZBuf],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	ebx,[Log]
			mov	eax,[RepMask]

			sub	edi,ebx
			and	eax,edx

			shl	ebp,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			xor	ebx,ebx

			or	edx,esi
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8



@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

		push eax
		mov eax,[PtrZBuffer1]
		mov	word ptr [edi*2+eax],dx
		pop eax

			mov	bl,[ebx+Fill_Logical_Palette]

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
		add	esi,[DecZBuf]

	adc	edx,[DecMapUIntZBuf]
			inc	edi

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

		adc	ah,[IntMapV]

		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureFogZBuf	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureChromaKeyFogZBuf PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	ebx,[PtrMap]
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			push	eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16

			shr	ebx,16
			mov	[IntMapU],al

			mov	[IntMapV],bl
			mov	ebx,[PtrZBuffer]

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	[PtrZBuffer1],ebx

			mov	ebx,[Fill_ZBuf_XSlope]
			mov	[DecZBuf],eax

			shr	ebx,8
			mov	eax,[Log]

			and	ebx,0FFFFh
			mov	[PtrLog],eax

			pop	eax

			or	eax,ebx
			pop	ebx

			mov	[DecMapUIntZBuf],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	ebx,[Log]
			mov	eax,[RepMask]

			sub	edi,ebx
			and	eax,edx

			shl	ebp,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			xor	ebx,ebx

			or	edx,esi
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8



@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

			test	bl,bl
			je	@@NoDisp

		push eax
		mov eax,[PtrZBuffer1]
		mov	word ptr [edi*2+eax],dx
		pop eax

			mov	bl,[ebx+Fill_Logical_Palette]

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
		add	esi,[DecZBuf]

	adc	edx,[DecMapUIntZBuf]
			inc	edi

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

		adc	ah,[IntMapV]

		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureChromaKeyFogZBuf ENDP











; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureNZW	PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	ebx,[PtrMap]
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			mov	[DecMapU],eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	eax,0FFh

			or	eax,ebx
			mov	ebx,[PtrZBuffer]

			mov	[Precalc_Inc],eax
			mov	[IntMapU],al

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf],eax

			mov	eax,[Log]
			mov	[IntZBuf],ebx

			pop	ebx
			mov	[PtrLog],eax

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@FastInit:

			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	eax,[Log]
			mov	ebx,[Precalc_Inc]

			shl	ebp,16
			sub	edi,eax

			or	ebx,esi
			mov	eax,[RepMask]

			and	eax,edx
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
			inc	edi
		add	ebx,[DecMapU]

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

			adc	ah,bh
		add	esi,[DecZBuf]

		adc	edx,[IntZBuf]
		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureNZW	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureChromaKeyNZW PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	ebx,[PtrMap]
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			mov	[DecMapU],eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	eax,0FFh

			or	eax,ebx
			mov	ebx,[PtrZBuffer]

			mov	[Precalc_Inc],eax
			mov	[IntMapU],al

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf],eax

			mov	eax,[Log]
			mov	[IntZBuf],ebx

			pop	ebx
			mov	[PtrLog],eax

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@FastInit:

			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	eax,[Log]
			mov	ebx,[Precalc_Inc]

			shl	ebp,16
			sub	edi,eax

			or	ebx,esi
			mov	eax,[RepMask]

			and	eax,edx
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

			test	bl,bl
			je	@@NoDisp

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
			inc	edi
		add	ebx,[DecMapU]

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

			adc	ah,bh
		add	esi,[DecZBuf]

		adc	edx,[IntZBuf]
		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureChromaKeyNZW ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureFlatNZW	PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[Fill_Color]
			mov	ebx,[PtrMap]

			mov	[PtrCLUT],eax
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			push	eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16

			shr	ebx,16
			mov	[IntMapU],al

			mov	[IntMapV],bl
			mov	ebx,[PtrZBuffer]

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf],eax

			mov	eax,[Log]
			and	ebx,0FFFFh

			mov	[PtrLog],eax
			pop	eax

			or	eax,ebx
			pop	ebx

			mov	[DecMapUIntZBuf],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	ebx,[Log]
			mov	eax,[RepMask]

			sub	edi,ebx
			and	eax,edx

			shl	ebp,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			xor	ebx,ebx

			or	edx,esi
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8



@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

		push eax
		mov eax,[PtrCLUT]
		mov	bl,[ebx+eax]
		pop eax

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
		add	esi,[DecZBuf]

	adc	edx,[DecMapUIntZBuf]
			inc	edi

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

		adc	ah,[IntMapV]

		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureFlatNZW	ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureFlatChromaKeyNZW PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[Fill_Color]
			mov	ebx,[PtrMap]

			mov	[PtrCLUT],eax
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			push	eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16

			shr	ebx,16
			mov	[IntMapU],al

			mov	[IntMapV],bl
			mov	ebx,[PtrZBuffer]

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf],eax

			mov	eax,[Log]
			and	ebx,0FFFFh

			mov	[PtrLog],eax
			pop	eax

			or	eax,ebx
			pop	ebx

			mov	[DecMapUIntZBuf],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	ebx,[Log]
			mov	eax,[RepMask]

			sub	edi,ebx
			and	eax,edx

			shl	ebp,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			xor	ebx,ebx

			or	edx,esi
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8



@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

			test	bl,bl
			je	@@NoDisp

		push eax
		mov eax,[PtrCLUT]
		mov	bl,[ebx+eax]
		pop eax

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
		add	esi,[DecZBuf]

	adc	edx,[DecMapUIntZBuf]
			inc	edi

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

		adc	ah,[IntMapV]

		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X

@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureFlatChromaKeyNZW ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureFogNZW	PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	ebx,[PtrMap]
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			push	eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16

			shr	ebx,16
			mov	[IntMapU],al

			mov	[IntMapV],bl
			mov	ebx,[PtrZBuffer]

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf],eax

			mov	eax,[Log]
			and	ebx,0FFFFh

			mov	[PtrLog],eax
			pop	eax

			or	eax,ebx
			pop	ebx

			mov	[DecMapUIntZBuf],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	ebx,[Log]
			mov	eax,[RepMask]

			sub	edi,ebx
			and	eax,edx

			shl	ebp,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			xor	ebx,ebx

			or	edx,esi
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8



@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

			mov	bl,[ebx+Fill_Logical_Palette]

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
		add	esi,[DecZBuf]

	adc	edx,[DecMapUIntZBuf]
			inc	edi

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

		adc	ah,[IntMapV]

		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureFogNZW	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureChromaKeyFogNZW PROC
			inc	ecx
			mov	eax,[Fill_CurY]

			add	eax,ecx
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	esi,[Fill_CurMapUMin]	; ESI = MapU

			mov	eax,[Fill_Patch]
			mov	ebp,[Fill_CurMapVMin]	; EBP = MapV

			dec	eax
			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	ebx,[PtrMap]
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,16
			mov	[PtrMapPatch],ebx

			push	eax
			mov	ebx,[Fill_MapV_XSlope]

			shl	ebx,16
			mov	eax,[RepMask]

			mov	[DecMapV],ebx
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]
			mov	ebx,[Fill_MapV_XSlope]

			shr	eax,16

			shr	ebx,16
			mov	[IntMapU],al

			mov	[IntMapV],bl
			mov	ebx,[PtrZBuffer]

			mov	[PtrZBufferPatch],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf],eax

			mov	eax,[Log]
			and	ebx,0FFFFh

			mov	[PtrLog],eax
			pop	eax

			or	eax,ebx
			pop	ebx

			mov	[DecMapUIntZBuf],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_MapU_LeftSlope]

			add	esi,eax
			mov	eax,[Fill_MapV_LeftSlope]

			add	ebp,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			push	ecx
			mov	eax,ebx

			shr	eax,16
			mov	ecx,edx

			shr	ecx,16
			push	edi

			sub	ecx,eax
			jle	@@EndScanLine

			add	edi,eax
			push	ebp

			push	esi
			push	ebx

			push	edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_MapU_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	edx,eax
			mov	eax,[Fill_MapV_XSlope]

			add	esi,edx

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	ebp,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***
@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi

@@FastInit:
			mov	edx,esi
			mov	ebx,ebp

			shr	edx,16
			and	ebx,0FF0000h

			shr	ebx,8
			and	edx,0FFh

			shl	esi,16
			or	edx,ebx

			mov	ebx,[Log]
			mov	eax,[RepMask]

			sub	edi,ebx
			and	eax,edx

			shl	ebp,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			xor	ebx,ebx

			or	edx,esi
			mov	esi,[Fill_CurZBufMin]

			shl	esi,32-8



@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	word ptr [edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

			test	bl,bl
			je	@@NoDisp

			mov	bl,[ebx+Fill_Logical_Palette]

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
		add	esi,[DecZBuf]

	adc	edx,[DecMapUIntZBuf]
			inc	edi

		adc	al,[IntMapU]
		add	ebp,[DecMapV]

		adc	ah,[IntMapV]

		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X


@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi
			pop	ecx

			dec	ecx
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurMapUMin],esi

			mov	[Fill_CurMapVMin],ebp
			jmp	Triangle_ReadNextEdge
Filler_TextureChromaKeyFogNZW ENDP

_TEXT			ENDS

;			The
			End
*/
