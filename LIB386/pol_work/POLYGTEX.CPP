/*
; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                         (Adeline Software Intl.)                   ***
; ***                           1995-96-97                               ***
; ***                                                                    ***
; **************************************************************************
; **************************************************************************
; **************************************************************************
*/

#include <POLYGON/POLYGTEX.H>

#include <POLYGON/POLY.H>

#include <stdio.h>

PTR_U8 GTEX_PtrLog = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrCLUT = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrZBuffer1 = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrZBufferPatch = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrMap2 = (PTR_U8)0x12345678;
PTR_U8 GTEX_PtrMapPatch = (PTR_U8)0x12345678;
U32 GTEX_RepMask2 = 0x12345678;
U32 GTEX_RepMaskPatch = 0x12345678;
S32 GTEX_DecZBuf_Gour = 0x12345678;
S32 GTEX_DecMapU = 0x12345678;
S32 GTEX_DecMapU2 = 0x12345678;
S32 GTEX_DecMapV_Gour = 0x12345678;
S32 GTEX_DecMapV_Gour2 = 0x12345678;
S32 GTEX_DecMapV_IntZBuf = 0x12345678;
S8 GTEX_IntMapV = 0x12;
S8 GTEX_IntMapV2 = 0x12;
S8 GTEX_IntMapU = 0x12;
S8 GTEX_IntMapU2 = 0x12;

S32 Filler_TextureGouraud(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = Fill_CurOffLine;
  U32 curMapU = Fill_CurMapUMin;
  U32 curMapV = Fill_CurMapVMin;
  Fill_CurY += count;
  U32 patch = Fill_Patch - 1;

  if (patch != 0)
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curMapU += Fill_MapU_LeftSlope;
    curMapV += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  Fill_Patch = patch;

  GTEX_PtrMapPatch = PtrMap;
  GTEX_PtrCLUT = PtrCLUTGouraud;
  GTEX_DecMapU = Fill_MapU_XSlope << 16;
  GTEX_IntMapU = (Fill_MapU_XSlope >> 16) & 0xFF;
  GTEX_IntMapV = (Fill_MapV_XSlope >> 16) & 0xFF;
  GTEX_DecMapV_Gour = (Fill_MapV_XSlope << 16) | (Fill_Gouraud_XSlope >> 8) & 0xFFFF;
  while (1)
  {
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      U8 *screen = (fillCurXMin >> 16) + curOffLine;
      U32 gouraudXSlope = Fill_Gouraud_XSlope;
      S32 curGouraudMin = Fill_CurGouraudMin;
      gouraudXSlope = (gouraudXSlope & 0xFFFF00FF) | (((curMapV >> 16) & 0xFF) << 8);
      U32 u = curMapU << 16;
      gouraudXSlope = (gouraudXSlope & 0xFFFFFF00) | ((curMapU >> 16) & 0xFF);
      U32 v = (curMapV << 16) | (curGouraudMin >> 8) & 0xFFFF;
      U32 colorIndex = (curGouraudMin >> 8);
      U32 mask = RepMask & gouraudXSlope;
      do
      {
        colorIndex = (colorIndex & 0xFFFFFF00) | (*(mask + GTEX_PtrMapPatch) & 0xFF);
        U8 carry = ((U64)GTEX_DecMapU + (U64)u) > 0xFFFFFFFF;
        u += GTEX_DecMapU;
        mask = (mask & 0xFFFFFF00) | ((GTEX_IntMapU + carry + mask) & 0xFF);
        carry = ((U64)GTEX_DecMapV_Gour + (U64)v) > 0xFFFFFFFF;
        v += GTEX_DecMapV_Gour;
        mask = (mask & 0xFFFF00FF) | ((((mask >> 8) & 0xFF) + GTEX_IntMapV + carry) & 0xFF);
        *screen++ = *(colorIndex + GTEX_PtrCLUT);
        colorIndex = (colorIndex & 0xFFFF00FF) | (v & 0xFF00);
        mask &= RepMask;
        --diffX;
      }
      while (diffX);
    }
    // @@EndScanLine:
    if (!--count)
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    curMapU += Fill_MapU_LeftSlope;
    curMapV += Fill_MapV_LeftSlope;
    Fill_CurGouraudMin += Fill_Gouraud_LeftSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;
  Fill_CurMapUMin = curMapU;
  Fill_CurMapVMin = curMapV;

  return Triangle_ReadNextEdge(NULL, NULL);
}

/*

; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureDither	PROC
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
			mov	eax,[PtrMap]

			mov	[PtrMapPatch],eax
			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			mov	eax,[Fill_MapU_XSlope]

			push	ebx
			mov	ebx,eax

			shl	eax,16
			push	ecx

			shr	ebx,16
			mov	[DecMapU],eax

			mov	[IntMapU],bl
			mov	eax,[Fill_MapV_XSlope]

			mov	ecx,[Fill_Gouraud_XSlope]
			mov	ebx,eax

			shr	ecx,8

			shr	ebx,16
			and	ecx,0FFFFh

			shl	eax,16
			mov	[IntMapV],bl

			or	eax,ecx
			pop	ecx

			mov	[DecMapV_Gour],eax

			pop	ebx
			mov	eax,[RepMask]

			mov	[RepMaskPatch],eax
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

			mov	ebx,[Fill_Gouraud_LeftSlope]
			mov	eax,[Fill_CurGouraudMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurGouraudMin],eax
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

			mov	al,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	edx,[Fill_CurGouraudMin]
			mov	eax,[Fill_MapU_XSlope]

			push	edx

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
			mov	ebx,esi
			mov	edx,ebp

			shr	edx,16

			shr	ebx,16
			mov	ah,dl

			shl	esi,16
			mov	al,bl

			shl	ebp,16
			pop	edx		; Fill_CurGouraudMin

			shr	edx,8

			mov	ebx,edx
			and	edx,0FFFFh

			or	ebp,edx

			and	ebx,0FFFFh
			and	eax,[RepMask]

@@Each_X:
		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

		add	esi,[DecMapU]
		adc	al,[IntMapU]
		add	ebp,[DecMapV_Gour]
		adc	ah,[IntMapV]

		push eax
		mov eax,[PtrCLUT]
		mov	bl,[ebx+eax]
		pop eax

			xor	dh,dh
			mov	[edi],bl

			add	edx,ebp
			inc	edi

			mov	bh,dh
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
Filler_TextureDither	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureGouraudChromaKey	PROC
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
			mov	eax,[PtrMap]

			mov	[PtrMapPatch],eax
			mov	[PtrMap2],eax

			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			mov	eax,[Fill_MapU_XSlope]

			push	ebx
			mov	ebx,eax

			shl	eax,16
			push	ecx

			shr	ebx,16
			mov	[DecMapU],eax

			mov	[DecMapU2],eax
			mov	[IntMapU],bl

			mov	[IntMapU2],bl
			mov	eax,[Fill_MapV_XSlope]

			mov	ecx,[Fill_Gouraud_XSlope]
			mov	ebx,eax

			shr	ecx,8

			shr	ebx,16
			and	ecx,0FFFFh

			shl	eax,16
			mov	[IntMapV],bl
			mov	[IntMapV2],bl

			or	eax,ecx
			pop	ecx

			mov	[DecMapV_Gour],eax
			mov	[DecMapV_Gour2],eax

			pop	ebx
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

			mov	ebx,[Fill_Gouraud_LeftSlope]
			mov	eax,[Fill_CurGouraudMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurGouraudMin],eax
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

			mov	al,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	edx,[Fill_CurGouraudMin]
			mov	eax,[Fill_Gouraud_XSlope]

			push	edx

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx
			pop	edx

			add	edx,eax

			push	edx
			mov	eax,[Fill_MapU_XSlope]

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
			mov	ebx,esi
			mov	edx,ebp

			shr	edx,16

			shr	ebx,16
			mov	ah,dl

			shl	esi,16
			mov	al,bl

			shl	ebp,16
			pop	edx		; Fill_CurGouraudMin

			shr	edx,8

			mov	ebx,edx
			and	edx,0FFFFh

			or	edx,ebp
			mov	ebp,[RepMask]

			and	ebx,0FFFFh
			and	eax,ebp

@@Each_X:
		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

		add	esi,[DecMapU]
		adc	al,[IntMapU]
		add	edx,[DecMapV_Gour]
		adc	ah,[IntMapV]

			inc	edi

			test	bl,bl
			je	@@NotVisible
@@Visible:
		push eax
		mov eax,[PtrCLUT]
		mov	bl,[ebx+eax]
		pop eax

			and	eax,ebp

			mov	[edi-1],bl
			mov	bh,dh

			dec	ecx
			jne	@@Each_X
			jmp	@@End_X

@@Each_X2:
		push ecx
		mov ecx,[PtrMap2]
		mov	bl,[eax+ecx]
		pop ecx

		add	esi,[DecMapU2]
		adc	al,[IntMapU2]
		add	edx,[DecMapV_Gour2]
		adc	ah,[IntMapV2]

			inc	edi

			test	bl,bl
			jne	@@Visible
@@NotVisible:
			mov	bh,dh
			and	eax,ebp

			dec	ecx
			jne	@@Each_X2


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
Filler_TextureGouraudChromaKey	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureDitherChromaKey	PROC
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
			mov	eax,[PtrMap]

			mov	[PtrMapPatch],eax
			mov	[PtrMap2],eax

			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			mov	eax,[Fill_MapU_XSlope]

			push	ebx
			mov	ebx,eax

			shl	eax,16
			push	ecx

			shr	ebx,16
			mov	[DecMapU],eax

			mov	[DecMapU2],eax
			mov	[IntMapU],bl

			mov	[IntMapU2],bl
			mov	eax,[Fill_MapV_XSlope]

			mov	ecx,[Fill_Gouraud_XSlope]
			mov	ebx,eax

			shr	ecx,8

			shr	ebx,16
			and	ecx,0FFFFh

			shl	eax,16
			mov	[IntMapV],bl

			mov	[IntMapV2],bl
			or	eax,ecx

			pop	ecx
			mov	[DecMapV_Gour],eax

			mov	[DecMapV_Gour2],eax
			mov	eax,[RepMask]

			mov	[RepMaskPatch],eax
			mov	[RepMask2],eax

			pop	ebx
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

			mov	ebx,[Fill_Gouraud_LeftSlope]
			mov	eax,[Fill_CurGouraudMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurGouraudMin],eax
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

			mov	al,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	edx,[Fill_CurGouraudMin]
			mov	eax,[Fill_MapU_XSlope]

			push	edx

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
			mov	ebx,esi
			mov	edx,ebp

			shr	edx,16

			shr	ebx,16
			mov	ah,dl

			shl	esi,16
			mov	al,bl

			shl	ebp,16
			pop	edx		; Fill_CurGouraudMin

			shr	edx,8

			mov	ebx,edx
			and	edx,0FFFFh

			or	ebp,edx

			and	ebx,0FFFFh
			and	eax,[RepMask]

@@Each_X:
		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

		add	esi,[DecMapU]
		adc	al,[IntMapU]
		add	ebp,[DecMapV_Gour]
		adc	ah,[IntMapV]

			inc	edi

			test	bl,bl
			je	@@NotVisible
@@Visible:
		push eax
		mov eax,[PtrCLUT]
		mov	bl,[ebx+eax]
		pop eax

			xor	dh,dh

			mov	[edi-1],bl
			add	edx,ebp

			mov	bh,dh
		and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@Each_X
			jmp	@@End_X

@@Each_X2:
		push ecx
		mov ecx,[PtrMap2]
		mov	bl,[eax+ecx]
		pop ecx

		add	esi,[DecMapU2]
		adc	al,[IntMapU2]
		add	ebp,[DecMapV_Gour2]
		adc	ah,[IntMapV2]

			inc	edi

			test	bl,bl
			jne	@@Visible
@@NotVisible:
			xor	dh,dh
		and	eax,[RepMask2]

			add	edx,ebp
			dec	ecx

			mov	bh,dh
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
Filler_TextureDitherChromaKey	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureGouraudZBuf PROC
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

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrLog],ebx
			mov	eax,[PtrMap]

			mov	[PtrMapPatch],eax
			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			mov	eax,[RepMask]

			mov	[RepMaskPatch],eax
			mov	eax,[Fill_MapU_XSlope]

			shr	eax,16
			mov	ebx,[Fill_MapU_XSlope]

			shl	ebx,16
			mov	[IntMapU],al

			mov	[DecMapU],ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			shr	ebx,8
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			and	ebx,0FFFFh

			or	eax,ebx
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf_Gour],eax

			mov	eax,[Fill_MapV_XSlope]

			shl	eax,16

			or	eax,ebx
			mov	ebx,[Fill_MapV_XSlope]

			shr	ebx,16
			mov	[DecMapV_IntZBuf],eax

			mov	[IntMapV],bl
			pop	ebx

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

			mov	ebx,[Fill_Gouraud_LeftSlope]
			mov	eax,[Fill_CurGouraudMin]

			add	eax,ebx
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurGouraudMin],eax
			mov	eax,[Fill_CurZBufMin]

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

			mov	al,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	edx,[Fill_CurGouraudMin]
			mov	eax,[Fill_Gouraud_XSlope]

			push	edx

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx
			pop	edx

			add	edx,eax

			push	edx
			mov	eax,[Fill_MapU_XSlope]

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
			mov	eax,esi
			mov	ebx,ebp

			shr	eax,16
			mov	edx,[Log]

			shr	ebx,16
			sub	edi,edx

			mov	ah,bl
			mov	ebx,[RepMask]

			shl	ebp,16
			and	eax,ebx

			shl	esi,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			mov	ebx,[Fill_CurZBufMin]

			or	ebp,edx
			pop	edx		; Fill_CurGouraudMin

			shr	edx,8

			shl	ebx,32-8

			or	edx,ebx
			xor	ebx,ebx
			mov	bh,dh
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bp
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

		push eax
		mov eax,[PtrZBuffer1]
		mov	[edi*2+eax],bp
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
		add	esi,[DecMapU]
		adc	al,[IntMapU]
		add	edx,[DecZBuf_Gour]
		adc	ebp,[DecMapV_IntZBuf]

			inc	edi

		adc	ah,[IntMapV]
			mov	bh,dh

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
Filler_TextureGouraudZBuf ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureGouraudChromaKeyZBuf PROC
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

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrLog],ebx
			mov	eax,[PtrMap]

			mov	[PtrMapPatch],eax
			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			mov	eax,[RepMask]

			mov	[RepMaskPatch],eax
			mov	eax,[Fill_MapU_XSlope]

			shr	eax,16
			mov	ebx,[Fill_MapU_XSlope]

			shl	ebx,16
			mov	[IntMapU],al

			mov	[DecMapU],ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			shr	ebx,8
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			and	ebx,0FFFFh

			or	eax,ebx
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf_Gour],eax

			mov	eax,[Fill_MapV_XSlope]

			shl	eax,16

			or	eax,ebx
			mov	ebx,[Fill_MapV_XSlope]

			shr	ebx,16
			mov	[DecMapV_IntZBuf],eax

			mov	[IntMapV],bl
			pop	ebx

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

			mov	ebx,[Fill_Gouraud_LeftSlope]
			mov	eax,[Fill_CurGouraudMin]

			add	eax,ebx
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurGouraudMin],eax
			mov	eax,[Fill_CurZBufMin]

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

			mov	al,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	edx,[Fill_CurGouraudMin]
			mov	eax,[Fill_Gouraud_XSlope]

			push	edx

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx
			pop	edx

			add	edx,eax

			push	edx
			mov	eax,[Fill_MapU_XSlope]

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
			mov	eax,esi
			mov	ebx,ebp

			shr	eax,16
			mov	edx,[Log]

			shr	ebx,16
			sub	edi,edx

			mov	ah,bl
			mov	ebx,[RepMask]

			shl	ebp,16
			and	eax,ebx

			shl	esi,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			mov	ebx,[Fill_CurZBufMin]

			or	ebp,edx
			pop	edx		; Fill_CurGouraudMin

			shr	edx,8

			shl	ebx,32-8

			or	edx,ebx
			xor	ebx,ebx
			mov	bh,dh
@@Each_X:
		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

			test	bl,bl
			je	@@NoDisp

		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bp
		pop eax

			jb	@@NoDisp

		push eax
		mov eax,[PtrZBuffer1]
		mov	[edi*2+eax],bp
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
		add	esi,[DecMapU]
		adc	al,[IntMapU]
		add	edx,[DecZBuf_Gour]
		adc	ebp,[DecMapV_IntZBuf]

			inc	edi

		adc	ah,[IntMapV]
			mov	bh,dh

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
Filler_TextureGouraudChromaKeyZBuf ENDP











; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureGouraudNZW PROC
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

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrLog],ebx

			mov	eax,[PtrMap]
			mov	[PtrMapPatch],eax

			mov	eax,[PtrCLUTGouraud]
			mov	[PtrCLUT],eax

			mov	eax,[RepMask]
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]

			shr	eax,16
			mov	ebx,[Fill_MapU_XSlope]

			shl	ebx,16
			mov	[IntMapU],al

			mov	[DecMapU],ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			shr	ebx,8
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			and	ebx,0FFFFh

			or	eax,ebx
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf_Gour],eax

			mov	eax,[Fill_MapV_XSlope]

			shl	eax,16

			or	eax,ebx
			mov	ebx,[Fill_MapV_XSlope]

			shr	ebx,16
			mov	[DecMapV_IntZBuf],eax

			mov	[IntMapV],bl
			pop	ebx

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

			mov	ebx,[Fill_Gouraud_LeftSlope]
			mov	eax,[Fill_CurGouraudMin]

			add	eax,ebx
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurGouraudMin],eax
			mov	eax,[Fill_CurZBufMin]

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

			mov	al,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	edx,[Fill_CurGouraudMin]
			mov	eax,[Fill_Gouraud_XSlope]

			push	edx

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx
			pop	edx

			add	edx,eax

			push	edx
			mov	eax,[Fill_MapU_XSlope]

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
			mov	eax,esi
			mov	ebx,ebp

			shr	eax,16
			mov	edx,[Log]

			shr	ebx,16
			sub	edi,edx

			mov	ah,bl
			mov	ebx,[RepMask]

			shl	ebp,16
			and	eax,ebx

			shl	esi,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			mov	ebx,[Fill_CurZBufMin]

			or	ebp,edx
			pop	edx		; Fill_CurGouraudMin

			shr	edx,8

			shl	ebx,32-8

			or	edx,ebx
			xor	ebx,ebx
			mov	bh,dh
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bp
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
		add	esi,[DecMapU]
		adc	al,[IntMapU]
		add	edx,[DecZBuf_Gour]
		adc	ebp,[DecMapV_IntZBuf]

			inc	edi

		adc	ah,[IntMapV]
			mov	bh,dh

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
Filler_TextureGouraudNZW ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureGouraudChromaKeyNZW PROC
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

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrLog],ebx

			mov	eax,[PtrMap]
			mov	[PtrMapPatch],eax

			mov	eax,[PtrCLUTGouraud]
			mov	[PtrCLUT],eax

			mov	eax,[RepMask]
			mov	[RepMaskPatch],eax

			mov	eax,[Fill_MapU_XSlope]

			shr	eax,16
			mov	ebx,[Fill_MapU_XSlope]

			shl	ebx,16
			mov	[IntMapU],al

			mov	[DecMapU],ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			shr	ebx,8
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			and	ebx,0FFFFh

			or	eax,ebx
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf_Gour],eax

			mov	eax,[Fill_MapV_XSlope]

			shl	eax,16

			or	eax,ebx
			mov	ebx,[Fill_MapV_XSlope]

			shr	ebx,16
			mov	[DecMapV_IntZBuf],eax

			mov	[IntMapV],bl
			pop	ebx

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

			mov	ebx,[Fill_Gouraud_LeftSlope]
			mov	eax,[Fill_CurGouraudMin]

			add	eax,ebx
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurGouraudMin],eax
			mov	eax,[Fill_CurZBufMin]

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

			mov	al,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	edx,[Fill_CurGouraudMin]
			mov	eax,[Fill_Gouraud_XSlope]

			push	edx

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx
			pop	edx

			add	edx,eax

			push	edx
			mov	eax,[Fill_MapU_XSlope]

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
			mov	eax,esi
			mov	ebx,ebp

			shr	eax,16
			mov	edx,[Log]

			shr	ebx,16
			sub	edi,edx

			mov	ah,bl
			mov	ebx,[RepMask]

			shl	ebp,16
			and	eax,ebx

			shl	esi,16
			mov	edx,[Fill_CurZBufMin]

			shr	edx,8
			mov	ebx,[Fill_CurZBufMin]

			or	ebp,edx
			pop	edx		; Fill_CurGouraudMin

			shr	edx,8

			shl	ebx,32-8

			or	edx,ebx
			xor	ebx,ebx
			mov	bh,dh
@@Each_X:
		push ecx
		mov ecx,[PtrMapPatch]
		mov	bl,[eax+ecx]
		pop ecx

			test	bl,bl
			je	@@NoDisp

		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bp
		pop eax

			jb	@@NoDisp

		push eax
		mov eax,[PtrCLUT]
		mov	bl,[ebx+eax]
		pop eax

		push eax
		mov eax,[PtrLog]
		mov	[edi+eax],bl
		pop eax

@@NoDisp:
		add	esi,[DecMapU]
		adc	al,[IntMapU]
		add	edx,[DecZBuf_Gour]
		adc	ebp,[DecMapV_IntZBuf]

			inc	edi

		adc	ah,[IntMapV]
			mov	bh,dh

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
Filler_TextureGouraudChromaKeyNZW ENDP

_TEXT			ENDS

;			The
			End
*/
