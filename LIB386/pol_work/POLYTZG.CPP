/*
; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                    (Adeline Software Intl.)                        ***
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
PUBLIC	C		Filler_TextureZGouraud
PUBLIC	C		Filler_TextureZGouraudChromaKey
PUBLIC	C		Filler_TextureZGouraudZBuf
PUBLIC	C		Filler_TextureZGouraudChromaKeyZBuf
PUBLIC	C		Filler_TextureZGouraudNZW
PUBLIC	C		Filler_TextureZGouraudChromaKeyNZW




;					*****************
;					*** VARIABLES ***
;					*****************

;					*** MODULE DATA ***




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

Extrn	C		Fill_CurWMin		:	DWORD
Extrn	C		Fill_W_LeftSlope	:	DWORD
Extrn	C		Fill_W_XSlope		:	DWORD

Extrn	C		Fill_CurGouraudMin	:	DWORD
Extrn	C		Fill_Gouraud_LeftSlope	:	DWORD
Extrn	C		Fill_Gouraud_XSlope 	:	DWORD

Extrn	C		Fill_CurZBufMin		:	DWORD
Extrn	C		Fill_ZBuf_LeftSlope	:	DWORD
Extrn	C		Fill_ZBuf_XSlope	:	DWORD
Extrn	C		Fill_CurZBuf		:	DWORD

Extrn	C		Fill_Cur_W		:	DWORD
Extrn	C		Fill_Next_W		:	DWORD
Extrn	C		Fill_Cur_MapU		:	DWORD
Extrn	C		Fill_Next_MapU		:	DWORD
Extrn	C		Fill_Cur_MapV		:	DWORD
Extrn	C		Fill_Next_MapV		:	DWORD
Extrn	C		Fill_Cur_MapUOverW	:	DWORD
Extrn	C		Fill_Next_MapUOverW	:	DWORD
Extrn	C		Fill_Cur_MapVOverW	:	DWORD
Extrn	C		Fill_Next_MapVOverW	:	DWORD

Extrn	C		Fill_Init_Perspective	:	DWORD
Extrn	C		Fill_Loop_Perspective	:	DWORD





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
Cur_Gouraud		dd	0






;				******************
;				*** Local data ***
;				******************
			ALIGN	4
; Runtime code patch refactor, uses memory instead of instruction substitution
PtrZBuffer1     DWORD 12345678h
PtrZBuffer2     DWORD 12345678h
PtrZBufferPatch DWORD 12345678h
PtrCLUT         DWORD 12345678h
PtrLog          DWORD 12345678h
PtrMap2         DWORD 12345678h
PtrMapPatch     DWORD 12345678h
IntZBuf         DWORD 12345678h
IntZBuf2        DWORD 12345678h
RepMask2        DWORD 12345678h
RepMaskPatch    DWORD 12345678h
DecGour         DWORD 12345678h
DecGour2        DWORD 12345678h
DecZBuf_AddGour     DWORD 12345678h
DecZBuf_AddGour2    DWORD 12345678h
IntGour         BYTE 12h
IntGour2        BYTE 12h


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
Filler_TextureZGouraud	PROC
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
			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			push	ebx

			mov	eax,[RepMask]
			mov	ebx,[Fill_Gouraud_XSlope]

			mov	[RepMaskPatch],eax
			mov	eax,ebx

			shl	ebx,16

			shr	eax,16
			mov	[DecGour],ebx

			mov	[IntGour],al
			mov	eax,[PtrMap]

			mov	[PtrMapPatch],eax
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

			mov	ebx,[Fill_W_LeftSlope]
			mov	eax,[Fill_CurWMin]

			add	eax,ebx
			mov	ebx,[Fill_Gouraud_LeftSlope]

			mov	[Fill_CurWMin],eax
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
			mov	eax,[Fill_CurGouraudMin]

			mov	[Cur_Gouraud],eax
			mov	edx,[Fill_CurWMin]

			mov	[Fill_Cur_W],edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_W_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx

			mov	edx,[Fill_CurWMin]

			add	edx,eax
			mov	eax,[Fill_MapU_XSlope]

			mov	[Fill_Cur_W],edx

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
			call	[Fill_Init_Perspective]
@@FastFill:
			call	[Fill_Loop_Perspective]

			mov	ebx,[Fill_Cur_MapU]	; EBX = CurMapU
			mov	ebp,[Fill_Next_MapU]	; EBP = NextMapU

			sub	ebp,ebx
			mov	edx,[Fill_Cur_MapV]	; EDX = CurMapV

			sar	ebp,PERSP_SHIFT			; EBP = IncMapU
			mov	eax,[Fill_Next_MapV]	; EAX = Next MapV

			sub	eax,edx
			and	ebp,0FFFFh

			sar	eax,PERSP_SHIFT			; EAX = IncMapV
			and	ebx,0FFFFh

			shl	eax,16

			shl	edx,16
			or	ebp,eax			; EBP = IncV | IncU

			or	ebx,edx			; EBX = MapV | MapU
			mov	eax,ebx

			shr	eax,16
			mov	edx,[RepMask]

			mov	al,bh
			push	ecx

			and	eax,edx
			cmp	ecx,PERSP_VALUE

			jle	@@LessPERSP_VALUE

			mov	edx,[Cur_Gouraud]
			mov	ecx,-PERSP_VALUE			; Rasterize PERSP_VALUE pix

			mov	esi,edx
			add	edi,PERSP_VALUE

			shl	esi,16

			shr	edx,8
			jmp	@@LoopX
@@LessPERSP_VALUE:
			lea	edi,[edi+ecx]		; Rasterize less than PERSP_VALUE pix
			mov	edx,[Cur_Gouraud]

			xor	ecx,-1
			mov	esi,edx

			shl	esi,16
			inc	ecx

			shr	edx,8
@@LoopX:
		push ebx
		mov ebx,[PtrMapPatch]
		mov	dl,[eax+ebx]
		pop ebx

			lea	eax,[ebx+ebp]

			shr	eax,16
			add	ebx,ebp

		push ebx
		mov ebx,[PtrCLUT]
		mov	dl,[edx+ebx]
		pop ebx

			mov	al,bh

			mov	[edi+ecx],dl
			add	esi,[DecGour]

			adc	dh,[IntGour]
			and	eax,[RepMaskPatch]

			inc	ecx
			jne	@@LoopX

			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shl	edx,8

			shr	esi,16
			and	edx,0FF0000h

			or	edx,esi
			mov	ebx,[Fill_Next_MapV]

			mov	[Cur_Gouraud],edx
			mov	[Fill_Cur_MapU],eax

			mov	[Fill_Cur_MapV],ebx
			mov	eax,[Fill_Next_MapUOverW]

			mov	ebx,[Fill_Next_MapVOverW]
			mov	[Fill_Cur_MapUOverW],eax

			mov	eax,[Fill_Next_W]
			mov	[Fill_Cur_MapVOverW],ebx

			mov	[Fill_Cur_W],eax
			sub	ecx,PERSP_VALUE

			jg	@@FastFill

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
Filler_TextureZGouraud	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZGouraudChromaKey	PROC
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
			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			push	ebx

			mov	eax,[RepMask]
			mov	ebx,[Fill_Gouraud_XSlope]

			mov	[RepMaskPatch],eax
			mov	[RepMask2],eax

			mov	eax,ebx

			shl	ebx,16

			shr	eax,16
			mov	[DecGour],ebx

			mov	[DecGour2],ebx
			mov	[IntGour],al

			mov	[IntGour2],al
			mov	eax,[PtrMap]

			mov	[PtrMapPatch],eax
			mov	[PtrMap2],eax

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

			mov	ebx,[Fill_W_LeftSlope]
			mov	eax,[Fill_CurWMin]

			add	eax,ebx
			mov	ebx,[Fill_Gouraud_LeftSlope]

			mov	[Fill_CurWMin],eax
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
			mov	eax,[Fill_CurGouraudMin]

			mov	[Cur_Gouraud],eax
			mov	edx,[Fill_CurWMin]

			mov	[Fill_Cur_W],edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_W_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx

			mov	edx,[Fill_CurWMin]

			add	edx,eax
			mov	eax,[Fill_MapU_XSlope]

			mov	[Fill_Cur_W],edx

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
			call	[Fill_Init_Perspective]
@@FastFill:
			call	[Fill_Loop_Perspective]

			mov	ebx,[Fill_Cur_MapU]	; EBX = CurMapU
			mov	ebp,[Fill_Next_MapU]	; EBP = NextMapU

			sub	ebp,ebx
			mov	edx,[Fill_Cur_MapV]	; EDX = CurMapV

			sar	ebp,PERSP_SHIFT			; EBP = IncMapU
			mov	eax,[Fill_Next_MapV]	; EAX = Next MapV

			sub	eax,edx
			and	ebp,0FFFFh

			sar	eax,PERSP_SHIFT			; EAX = IncMapV
			and	ebx,0FFFFh

			shl	eax,16

			shl	edx,16
			or	ebp,eax			; EBP = IncV | IncU

			or	ebx,edx			; EBX = MapV | MapU
			mov	eax,ebx

			shr	eax,16
			mov	edx,[RepMask]

			mov	al,bh
			push	ecx

			and	eax,edx
			cmp	ecx,PERSP_VALUE

			jle	@@LessPERSP_VALUE

			mov	edx,[Cur_Gouraud]
			mov	ecx,-PERSP_VALUE			; Rasterize PERSP_VALUE pix

			mov	esi,edx
			add	edi,PERSP_VALUE

			shl	esi,16

			shr	edx,8
			jmp	@@LoopX
@@LessPERSP_VALUE:
			lea	edi,[edi+ecx]		; Rasterize less than PERSP_VALUE pix
			mov	edx,[Cur_Gouraud]

			xor	ecx,-1
			mov	esi,edx

			shl	esi,16
			inc	ecx

			shr	edx,8
@@LoopX:
		push ebx
		mov ebx,[PtrMapPatch]
		mov	dl,[eax+ebx]
		pop ebx

			lea	eax,[ebx+ebp]

			shr	eax,16
			add	ebx,ebp

			test	dl,dl
			je	@@NotVisible
@@Visible:

		push ebx
		mov ebx,[PtrCLUT]
		mov	dl,[edx+ebx]
		pop ebx

			mov	al,bh

			mov	[edi+ecx],dl
			add	esi,[DecGour]

			adc	dh,[IntGour]
			and	eax,[RepMaskPatch]

			inc	ecx
			jne	@@LoopX
			jmp	@@EndRaster


@@LoopX2:

		push ebx
		mov ebx,[PtrMap2]
		mov	dl,[eax+ebx]
		pop ebx

			lea	eax,[ebx+ebp]

			shr	eax,16
			add	ebx,ebp

			test	dl,dl
			jne	@@Visible
@@NotVisible:
			mov	al,bh
			add	esi,[DecGour2]

			adc	dh,[IntGour2]
			and	eax,[RepMask2]

			inc	ecx
			jne	@@LoopX2

@@EndRaster:


			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shl	edx,8

			shr	esi,16
			and	edx,0FF0000h

			or	edx,esi
			mov	ebx,[Fill_Next_MapV]

			mov	[Cur_Gouraud],edx
			mov	[Fill_Cur_MapU],eax

			mov	[Fill_Cur_MapV],ebx
			mov	eax,[Fill_Next_MapUOverW]

			mov	ebx,[Fill_Next_MapVOverW]
			mov	[Fill_Cur_MapUOverW],eax

			mov	eax,[Fill_Next_W]
			mov	[Fill_Cur_MapVOverW],ebx

			mov	[Fill_Cur_W],eax
			sub	ecx,PERSP_VALUE

			jg	@@FastFill

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
Filler_TextureZGouraudChromaKey	ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZGouraudZBuf PROC
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
			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			push	ebx

			mov	eax,[RepMask]
			mov	ebx,[PtrMap]

			mov	[RepMaskPatch],eax
			mov	[RepMask2],eax

			mov	[PtrMapPatch],ebx
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrZBuffer2],eax
			mov	eax,[Fill_Gouraud_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			and	eax,0FFFFh

			or	eax,ebx
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf_AddGour],eax

			mov	[DecZBuf_AddGour2],eax
			mov	[IntZBuf],ebx

			mov	[IntZBuf2],ebx
			mov	eax,[Log]

			mov	[PtrLog],eax
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

			mov	ebx,[Fill_W_LeftSlope]
			mov	eax,[Fill_CurWMin]

			add	eax,ebx
			mov	ebx,[Fill_Gouraud_LeftSlope]

			mov	[Fill_CurWMin],eax
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
			mov	eax,[Fill_CurGouraudMin]

			mov	[Cur_Gouraud],eax
			mov	eax,[Fill_CurZBufMin]

			mov	[Fill_CurZBuf],eax
			mov	edx,[Fill_CurWMin]

			mov	[Fill_Cur_W],edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_W_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx

			mov	edx,[Fill_CurWMin]

			add	edx,eax
			mov	eax,[Fill_MapU_XSlope]

			mov	[Fill_Cur_W],edx

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
			mov	eax,[Log]

			sub	edi,eax
			call	[Fill_Init_Perspective]
@@FastFill:
			call	[Fill_Loop_Perspective]

			mov	ebx,[Fill_Cur_MapU]	; EBX = CurMapU
			mov	ebp,[Fill_Next_MapU]	; EBP = NextMapU

			sub	ebp,ebx
			mov	edx,[Fill_Cur_MapV]	; EDX = CurMapV

			sar	ebp,PERSP_SHIFT			; EBP = IncMapU
			mov	eax,[Fill_Next_MapV]	; EAX = Next MapV

			sub	eax,edx
			and	ebp,0FFFFh

			sar	eax,PERSP_SHIFT			; EAX = IncMapV
			and	ebx,0FFFFh

			shl	eax,16

			shl	edx,16
			or	ebp,eax			; EBP = IncV | IncU

			or	ebx,edx			; EBX = MapV | MapU
			mov	eax,ebx

			shr	eax,16
			mov	edx,[RepMask]

			mov	al,bh
			push	ecx

			and	eax,edx
			cmp	ecx,PERSP_VALUE

			jle	@@LessPERSP_VALUE

			mov	ecx,PERSP_VALUE			; Rasterize PERSP_VALUE pix
@@LessPERSP_VALUE:               				; Rasterize less than PERSP_VALUE pix
			mov	edx,[Cur_Gouraud]
			mov	esi,[Fill_CurZBuf]

			shr	edx,8

			shl	esi,32-8

			or	edx,esi
			mov	esi,[Fill_CurZBuf]

			shr	esi,8
@@LoopX:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],si
		pop eax

			jb	@@NoDisp
@@Disp:
		push ebx
		mov ebx,[PtrMapPatch]
		mov	al,[eax+ebx]
		pop ebx

			add	ebx,ebp

			mov	ah,dh

		push ebx
		mov ebx,[PtrZBuffer1]
		mov	[edi*2+ebx],si
		pop ebx

		push ebx
		mov ebx,[PtrCLUT]
		mov	al,[eax+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

			mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
			add	edx,[DecZBuf_AddGour]

			adc	esi,[IntZBuf]
			and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@LoopX

			jmp	@@EndPERSP_VALUE

@@LoopX2:
		push ebx
		mov ebx,[PtrZBuffer2]
		cmp	[edi*2+ebx],si
		pop ebx

			jae	@@Disp
@@NoDisp:
			lea	eax,[ebx+ebp]
			add	ebx,ebp

			shr	eax,16
			inc	edi

			mov	al,bh
			add	edx,[DecZBuf_AddGour2]

			adc	esi,[IntZBuf2]
			and	eax,[RepMask2]

			dec	ecx
			jne	@@LoopX2



@@EndPERSP_VALUE:
			pop	ecx
			mov	eax,edx

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shl	eax,8
			mov	ebx,[Fill_Next_MapV]

			shr	edx,32-8
			mov	[Cur_Gouraud],eax

			shl	esi,8
			mov	eax,[Fill_Next_MapU]

			or	edx,esi
			mov	[Fill_Cur_MapU],eax

			mov	[Fill_CurZBuf],edx
			mov	[Fill_Cur_MapV],ebx

			mov	eax,[Fill_Next_MapUOverW]
			mov	ebx,[Fill_Next_MapVOverW]

			mov	[Fill_Cur_MapUOverW],eax
			mov	eax,[Fill_Next_W]

			mov	[Fill_Cur_MapVOverW],ebx
			mov	[Fill_Cur_W],eax

			sub	ecx,PERSP_VALUE
			jg	@@FastFill

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
Filler_TextureZGouraudZBuf ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZGouraudChromaKeyZBuf PROC
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
			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			mov	eax,[RepMask]

			push	ebx
			mov	[RepMaskPatch],eax

			mov	ebx,[PtrMap]
			mov	eax,[PtrZBuffer]

			mov	[PtrMapPatch],ebx
			mov	[PtrZBufferPatch],eax

			mov	[PtrZBuffer1],eax
			mov	eax,[Fill_Gouraud_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			and	eax,0FFFFh

			or	eax,ebx
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf_AddGour],eax

			mov	[IntZBuf],ebx
			mov	eax,[Log]

			mov	[PtrLog],eax
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

			mov	ebx,[Fill_W_LeftSlope]
			mov	eax,[Fill_CurWMin]

			add	eax,ebx
			mov	ebx,[Fill_Gouraud_LeftSlope]

			mov	[Fill_CurWMin],eax
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
			mov	eax,[Fill_CurGouraudMin]

			mov	[Cur_Gouraud],eax
			mov	eax,[Fill_CurZBufMin]

			mov	[Fill_CurZBuf],eax
			mov	edx,[Fill_CurWMin]

			mov	[Fill_Cur_W],edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_W_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx

			mov	edx,[Fill_CurWMin]

			add	edx,eax
			mov	eax,[Fill_MapU_XSlope]

			mov	[Fill_Cur_W],edx

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
			mov	eax,[Log]

			sub	edi,eax
			call	[Fill_Init_Perspective]
@@FastFill:
			call	[Fill_Loop_Perspective]

			mov	ebx,[Fill_Cur_MapU]	; EBX = CurMapU
			mov	ebp,[Fill_Next_MapU]	; EBP = NextMapU

			sub	ebp,ebx
			mov	edx,[Fill_Cur_MapV]	; EDX = CurMapV

			sar	ebp,PERSP_SHIFT			; EBP = IncMapU
			mov	eax,[Fill_Next_MapV]	; EAX = Next MapV

			sub	eax,edx
			and	ebp,0FFFFh

			sar	eax,PERSP_SHIFT			; EAX = IncMapV
			and	ebx,0FFFFh

			shl	eax,16

			shl	edx,16
			or	ebp,eax			; EBP = IncV | IncU

			or	ebx,edx			; EBX = MapV | MapU
			mov	eax,ebx

			shr	eax,16
			mov	edx,[RepMask]

			mov	al,bh
			push	ecx

			and	eax,edx
			cmp	ecx,PERSP_VALUE

			jle	@@LessPERSP_VALUE

			mov	ecx,PERSP_VALUE			; Rasterize PERSP_VALUE pix
@@LessPERSP_VALUE:               				; Rasterize less than PERSP_VALUE pix
			mov	edx,[Cur_Gouraud]
			mov	esi,[Fill_CurZBuf]

			shr	edx,8

			shl	esi,32-8

			or	edx,esi
			mov	esi,[Fill_CurZBuf]

			shr	esi,8
@@LoopX:
		push ebx
		mov ebx,[PtrMapPatch]
		mov	al,[eax+ebx]
		pop ebx

			add	ebx,ebp

			test	al,al
			je	@@NoDisp

		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],si
		pop eax

			jb	@@NoDisp

			mov	ah,dh

		push ebx
		mov ebx,[PtrZBuffer1]
		mov	[edi*2+ebx],si
		pop ebx

		push ebx
		mov ebx,[PtrCLUT]
		mov	al,[eax+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp:		mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
			add	edx,[DecZBuf_AddGour]

			adc	esi,[IntZBuf]
			and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@LoopX

			pop	ecx
			mov	eax,edx

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shl	eax,8
			mov	ebx,[Fill_Next_MapV]

			shr	edx,32-8
			mov	[Cur_Gouraud],eax

			shl	esi,8
			mov	eax,[Fill_Next_MapU]

			or	edx,esi
			mov	[Fill_Cur_MapU],eax

			mov	[Fill_CurZBuf],edx
			mov	[Fill_Cur_MapV],ebx

			mov	eax,[Fill_Next_MapUOverW]
			mov	ebx,[Fill_Next_MapVOverW]

			mov	[Fill_Cur_MapUOverW],eax
			mov	eax,[Fill_Next_W]

			mov	[Fill_Cur_MapVOverW],ebx
			mov	[Fill_Cur_W],eax

			sub	ecx,PERSP_VALUE
			jg	@@FastFill

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
Filler_TextureZGouraudChromaKeyZBuf ENDP











; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZGouraudNZW PROC
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
			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			push	ebx

			mov	eax,[RepMask]
			mov	ebx,[PtrMap]

			mov	[RepMaskPatch],eax
			mov	[RepMask2],eax

			mov	[PtrMapPatch],ebx
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax

			mov	[PtrZBuffer2],eax
			mov	eax,[Fill_Gouraud_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			and	eax,0FFFFh

			or	eax,ebx
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf_AddGour],eax

			mov	[DecZBuf_AddGour2],eax
			mov	[IntZBuf],ebx

			mov	[IntZBuf2],ebx
			mov	eax,[Log]

			mov	[PtrLog],eax
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

			mov	ebx,[Fill_W_LeftSlope]
			mov	eax,[Fill_CurWMin]

			add	eax,ebx
			mov	ebx,[Fill_Gouraud_LeftSlope]

			mov	[Fill_CurWMin],eax
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
			mov	eax,[Fill_CurGouraudMin]

			mov	[Cur_Gouraud],eax
			mov	eax,[Fill_CurZBufMin]

			mov	[Fill_CurZBuf],eax
			mov	edx,[Fill_CurWMin]

			mov	[Fill_Cur_W],edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_W_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx

			mov	edx,[Fill_CurWMin]

			add	edx,eax
			mov	eax,[Fill_MapU_XSlope]

			mov	[Fill_Cur_W],edx

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
			mov	eax,[Log]

			sub	edi,eax
			call	[Fill_Init_Perspective]
@@FastFill:
			call	[Fill_Loop_Perspective]

			mov	ebx,[Fill_Cur_MapU]	; EBX = CurMapU
			mov	ebp,[Fill_Next_MapU]	; EBP = NextMapU

			sub	ebp,ebx
			mov	edx,[Fill_Cur_MapV]	; EDX = CurMapV

			sar	ebp,PERSP_SHIFT			; EBP = IncMapU
			mov	eax,[Fill_Next_MapV]	; EAX = Next MapV

			sub	eax,edx
			and	ebp,0FFFFh

			sar	eax,PERSP_SHIFT			; EAX = IncMapV
			and	ebx,0FFFFh

			shl	eax,16

			shl	edx,16
			or	ebp,eax			; EBP = IncV | IncU

			or	ebx,edx			; EBX = MapV | MapU
			mov	eax,ebx

			shr	eax,16
			mov	edx,[RepMask]

			mov	al,bh
			push	ecx

			and	eax,edx
			cmp	ecx,PERSP_VALUE

			jle	@@LessPERSP_VALUE

			mov	ecx,PERSP_VALUE			; Rasterize PERSP_VALUE pix
@@LessPERSP_VALUE:               				; Rasterize less than PERSP_VALUE pix
			mov	edx,[Cur_Gouraud]
			mov	esi,[Fill_CurZBuf]

			shr	edx,8

			shl	esi,32-8

			or	edx,esi
			mov	esi,[Fill_CurZBuf]

			shr	esi,8
@@LoopX:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],si
		pop eax

			jb	@@NoDisp
@@Disp:
		push ebx
		mov ebx,[PtrMapPatch]
		mov	al,[eax+ebx]
		pop ebx

			add	ebx,ebp

			mov	ah,dh

		push ebx
		mov ebx,[PtrCLUT]
		mov	al,[eax+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

			mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
			add	edx,[DecZBuf_AddGour]

			adc	esi,[IntZBuf]
			and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@LoopX

			jmp	@@EndPERSP_VALUE

@@LoopX2:
		push ebx
		mov ebx,[PtrZBuffer2]
		cmp	[edi*2+ebx],si
		pop ebx

			jae	@@Disp
@@NoDisp:
			lea	eax,[ebx+ebp]
			add	ebx,ebp

			shr	eax,16
			inc	edi

			mov	al,bh
			add	edx,[DecZBuf_AddGour2]

			adc	esi,[IntZBuf2]
			and	eax,[RepMask2]

			dec	ecx
			jne	@@LoopX2



@@EndPERSP_VALUE:
			pop	ecx
			mov	eax,edx

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shl	eax,8
			mov	ebx,[Fill_Next_MapV]

			shr	edx,32-8
			mov	[Cur_Gouraud],eax

			shl	esi,8
			mov	eax,[Fill_Next_MapU]

			or	edx,esi
			mov	[Fill_Cur_MapU],eax

			mov	[Fill_CurZBuf],edx
			mov	[Fill_Cur_MapV],ebx

			mov	eax,[Fill_Next_MapUOverW]
			mov	ebx,[Fill_Next_MapVOverW]

			mov	[Fill_Cur_MapUOverW],eax
			mov	eax,[Fill_Next_W]

			mov	[Fill_Cur_MapVOverW],ebx
			mov	[Fill_Cur_W],eax

			sub	ecx,PERSP_VALUE
			jg	@@FastFill

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
Filler_TextureZGouraudNZW ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZGouraudChromaKeyNZW PROC
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
			mov	eax,[PtrCLUTGouraud]

			mov	[PtrCLUT],eax
			mov	eax,[RepMask]

			push	ebx
			mov	[RepMaskPatch],eax

			mov	ebx,[PtrMap]
			mov	eax,[PtrZBuffer]

			mov	[PtrMapPatch],ebx
			mov	[PtrZBufferPatch],eax

			mov	eax,[Fill_Gouraud_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			and	eax,0FFFFh

			or	eax,ebx
			mov	ebx,[Fill_ZBuf_XSlope]

			shr	ebx,8
			mov	[DecZBuf_AddGour],eax

			mov	[IntZBuf],ebx
			mov	eax,[Log]

			mov	[PtrLog],eax
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

			mov	ebx,[Fill_W_LeftSlope]
			mov	eax,[Fill_CurWMin]

			add	eax,ebx
			mov	ebx,[Fill_Gouraud_LeftSlope]

			mov	[Fill_CurWMin],eax
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
			mov	eax,[Fill_CurGouraudMin]

			mov	[Cur_Gouraud],eax
			mov	eax,[Fill_CurZBufMin]

			mov	[Fill_CurZBuf],eax
			mov	edx,[Fill_CurWMin]

			mov	[Fill_Cur_W],edx
			and	ebx,0FFFFh

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,0FFFFh

			mov	eax,[Fill_W_XSlope]

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16

			shr	eax,16

			or	eax,edx

			mov	edx,[Fill_CurWMin]

			add	edx,eax
			mov	eax,[Fill_MapU_XSlope]

			mov	[Fill_Cur_W],edx

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
			mov	eax,[Log]

			sub	edi,eax
			call	[Fill_Init_Perspective]
@@FastFill:
			call	[Fill_Loop_Perspective]

			mov	ebx,[Fill_Cur_MapU]	; EBX = CurMapU
			mov	ebp,[Fill_Next_MapU]	; EBP = NextMapU

			sub	ebp,ebx
			mov	edx,[Fill_Cur_MapV]	; EDX = CurMapV

			sar	ebp,PERSP_SHIFT		; EBP = IncMapU
			mov	eax,[Fill_Next_MapV]	; EAX = Next MapV

			sub	eax,edx
			and	ebp,0FFFFh

			sar	eax,PERSP_SHIFT		; EAX = IncMapV
			and	ebx,0FFFFh

			shl	eax,16

			shl	edx,16
			or	ebp,eax			; EBP = IncV | IncU

			or	ebx,edx			; EBX = MapV | MapU
			mov	eax,ebx

			shr	eax,16
			mov	edx,[RepMask]

			mov	al,bh
			push	ecx

			and	eax,edx
			cmp	ecx,PERSP_VALUE

			jle	@@LessPERSP_VALUE

			mov	ecx,PERSP_VALUE			; Rasterize PERSP_VALUE pix
@@LessPERSP_VALUE:               				; Rasterize less than PERSP_VALUE pix
			mov	edx,[Cur_Gouraud]
			mov	esi,[Fill_CurZBuf]

			shr	edx,8

			shl	esi,32-8

			or	edx,esi
			mov	esi,[Fill_CurZBuf]

			shr	esi,8
@@LoopX:
		push ebx
		mov ebx,[PtrMapPatch]
		mov	al,[eax+ebx]
		pop ebx

			add	ebx,ebp

			test	al,al
			je	@@NoDisp

		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],si
		pop eax

			jb	@@NoDisp

			mov	ah,dh

		push ebx
		mov ebx,[PtrCLUT]
		mov	al,[eax+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp:		mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
			add	edx,[DecZBuf_AddGour]

			adc	esi,[IntZBuf]
			and	eax,[RepMaskPatch]

			dec	ecx
			jne	@@LoopX

			pop	ecx
			mov	eax,edx

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shl	eax,8
			mov	ebx,[Fill_Next_MapV]

			shr	edx,32-8
			mov	[Cur_Gouraud],eax

			shl	esi,8
			mov	eax,[Fill_Next_MapU]

			or	edx,esi
			mov	[Fill_Cur_MapU],eax

			mov	[Fill_CurZBuf],edx
			mov	[Fill_Cur_MapV],ebx

			mov	eax,[Fill_Next_MapUOverW]
			mov	ebx,[Fill_Next_MapVOverW]

			mov	[Fill_Cur_MapUOverW],eax
			mov	eax,[Fill_Next_W]

			mov	[Fill_Cur_MapVOverW],ebx
			mov	[Fill_Cur_W],eax

			sub	ecx,PERSP_VALUE
			jg	@@FastFill

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
Filler_TextureZGouraudChromaKeyNZW ENDP

_TEXT			ENDS

;			The
			End
*/
