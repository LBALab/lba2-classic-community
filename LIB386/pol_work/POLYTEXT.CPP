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
#include <POLYGON/POLY.H>

U32 Precalc_Inc = 0; // Precalc value

// Runtime code patch refactor, uses memory instead of instruction substitution
U32 PtrZBuffer1 = 0x12345678;
U32 PtrZBufferPatch = 0x12345678;
U32 PtrMapPatch = 0x12345678;
U32 PtrCLUT = 0x12345678;
//U32 PtrLog = 0x12345678;
U32 RepMaskPatch = 0x12345678;
U32 DecMapUIntZBuf = 0x12345678;
U32 DecMapV = 0x12345678;
U32 DecMapU = 0x12345678;
U32 DecZBuf = 0x12345678;
U32 IntZBuf = 0x12345678;
U8 IntMapV = 0x12;
U8 IntMapU = 0x12;
/*
Extrn	C		Triangle_ReadNextEdge	:	PROC	; Entry point that reads the next point in the triangle
*/


S32 Filler_Texture(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 v15; // ebp@9
  unsigned int v17; // eax@9
  U32 v19;          // edx@9

  U32 diffY = nbLines + 1;
  Fill_CurY += diffY;
  U32 u = Fill_CurMapUMin;
  U32 v = Fill_CurMapVMin;
  if (Fill_Patch != 1)
  {
    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
  }
  else
  {
    --Fill_Patch;
    DecMapU = Fill_MapU_XSlope << 16;
    DecMapV = Fill_MapV_XSlope << 16;
    RepMaskPatch = RepMask;
    Precalc_Inc = ((Fill_MapV_XSlope & 0xFF0000u) >> 8) | ((U32)Fill_MapU_XSlope >> 16) & 0xFF;
  }

  while (1)
  {
    U32 fillWidth = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((S32)(fillCurXMax >> 16) > (S32)(fillCurXMin >> 16))
    {
      U8 *line = (fillCurXMin >> 16) + Fill_CurOffLine;
      U32 prevV = v;
      U32 prevU = u;
      // Load 1st dst cache line
      U16 factor = ((U16)fillCurXMin & 0xFFFF) ^ 0xFFFF;
      if (fillWidth > 16)
      {
        U64 product = ((U64)factor * (U64)Fill_MapU_XSlope);
        U32 edx = (U32)(product >> 32);
        U32 eax = (U32)(product & 0xFFFFFFFF);
        edx = edx << 16;
        eax = eax >> 16;
        edx |= eax;
        u += edx;

        product = ((U64)factor * (U64)Fill_MapV_XSlope);
        edx = (U32)(product >> 32);
        eax = (U32)(product & 0xFFFFFFFF);
        edx = edx << 16;
        eax = eax >> 16;
        eax |= edx;
        v += eax;

        // There was some @@Preload code here, but it did nothing other
        // than reading some bytes but not doing anything with it. It
        // also had the comment "Load dst in WB cache", which means nothing
        // to me. I'm guessing it was some kind of prefetching, but I'm
        // not sure. I'm leaving it out for now.
      }
      // @@FastInit:
      v15 = v << 16;
      v17 = RepMask & (((u & 0xFF0000u) >> 16) | ((v >> 8) & 0xFF00));
      v19 = u << 16;
      do
      {
        // @@Each_X:
        v19 = (v19 & 0xFFFFFF00) | (PtrMap[v17] & 0xFF);
        U8 carryFlag = ((U64)DecMapV + (U64)v15) > 0xFFFFFFFF;
        v15 += DecMapV;
        v17 = (v17 & 0xFFFF00FF) | ((((Precalc_Inc >> 8) & 0xFF) + ((v17 >> 8) & 0xFF) + carryFlag) << 8);
        carryFlag = ((U64)DecMapU + (U64)v19) > 0xFFFFFFFF;
        v19 += DecMapU;
        v17 = (v17 & 0xFFFFFF00) | (((Precalc_Inc & 0xFF) + (v17 & 0xFF) + carryFlag) & 0xFF);
        *line++ = v19 & 0xFF;
        v17 &= RepMaskPatch;
        --fillWidth;
      }
      while (fillWidth);
      // @@End_X:
      u = prevU;
      v = prevV;
    }
    // @@EndScanLine:
    diffY--;
    if (diffY == 0)
    {
      break;
    }

    Fill_CurOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
    u += Fill_MapU_LeftSlope;
    v += Fill_MapV_LeftSlope;
  }
  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurMapUMin = u;
  Fill_CurMapVMin = v;
  return Triangle_ReadNextEdge(NULL, NULL);
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
