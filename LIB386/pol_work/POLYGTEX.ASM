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

			OPTION	PROC:PRIVATE
			OPTION	SCOPED
			OPTION	LANGUAGE:C



;			**************
;			*** PUBLIC ***
;			**************

;					******************
;					*** PROCEDURES ***
;					******************
PUBLIC	C		Filler_TextureGouraud
PUBLIC	C		Filler_TextureDither
PUBLIC	C		Filler_TextureGouraudChromaKey
PUBLIC	C		Filler_TextureDitherChromaKey
PUBLIC	C		Filler_TextureGouraudZBuf
PUBLIC	C		Filler_TextureGouraudChromaKeyZBuf
PUBLIC	C		Filler_TextureGouraudNZW
PUBLIC	C		Filler_TextureGouraudChromaKeyNZW



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

Extrn	C		Fill_CurGouraudMin	:	DWORD
Extrn	C		Fill_Gouraud_LeftSlope	:	DWORD
Extrn	C		Fill_Gouraud_XSlope	:	DWORD

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

; Runtime code patch refactor, uses memory instead of instruction substitution
PtrLog              DWORD 12345678h;
PtrCLUT             DWORD 12345678h
PtrZBuffer1         DWORD 12345678h
PtrZBufferPatch     DWORD 12345678h
PtrMap2             DWORD 12345678h
PtrMapPatch         DWORD 12345678h
RepMask2            DWORD 12345678h
RepMaskPatch        DWORD 12345678h
DecZBuf_Gour        DWORD 12345678h
DecMapU             DWORD 12345678h
DecMapU2            DWORD 12345678h
DecMapV_Gour        DWORD 12345678h
DecMapV_Gour2       DWORD 12345678h
DecMapV_IntZBuf     DWORD 12345678h
IntMapV             BYTE  12h
IntMapV2            BYTE  12h
IntMapU             BYTE  12h
IntMapU2            BYTE  12h

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


; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureGouraud	PROC
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

		push eax
		mov eax,[PtrCLUT]
		mov	bl,[ebx+eax]
		pop eax

			mov	[edi],bl
			inc	edi

			mov	bh,dh
			and	eax,ebp

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
Filler_TextureGouraud	ENDP





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
