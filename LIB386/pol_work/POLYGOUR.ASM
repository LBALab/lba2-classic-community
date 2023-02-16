; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                              (Adeline Software Intl.)              ***
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
PUBLIC	C		Filler_Gouraud
PUBLIC	C		Filler_Dither
PUBLIC	C		Filler_GouraudTable
PUBLIC	C		Filler_DitherTable
PUBLIC	C		Filler_GouraudFog
PUBLIC	C		Filler_DitherFog
PUBLIC	C		Filler_GouraudZBuf
PUBLIC	C		Filler_DitherZBuf
PUBLIC	C		Filler_GouraudTableZBuf
PUBLIC	C		Filler_DitherTableZBuf
PUBLIC	C		Filler_GouraudFogZBuf
PUBLIC	C		Filler_DitherFogZBuf
PUBLIC	C		Filler_GouraudNZW
PUBLIC	C		Filler_DitherNZW
PUBLIC	C		Filler_GouraudTableNZW
PUBLIC	C		Filler_DitherTableNZW
PUBLIC	C		Filler_GouraudFogNZW
PUBLIC	C		Filler_DitherFogNZW

;					*****************
;					*** VARIABLES ***
;					*****************

;					*** PUBLIC DATA ***


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

Extrn	C		Fill_LeftSlope		:	DWORD
Extrn	C		Fill_RightSlope		:	DWORD

Extrn	C		Fill_CurY		:	DWORD
Extrn	C		Fill_CurXMin		:	DWORD
Extrn	C		Fill_CurXMax		:	DWORD

Extrn	C		PTR_TabOffLine		:	DWORD
Extrn	C		Fill_CurOffLine		:	DWORD
Extrn	C		Log			:	DWORD
Extrn	C		ScreenPitch		:	DWORD
Extrn	C		PtrZBuffer		:	DWORD

Extrn	C		Fill_CurGouraudMin	:	DWORD
Extrn	C		Fill_Gouraud_LeftSlope	:	DWORD
Extrn	C		Fill_Gouraud_XSlope	:	DWORD

Extrn	C		Fill_CurZBufMin		:	DWORD
Extrn	C		Fill_ZBuf_LeftSlope	:	DWORD
Extrn	C		Fill_ZBuf_XSlope	:	DWORD

Extrn	C		Fill_Logical_Palette	:	BYTE



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
AddGour             DWORD 12345678h
AddGour1            DWORD 12345678h
AddGour2            DWORD 12345678h
AddGour3            DWORD 12345678h
IntZBuf             DWORD 12345678h
IntZBuf1            DWORD 12345678h
IntZBuf2            DWORD 12345678h
IntZBuf3            DWORD 12345678h
DecGour             DWORD 12345678h
DecZBuf             DWORD 12345678h
DecZBuf1            DWORD 12345678h
DecZBuf2            DWORD 12345678h
DecZBuf3            DWORD 12345678h
DecZBuf_AddGour     DWORD 12345678h
DecZBuf_AddGour1    DWORD 12345678h
DecZBuf_AddGour2    DWORD 12345678h
DecZBuf_AddGour3    DWORD 12345678h
PtrLog              DWORD 12345678h
PtrLog_1            DWORD 12345678h
PtrLog2             DWORD 12345678h
PtrLog2_1           DWORD 12345678h
PtrLog4             DWORD 12345678h
PtrZBuffer_1        DWORD 12345678h
PtrZBuffer1         DWORD 12345678h
PtrZBuffer1_1       DWORD 12345678h
PtrZBuffer2         DWORD 12345678h
PtrZBuffer2_1       DWORD 12345678h
PtrZBuffer3         DWORD 12345678h
PtrZBuffer3_1       DWORD 12345678h
PtrZBuffer4         DWORD 12345678h
PtrZBuffer5         DWORD 12345678h
PtrZBufferPatch     DWORD 12345678h
PtrColor            DWORD 12345678h
PtrColor1           DWORD 12345678h
PtrColor2           DWORD 12345678h
PtrColor3           DWORD 12345678h
PtrColor4           DWORD 12345678h
IntGour             BYTE  12h

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
Filler_Gouraud		PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebp
			push	esi

			push	ebx
			push	edx

			mov	eax,ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			and	eax,0FFFFh
			mov	dl,[edi]	; Load 1st dst cache line

			xor	eax,0FFFFh

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	esi,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***

@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@FastInit:		mov	ebp,ebx		; EBP = Gouraud_XSlope(Frac part)

			shl	ebp,16

			sar	ebx,16		; EBX = Gouraud_XSlope(Int part)
			mov	eax,esi

			shr	eax,16

			shl	esi,16

				; *** Gouraud loop ***
@@Each_X:
			mov	[edi],al
			add	esi,ebp

			adc	al,bl
			inc	edi

			dec	ecx
			jne	@@Each_X
@@End_X:

			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_Gouraud		ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_Dither		PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


				; *** Gouraud dither loop ***
			push	edx
			push	ebp

			mov	dl,[edi]	; Load 1st dst cache line

			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache
			jb	@@NoPreload

				; *** Load dst in WB cache ***
			push	edi
			lea	edx,[edi+ecx]

			and	edi,not 31
			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@NoPreload:		mov	edx,esi

			sar	edx,8
			mov	ebp,[Fill_Gouraud_XSlope]

			sar	ebp,8
			mov	eax,edx

			shr	ecx,1
			jnc	@@Pas_Add1
			je	@@Add_One

			rol	al,cl
@@Each_X:
			and	eax,0FFh

			add	eax,edx
			add	edx,ebp

			mov	[edi],ah
			and	eax,0FFh

			add	eax,edx
			add	edx,ebp

			mov	[edi+1],ah
			add	edi,2

			rol	al,cl

			dec	ecx
			jne	@@Each_X

@@Add_One:
			and	eax,0FFh
			add	eax,edx
			mov	[edi],ah
			jmp	@@End_X
@@Pas_Add1:
			je	@@End_X
@@Each_X2:
			and	eax,0FFh

			add	eax,edx
			add	edx,ebp

			mov	[edi],ah
			and	eax,0FFh

			add	eax,edx
			add	edx,ebp

			mov	[edi+1],ah
			add	edi,2

			rol	al,cl

			dec	ecx
			jne	@@Each_X2
@@End_X:
			pop	ebp
			pop	edx

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_Dither		ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_GouraudTable	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebp
			push	esi

			push	ebx
			push	edx

			mov	eax,ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			and	eax,0FFFFh
			mov	dl,[edi]	; Load 1st dst cache line
			xor	eax,0FFFFh

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			lea	ebp,[edi+ecx]

			or	eax,edx
			and	edi,not 31

			add	esi,eax
			and	ebp,not 31

				; *** Load dst in WB cache ***

@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,ebp
			jbe	@@Preload

			pop	edi
@@FastInit:
			mov	edx,[Fill_Color]	; PtrCLUTGouraud + Color index
			mov	eax,esi

			shr	eax,8
			mov	ebp,0FF00h

			and	eax,ebp

				; *** Gouraud loop ***
@@Each_X:
			mov	al,[eax+edx]
			add	esi,ebx

			mov	[edi],al
			mov	eax,esi

			shr	eax,8
			inc	edi

			and	eax,ebp

			dec	ecx
			jne	@@Each_X

@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_GouraudTable	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_DitherTable	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


				; *** Gouraud dither loop ***
			push	edx
			push	ebp
			push	esi
			push	ebx

			mov	dl,[edi]	; Load 1st dst cache line

			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache
			jb	@@NoPreload

				; *** Load dst in WB cache ***
			push	edi
			lea	edx,[edi+ecx]

			and	edi,not 31
			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@NoPreload:		mov	edx,esi

			sar	edx,8
			mov	ebp,[Fill_Gouraud_XSlope]

			sar	ebp,8
			mov	eax,edx

			mov	esi,[Fill_Color]
			and	eax,0FFh

			add	eax,edx

			shr	ecx,1
			jnc	@@Pas_Add1
			je	@@Add_One

			rol	al,cl
@@Each_X:
			mov	ebx,eax
			add	edx,ebp

			and	eax,0FFh
			and	ebx,0FF00h

			add	eax,edx
			add	edx,ebp

			mov	bl,[esi+ebx]

			mov	[edi],bl
			mov	ebx,eax

			and	ebx,0FF00h
			and	eax,0FFh

			rol	al,cl

			mov	bl,[ebx+esi]
			add	eax,edx

			mov	[edi+1],bl
			add	edi,2

			dec	ecx
			jne	@@Each_X

@@Add_One:
			and	eax,0FF00h
			mov	al,[eax+esi]
			mov	[edi],al
			jmp	@@End_X
@@Pas_Add1:
			je	@@End_X
@@Each_X2:
			mov	ebx,eax
			add	edx,ebp

			and	eax,0FFh
			and	ebx,0FF00h

			add	eax,edx
			add	edx,ebp

			mov	bl,[esi+ebx]

			mov	[edi],bl
			mov	ebx,eax

			and	ebx,0FF00h
			and	eax,0FFh

			rol	al,cl

			mov	bl,[ebx+esi]
			add	eax,edx

			mov	[edi+1],bl
			add	edi,2

			dec	ecx
			jne	@@Each_X2

@@End_X:
			pop	ebx
			pop	esi
			pop	ebp
			pop	edx

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_DitherTable	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_GouraudFog	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebp
			push	esi

			push	ebx
			push	edx

			mov	eax,ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			and	eax,0FFFFh
			mov	dl,[edi]	; Load 1st dst cache line
			xor	eax,0FFFFh

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	esi,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***

@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@FastInit:		mov	ebp,ebx		; EBP = Gouraud_XSlope(Frac part)

			shl	ebp,16
			add	edi,ecx

			sar	ebx,16		; EBX = Gouraud_XSlope(Int part)
			mov	eax,esi

			shr	eax,16
			xor	ecx,-1

			shl	esi,16
			and	eax,0FFh

			inc	ecx

				; *** Gouraud loop ***
@@Each_X:
			add	esi,ebp
			mov	dl,[eax+Fill_Logical_Palette]

			adc	al,bl
			mov	[edi+ecx],dl

			inc	ecx
			jne	@@Each_X
@@End_X:

			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_GouraudFog	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_DitherFog	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


				; *** Gouraud dither loop ***
			push	ebx
			push	edx
			push	ebp

			mov	dl,[edi]	; Load 1st dst cache line
			xor	ebx,ebx

			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache
			jb	@@NoPreload

				; *** Load dst in WB cache ***
			push	edi
			lea	edx,[edi+ecx]

			and	edi,not 31
			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@NoPreload:		mov	edx,esi

			sar	edx,8
			mov	ebp,[Fill_Gouraud_XSlope]

			sar	ebp,8
			mov	eax,edx

			shr	ecx,1
			jnc	@@Pas_Add1
			je	@@Add_One

			rol	al,cl
@@Each_X:
			and	eax,0FFh

			add	eax,edx
			add	edx,ebp

			mov	bl,ah
			and	eax,0FFh

			add	eax,edx
			add	edx,ebp

			mov	bl,[Fill_Logical_Palette+ebx]

			mov	[edi],bl
			mov	bl,ah

			rol	al,cl

			mov	bl,[Fill_Logical_Palette+ebx]

			mov	[edi+1],bl
			add	edi,2

			dec	ecx
			jne	@@Each_X

@@Add_One:
			and	eax,0FFh
			add	eax,edx
			mov	bl,ah
			mov	bl,[Fill_Logical_Palette+ebx]
			mov	[edi],bl
			jmp	@@End_X
@@Pas_Add1:
			je	@@End_X
@@Each_X2:
			and	eax,0FFh

			add	eax,edx
			add	edx,ebp

			mov	bl,ah
			and	eax,0FFh

			add	eax,edx
			add	edx,ebp

			mov	bl,[Fill_Logical_Palette+ebx]

			mov	[edi],bl
			mov	bl,ah

			rol	al,cl

			mov	bl,[Fill_Logical_Palette+ebx]

			mov	[edi+1],bl
			add	edi,2

			dec	ecx
			jne	@@Each_X2
@@End_X:
			pop	ebp
			pop	edx
			pop	ebx

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_DitherFog	ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_GouraudZBuf	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[Fill_Gouraud_XSlope]

			shl	eax,16
			push	ebx

			mov	[DecGour],eax
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	[DecZBuf],ebx

			mov	[IntZBuf],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	ebx,[Log]

			mov	[PtrLog],ebx
			pop	ebx

			mov	[PtrZBuffer1],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebp
			push	esi

			push	ebx
			push	edx

			mov	eax,ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			and	eax,0FFFFh
			mov	dl,[edi]	; Load 1st dst cache line
			xor	eax,0FFFFh

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	esi,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***

@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@FastInit:		sar	ebx,16		; EBX = Gouraud_XSlope(Int part)
			mov	ebp,[Fill_CurZBufMin]

			mov	eax,[Log]
			mov	edx,ebp

			shl	ebp,32-8
			sub	edi,eax

			shr	edx,8
			mov	eax,esi

			shr	eax,16

			shl	esi,16

				; *** Gouraud loop ***
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push eax
		mov eax,[PtrZBuffer1]
		mov	[edi*2+eax],dx
		pop eax

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp:
		add	esi,[DecGour]

			adc	al,bl
		add	ebp,[DecZBuf]

		adc	edx,[IntZBuf]
			inc	edi

			dec	ecx
			jne	@@Each_X
@@End_X:

			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_GouraudZBuf	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_DitherZBuf	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[Fill_Gouraud_XSlope]

			sar	eax,8
			push	ebx

			mov	[AddGour],eax
			mov	[AddGour1],eax

			mov	[AddGour2],eax
			mov	[AddGour3],eax

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrZBuffer2],eax
			mov	[PtrZBuffer3],eax

			mov	[PtrZBuffer4],eax
			mov	[PtrZBuffer5],eax

			mov	[PtrLog],ebx
			mov	[PtrLog2],ebx

			mov	[PtrLog4],ebx
			add	eax,2

			inc	ebx
			mov	[PtrZBuffer_1],eax

			mov	[PtrZBuffer1_1],eax
			mov	[PtrZBuffer2_1],eax

			mov	[PtrZBuffer3_1],eax
			mov	[PtrLog_1],ebx

			mov	[PtrLog2_1],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	[IntZBuf],eax

			mov	[IntZBuf1],eax
			mov	[IntZBuf2],eax

			mov	[IntZBuf3],eax
			mov	[DecZBuf],ebx

			mov	[DecZBuf1],ebx
			mov	[DecZBuf2],ebx

			mov	[DecZBuf3],ebx
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


				; *** Gouraud dither loop ***
			push	edx
			push	ebp
			push	ebx

			mov	dl,[edi]	; Load 1st dst cache line

			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache
			jb	@@NoPreload

				; *** Load dst in WB cache ***
			push	edi
			lea	edx,[edi+ecx]

			and	edi,not 31
			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@NoPreload:		mov	edx,esi
			mov	ebx,[Fill_CurZBufMin]

			shr	ebx,8
			mov	ebp,[Fill_CurZBufMin]

			shl	ebp,32-8
			mov	eax,[Log]

			sub	edi,eax

			sar	edx,8
			mov	eax,edx

			shr	ecx,1
			jnc	@@Pas_Add1
			je	@@Add_One

			rol	al,cl
@@Each_X:
			and	eax,0FFh

			add	eax,edx
		add	edx,[AddGour]

		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp0

		push eax
		mov eax,[PtrZBuffer1]
		mov	[edi*2+eax],bx
		pop eax

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp0:
			and	eax,0FFh
		add	ebp,[DecZBuf]

		adc	ebx,[IntZBuf]
			add	eax,edx

		push eax
		mov eax,[PtrZBuffer_1]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp1

	push eax
	mov eax,[PtrZBuffer1_1]
	mov	[edi*2+eax],bx
	pop eax

		push ebx
		mov ebx,[PtrLog_1]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp1:
		add	edx,[AddGour1]
		add	ebp,[DecZBuf1]

		adc	ebx,[IntZBuf1]
			add	edi,2

			rol	al,cl

			dec	ecx
			jne	@@Each_X

@@Add_One:
		push eax
		mov eax,[PtrZBuffer4]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@End_X

		push eax
		mov eax,[PtrZBuffer5]
		mov	[edi*2+eax],bx
		pop eax

			and	eax,0FFh
			add	eax,edx

		push ebx
		mov ebx,[PtrLog4]
		mov	[edi+ebx],ah
		pop ebx

			jmp	@@End_X
@@Pas_Add1:
			je	@@End_X
@@Each_X2:
			and	eax,0FFh

			add	eax,edx
		add	edx,[AddGour2]

		push eax
		mov eax,[PtrZBuffer2]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp2

		push eax
		mov eax,[PtrZBuffer3]
		mov	[edi*2+eax],bx
		pop eax

		push ebx
		mov ebx,[PtrLog2]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp2:
			and	eax,0FFh
		add	ebp,[DecZBuf2]

		adc	ebx,[IntZBuf2]
			add	eax,edx

	push eax
	mov eax,[PtrZBuffer2_1]
	cmp	[edi*2+eax],bx
	pop eax

			jb	@@NoDisp3

	push eax
	mov eax,[PtrZBuffer3_1]
	mov	[edi*2+eax],bx
	pop eax

		push ebx
		mov ebx,[PtrLog2_1]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp3:
		add	edx,[AddGour3]
		add	ebp,[DecZBuf3]

		adc	ebx,[IntZBuf3]
			add	edi,2

			rol	al,cl

			dec	ecx
			jne	@@Each_X2
@@End_X:
			pop	ebx
			pop	ebp
			pop	edx

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_DitherZBuf	ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_GouraudTableZBuf	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx
			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrLog],ebx
			mov	eax,[Fill_Gouraud_XSlope]

			mov	[AddGour],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	[IntZBuf],eax

			mov	[DecZBuf],ebx
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebp
			push	esi

			push	ebx
			push	edx

			mov	eax,ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			and	eax,0FFFFh
			mov	dl,[edi]	; Load 1st dst cache line
			xor	eax,0FFFFh

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			lea	ebp,[edi+ecx]

			or	eax,edx
			and	edi,not 31

			add	esi,eax
			and	ebp,not 31

				; *** Load dst in WB cache ***

@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,ebp
			jbe	@@Preload

			pop	edi
@@FastInit:
			mov	edx,[Fill_Color]	; PtrCLUTGouraud + Color index
			mov	ebx,[Fill_CurZBufMin]

			mov	eax,[Log]
			mov	ebp,ebx

			shr	ebx,8
			sub	edi,eax

			shl	ebp,32-8
			mov	eax,esi

			shr	eax,8

			and	eax,0FF00h

				; *** Gouraud loop ***
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp

		push eax
		mov eax,[PtrZBuffer1]
		mov	[edi*2+eax],bx
		pop eax

			mov	al,[eax+edx]

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp:
		add	esi,[AddGour]
		add	ebp,[DecZBuf]

		adc	ebx,[IntZBuf]
			mov	eax,esi

			shr	eax,8
			inc	edi

			and	eax,0FF00h

			dec	ecx
			jne	@@Each_X

@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_GouraudTableZBuf	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_DitherTableZBuf	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrZBuffer2],eax
			mov	[PtrZBuffer3],eax

			mov	[PtrZBuffer4],eax
			mov	[PtrZBuffer5],eax

			add	eax,2
			mov	[PtrLog],ebx

			mov	[PtrZBuffer_1],eax
			mov	[PtrZBuffer1_1],eax

			mov	[PtrZBuffer2_1],eax
			mov	[PtrLog2],ebx

			mov	[PtrLog4],ebx
			inc	ebx

			mov	[PtrLog_1],ebx
			mov	[PtrZBuffer3_1],eax

			mov	[PtrLog2_1],ebx
			mov	eax,[Fill_Gouraud_XSlope]

			sar	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			mov	[AddGour],eax
			mov	[AddGour1],eax

			mov	[AddGour2],eax
			mov	[AddGour3],eax

			shr	ebx,8
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	[IntZBuf],ebx

			mov	[IntZBuf1],ebx
			mov	[IntZBuf2],ebx

			mov	[IntZBuf3],ebx
			mov	[DecZBuf],eax

			mov	[DecZBuf1],eax
			mov	[DecZBuf2],eax

			mov	[DecZBuf3],eax
			mov	eax,[Fill_Color]

			mov	[PtrColor],eax
			mov	[PtrColor1],eax

			mov	[PtrColor2],eax
			mov	[PtrColor3],eax

			mov	[PtrColor4],eax
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


				; *** Gouraud dither loop ***
			push	edx
			push	ebp
			push	esi
			push	ebx

			mov	dl,[edi]	; Load 1st dst cache line

			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache
			jb	@@NoPreload

				; *** Load dst in WB cache ***
			push	edi
			lea	edx,[edi+ecx]

			and	edi,not 31
			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@NoPreload:		mov	edx,esi

			sar	edx,8
			mov	eax,[Log]

			sub	edi,eax
			mov	eax,edx

			and	eax,0FFh
			mov	ebx,[Fill_CurZBufMin]

			shr	ebx,8
			add	eax,edx

			mov	ebp,[Fill_CurZBufMin]
			mov	esi,eax

			shl	ebp,32-8
			and	esi,0FF00h

			shr	ecx,1
			jnc	@@Pas_Add1
			je	@@Add_One

			rol	al,cl

@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp

		push eax
		mov eax,[PtrZBuffer1]
		mov	[edi*2+eax],bx
		pop eax

		push ebx
		mov ebx,[PtrColor]
		mov	ah,[esi+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp:
			and	eax,0FFh
		add	edx,[AddGour]

			add	eax,edx
		add	ebp,[DecZBuf]

		adc	ebx,[IntZBuf]
			mov	esi,eax

			and	esi,0FF00h

		push eax
		mov eax,[PtrZBuffer_1]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp1

	push eax
	mov eax,[PtrZBuffer1_1]
	mov	[edi*2+eax],bx
	pop eax

		push ebx
		mov ebx,[PtrColor1]
		mov	ah,[esi+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog_1]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp1:
		add	edx,[AddGour1]
			and	eax,0FFh

			add	eax,edx
		add	ebp,[DecZBuf1]

		adc	ebx,[IntZBuf1]
			mov	esi,eax

			add	edi,2
			and	esi,0FF00h

			rol	al,cl

			dec	ecx
			jne	@@Each_X


@@Add_One:
		push eax
		mov eax,[PtrZBuffer4]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@End_X

		push eax
		mov eax,[PtrZBuffer5]
		mov	[edi*2+eax],bx
		pop eax

		push ebx
		mov ebx,[PtrColor4]
		mov	ah,[esi+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog4]
		mov	[edi+ebx],ah
		pop ebx

			jmp	@@End_X
@@Pas_Add1:
			je	@@End_X
@@Each_X2:
		push eax
		mov eax,[PtrZBuffer2]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp2

		push eax
		mov eax,[PtrZBuffer3]
		mov	[edi*2+eax],bx
		pop eax

		push ebx
		mov ebx,[PtrColor2]
		mov	ah,[esi+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog2]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp2:
			and	eax,0FFh
		add	edx,[AddGour2]

			add	eax,edx
		add	ebp,[DecZBuf2]

		adc	ebx,[IntZBuf2]
			mov	esi,eax

			and	esi,0FF00h

	push eax
	mov eax,[PtrZBuffer2_1]
	cmp	[edi*2+eax],bx
	pop eax

			jb	@@NoDisp3

	push eax
	mov eax,[PtrZBuffer3_1]
	mov	[edi*2+eax],bx
	pop eax

		push ebx
		mov ebx,[PtrColor3]
		mov	ah,[esi+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog2_1]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp3:
		add	edx,[AddGour3]
			and	eax,0FFh

			add	eax,edx
		add	ebp,[DecZBuf3]

		adc	ebx,[IntZBuf3]
			mov	esi,eax

			add	edi,2
			and	esi,0FF00h

			rol	al,cl

			dec	ecx
			jne	@@Each_X2

@@End_X:
			pop	ebx
			pop	esi
			pop	ebp
			pop	edx

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_DitherTableZBuf	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_GouraudFogZBuf	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrLog],ebx
			mov	eax,[Fill_Gouraud_XSlope]

			shr	eax,16
			mov	ebx,[Fill_Gouraud_XSlope]

			shl	ebx,16
			mov	[IntGour],al

			mov	[DecGour],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	[IntZBuf],eax

			mov	[DecZBuf],ebx
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebp
			push	esi

			push	ebx
			push	edx

			mov	eax,ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			and	eax,0FFFFh
			mov	dl,[edi]	; Load 1st dst cache line
			xor	eax,0FFFFh

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	esi,eax
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
			mov	ebp,[Fill_CurZBufMin]

			shl	ebp,32-8
			sub	edi,eax

			mov	ebx,[Fill_CurZBufMin]
			mov	eax,esi

			shr	ebx,8

			shr	eax,16

			shl	esi,16
			and	eax,0FFh

				; *** Gouraud loop ***
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp

		push eax
		mov eax,[PtrZBuffer1]
		mov	[edi*2+eax],bx
		pop eax

			mov	dl,[eax+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],dl
		pop ebx

@@NoDisp:
			inc	edi
		add	esi,[DecGour]

		adc	al,[IntGour]
		add	ebp,[DecZBuf]

		adc	ebx,[IntZBuf]
			dec	ecx

			jne	@@Each_X
@@End_X:

			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_GouraudFogZBuf	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_DitherFogZBuf	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[Fill_Gouraud_XSlope]
			mov	ebx,[Fill_ZBuf_XSlope]

			sar	eax,8

			shl	ebx,32-8
			and	eax,0FFFFh

			or	eax,ebx

			mov	[DecZBuf_AddGour],eax
			mov	[DecZBuf_AddGour1],eax

			mov	[DecZBuf_AddGour2],eax
			mov	[DecZBuf_AddGour3],eax

			mov	ebx,[Log]
			mov	eax,[Fill_ZBuf_XSlope]

			mov	[PtrLog],ebx
			mov	[PtrLog2],ebx

			mov	[PtrLog4],ebx
			inc	ebx

			mov	[PtrLog_1],ebx
			mov	[PtrLog2_1],ebx

			shr	eax,8

			mov	[IntZBuf],eax
			mov	[IntZBuf1],eax

			mov	[IntZBuf2],eax
			mov	[IntZBuf3],eax

			pop	ebx
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


				; *** Gouraud dither loop ***
			push	edx
			push	ebp
			push	ebx
			push	esi

			mov	dl,[edi]	; Load 1st dst cache line

			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache
			jb	@@NoPreload

				; *** Load dst in WB cache ***
			push	edi
			lea	edx,[edi+ecx]

			and	edi,not 31
			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@NoPreload:
			shr	esi,8
			mov	ebx,[Fill_CurZBufMin]

			and	esi,0FFFFh
			mov	ebp,ebx

			shr	ebx,8
			mov	eax,[Log]

			shl	ebp,32-8
			sub	edi,eax

			mov	eax,esi
			or	esi,ebp

			mov	ebp,[PtrZBuffer]
			and	eax,0FFh

			add	eax,esi
			xor	edx,edx

			mov	dl,ah

			shr	ecx,1
			jnc	@@Pas_Add1
			je	@@Add_One

			rol	al,cl
@@Each_X:
			cmp	word ptr [edi*2+ebp],bx
			jb	@@NoDisp0
			mov	word ptr [edi*2+ebp],bx
			mov	dl,[edx+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],dl
		pop ebx

@@NoDisp0:
			and	eax,0FFh
	add	esi,[DecZBuf_AddGour]

		adc	ebx,[IntZBuf]
			add	eax,esi

			mov	dl,ah

			cmp	word ptr [edi*2+ebp+2],bx
			jb	@@NoDisp1
			mov	word ptr [edi*2+ebp+2],bx
			mov	dl,[edx+Fill_Logical_Palette]

		push eax
		mov eax,[PtrLog_1]
		mov	[edi+eax],dl
		pop eax

@@NoDisp1:
			and	eax,0FFh
	add	esi,[DecZBuf_AddGour1]

		adc	ebx,[IntZBuf1]
			add	eax,esi

			add	edi,2
			mov	dl,ah

			rol	al,cl

			dec	ecx
			jne	@@Each_X
@@Add_One:
			cmp	word ptr [edi*2+ebp],bx
			jb	@@End_X
			mov	word ptr [edi*2+ebp],bx
			mov	dl,[edx+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog4]
		mov	[edi+ebx],dl
		pop ebx

			jmp	@@End_X
@@Pas_Add1:
			je	@@End_X
@@Each_X2:
			cmp	word ptr [edi*2+ebp],bx
			jb	@@NoDisp2
			mov	word ptr [edi*2+ebp],bx
			mov	dl,[edx+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog2]
		mov	[edi+ebx],dl
		pop ebx

@@NoDisp2:
			and	eax,0FFh
	add	esi,[DecZBuf_AddGour2]

		adc	ebx,[IntZBuf2]
			add	eax,esi

			mov	dl,ah

			cmp	word ptr [edi*2+ebp+2],bx
			jb	@@NoDisp3
			mov	word ptr [edi*2+ebp+2],bx
			mov	dl,[edx+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog2_1]
		mov	[edi+ebx],dl
		pop ebx

@@NoDisp3:
			and	eax,0FFh
	add	esi,[DecZBuf_AddGour3]

		adc	ebx,[IntZBuf3]
			add	eax,esi

			add	edi,2
			mov	dl,ah

			rol	al,cl

			dec	ecx
			jne	@@Each_X2
@@End_X:
			pop	esi
			pop	ebx
			pop	ebp
			pop	edx

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_DitherFogZBuf	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_GouraudNZW	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[Fill_Gouraud_XSlope]

			shl	eax,16
			push	ebx

			mov	[DecGour],eax
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	[DecZBuf],ebx

			mov	[IntZBuf],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	ebx,[Log]

			mov	[PtrLog],ebx
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebp
			push	esi

			push	ebx
			push	edx

			mov	eax,ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			and	eax,0FFFFh
			mov	dl,[edi]	; Load 1st dst cache line
			xor	eax,0FFFFh

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	esi,eax
			and	edx,not 31	; AGI on EDI on first loop

				; *** Load dst in WB cache ***

@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@FastInit:		sar	ebx,16		; EBX = Gouraud_XSlope(Int part)
			mov	ebp,[Fill_CurZBufMin]

			mov	eax,[Log]
			mov	edx,ebp

			shl	ebp,32-8
			sub	edi,eax

			shr	edx,8
			mov	eax,esi

			shr	eax,16

			shl	esi,16

				; *** Gouraud loop ***
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp:
		add	esi,[DecGour]

			adc	al,bl
		add	ebp,[DecZBuf]

		adc	edx,[IntZBuf]
			inc	edi

			dec	ecx
			jne	@@Each_X
@@End_X:

			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_GouraudNZW	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_DitherNZW	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[Fill_Gouraud_XSlope]

			sar	eax,8
			push	ebx

			mov	[AddGour],eax
			mov	[AddGour1],eax

			mov	[AddGour2],eax
			mov	[AddGour3],eax

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrZBuffer2],eax
			mov	[PtrZBuffer3],eax

			mov	[PtrZBuffer4],eax

			mov	[PtrLog],ebx
			mov	[PtrLog2],ebx

			mov	[PtrLog4],ebx
			add	eax,2

			inc	ebx
			mov	[PtrLog_1],ebx

			mov	[PtrLog2_1],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	[IntZBuf],eax

			mov	[IntZBuf1],eax
			mov	[IntZBuf2],eax

			mov	[IntZBuf3],eax
			mov	[DecZBuf],ebx

			mov	[DecZBuf1],ebx
			mov	[DecZBuf2],ebx

			mov	[DecZBuf3],ebx
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


				; *** Gouraud dither loop ***
			push	edx
			push	ebp
			push	ebx

			mov	dl,[edi]	; Load 1st dst cache line

			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache
			jb	@@NoPreload

				; *** Load dst in WB cache ***
			push	edi
			lea	edx,[edi+ecx]

			and	edi,not 31
			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@NoPreload:		mov	edx,esi
			mov	ebx,[Fill_CurZBufMin]

			shr	ebx,8
			mov	ebp,[Fill_CurZBufMin]

			shl	ebp,32-8
			mov	eax,[Log]

			sub	edi,eax

			sar	edx,8
			mov	eax,edx

			shr	ecx,1
			jnc	@@Pas_Add1
			je	@@Add_One

			rol	al,cl
@@Each_X:
			and	eax,0FFh

			add	eax,edx
		add	edx,[AddGour]

		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp0

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp0:
			and	eax,0FFh
		add	ebp,[DecZBuf]

		adc	ebx,[IntZBuf]
			add	eax,edx

		push eax
		mov eax,[PtrZBuffer1]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp1

		push ebx
		mov ebx,[PtrLog_1]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp1:
		add	edx,[AddGour1]
		add	ebp,[DecZBuf1]

		adc	ebx,[IntZBuf1]
			add	edi,2

			rol	al,cl

			dec	ecx
			jne	@@Each_X

@@Add_One:
		push eax
		mov eax,[PtrZBuffer4]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@End_X
			and	eax,0FFh
			add	eax,edx

		push ebx
		mov ebx,[PtrLog4]
		mov	[edi+ebx],ah
		pop ebx

			jmp	@@End_X
@@Pas_Add1:
			je	@@End_X
@@Each_X2:
			and	eax,0FFh

			add	eax,edx
		add	edx,[AddGour2]

		push eax
		mov eax,[PtrZBuffer2]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp2

		push ebx
		mov ebx,[PtrLog2]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp2:
			and	eax,0FFh
		add	ebp,[DecZBuf2]

		adc	ebx,[IntZBuf2]
			add	eax,edx

		push eax
		mov eax,[PtrZBuffer3]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp3

		push ebx
		mov ebx,[PtrLog2_1]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp3:
		add	edx,[AddGour3]
		add	ebp,[DecZBuf3]

		adc	ebx,[IntZBuf3]
			add	edi,2

			rol	al,cl

			dec	ecx
			jne	@@Each_X2
@@End_X:
			pop	ebx
			pop	ebp
			pop	edx

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_DitherNZW	ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_GouraudTableNZW	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrLog],ebx

			mov	eax,[Fill_Gouraud_XSlope]

			mov	[AddGour],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	[IntZBuf],eax

			mov	[DecZBuf],ebx
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebp
			push	esi

			push	ebx
			push	edx

			mov	eax,ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			and	eax,0FFFFh
			mov	dl,[edi]	; Load 1st dst cache line
			xor	eax,0FFFFh

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			lea	ebp,[edi+ecx]

			or	eax,edx
			and	edi,not 31

			add	esi,eax
			and	ebp,not 31

				; *** Load dst in WB cache ***

@@Preload:
			mov	al,[edi]
			add	edi,32

			cmp	edi,ebp
			jbe	@@Preload

			pop	edi
@@FastInit:
			mov	edx,[Fill_Color]	; PtrCLUTGouraud + Color index
			mov	ebx,[Fill_CurZBufMin]

			mov	eax,[Log]
			mov	ebp,ebx

			shr	ebx,8
			sub	edi,eax

			shl	ebp,32-8
			mov	eax,esi

			shr	eax,8

			and	eax,0FF00h

				; *** Gouraud loop ***
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp
			mov	al,[eax+edx]

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp:
		add	esi,[AddGour]
		add	ebp,[DecZBuf]

		adc	ebx,[IntZBuf]
			mov	eax,esi

			shr	eax,8
			inc	edi

			and	eax,0FF00h

			dec	ecx
			jne	@@Each_X

@@End_X:
			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_GouraudTableNZW	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_DitherTableNZW	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrZBuffer2],eax
			mov	[PtrZBuffer3],eax

			mov	[PtrZBuffer4],eax

			add	eax,2
			mov	[PtrLog],ebx

			mov	[PtrLog2],ebx
			mov	[PtrLog4],ebx

			inc	ebx
			mov	eax,[Fill_Gouraud_XSlope]

			mov	[PtrLog_1],ebx
			mov	[PtrLog2_1],ebx

			sar	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			mov	[AddGour],eax
			mov	[AddGour1],eax

			mov	[AddGour2],eax
			mov	[AddGour3],eax

			shr	ebx,8
			mov	eax,[Fill_ZBuf_XSlope]

			shl	eax,32-8
			mov	[IntZBuf],ebx

			mov	[IntZBuf1],ebx
			mov	[IntZBuf2],ebx

			mov	[IntZBuf3],ebx
			mov	[DecZBuf],eax

			mov	[DecZBuf1],eax
			mov	[DecZBuf2],eax

			mov	[DecZBuf3],eax
			mov	eax,[Fill_Color]

			mov	[PtrColor],eax
			mov	[PtrColor1],eax

			mov	[PtrColor2],eax
			mov	[PtrColor3],eax

			mov	[PtrColor4],eax
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


				; *** Gouraud dither loop ***
			push	edx
			push	ebp
			push	esi
			push	ebx

			mov	dl,[edi]	; Load 1st dst cache line

			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache
			jb	@@NoPreload

				; *** Load dst in WB cache ***
			push	edi
			lea	edx,[edi+ecx]

			and	edi,not 31
			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@NoPreload:		mov	edx,esi

			sar	edx,8
			mov	eax,[Log]

			sub	edi,eax
			mov	eax,edx

			and	eax,0FFh
			mov	ebx,[Fill_CurZBufMin]

			shr	ebx,8
			add	eax,edx

			mov	ebp,[Fill_CurZBufMin]
			mov	esi,eax

			shl	ebp,32-8
			and	esi,0FF00h

			shr	ecx,1
			jnc	@@Pas_Add1
			je	@@Add_One

			rol	al,cl

@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp

		push ebx
		mov ebx,[PtrColor]
		mov	ah,[esi+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp:
			and	eax,0FFh
		add	edx,[AddGour]

			add	eax,edx
		add	ebp,[DecZBuf]

		adc	ebx,[IntZBuf]
			mov	esi,eax

			and	esi,0FF00h

		push eax
		mov eax,[PtrZBuffer1]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp1

		push ebx
		mov ebx,[PtrColor1]
		mov	ah,[esi+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog_1]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp1:
		add	edx,[AddGour1]
			and	eax,0FFh

			add	eax,edx
		add	ebp,[DecZBuf1]

		adc	ebx,[IntZBuf1]
			mov	esi,eax

			add	edi,2
			and	esi,0FF00h

			rol	al,cl

			dec	ecx
			jne	@@Each_X


@@Add_One:
		push eax
		mov eax,[PtrZBuffer4]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@End_X

		push ebx
		mov ebx,[PtrColor4]
		mov	ah,[esi+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog4]
		mov	[edi+ebx],ah
		pop ebx

			jmp	@@End_X
@@Pas_Add1:
			je	@@End_X
@@Each_X2:
		push eax
		mov eax,[PtrZBuffer2]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp2

		push ebx
		mov ebx,[PtrColor2]
		mov	ah,[esi+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog2]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp2:
			and	eax,0FFh
		add	edx,[AddGour2]

			add	eax,edx
		add	ebp,[DecZBuf2]

		adc	ebx,[IntZBuf2]
			mov	esi,eax

			and	esi,0FF00h

		push eax
		mov eax,[PtrZBuffer3]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp3

		push ebx
		mov ebx,[PtrColor3]
		mov	ah,[esi+ebx]
		pop ebx

		push ebx
		mov ebx,[PtrLog2_1]
		mov	[edi+ebx],ah
		pop ebx

@@NoDisp3:
		add	edx,[AddGour3]
			and	eax,0FFh

			add	eax,edx
		add	ebp,[DecZBuf3]

		adc	ebx,[IntZBuf3]
			mov	esi,eax

			add	edi,2
			and	esi,0FF00h

			rol	al,cl

			dec	ecx
			jne	@@Each_X2

@@End_X:
			pop	ebx
			pop	esi
			pop	ebp
			pop	edx

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_DitherTableNZW	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_GouraudFogNZW	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[PtrZBuffer]
			mov	ebx,[Log]

			mov	[PtrZBufferPatch],eax

			mov	[PtrLog],ebx
			mov	eax,[Fill_Gouraud_XSlope]

			shr	eax,16
			mov	ebx,[Fill_Gouraud_XSlope]

			shl	ebx,16
			mov	[IntGour],al

			mov	[DecGour],ebx
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	[IntZBuf],eax

			mov	[DecZBuf],ebx
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebp
			push	esi

			push	ebx
			push	edx

			mov	eax,ebx
			mov	ebx,[Fill_Gouraud_XSlope]

			and	eax,0FFFFh
			mov	dl,[edi]	; Load 1st dst cache line
			xor	eax,0FFFFh

			cmp	ecx,16
			jbe	@@FastInit

			imul	ebx

			shl	edx,16
			push	edi

			shr	eax,16
			and	edi,not 31

			or	eax,edx
			lea	edx,[edi+ecx]

			add	esi,eax
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
			mov	ebp,[Fill_CurZBufMin]

			shl	ebp,32-8
			sub	edi,eax

			mov	ebx,[Fill_CurZBufMin]
			mov	eax,esi

			shr	ebx,8

			shr	eax,16

			shl	esi,16
			and	eax,0FFh

				; *** Gouraud loop ***
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],bx
		pop eax

			jb	@@NoDisp
			mov	dl,[eax+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],dl
		pop ebx

@@NoDisp:
			inc	edi
		add	esi,[DecGour]

		adc	al,[IntGour]
		add	ebp,[DecZBuf]

		adc	ebx,[IntZBuf]
			dec	ecx

			jne	@@Each_X
@@End_X:

			pop	edx
			pop	ebx

			pop	esi
			pop	ebp

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_GouraudFogNZW	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_DitherFogNZW	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			add	eax,ebp
			mov	edi,[Fill_CurOffLine]

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurGouraudMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			push	ebx

			mov	eax,[Fill_Gouraud_XSlope]
			mov	ebx,[Fill_ZBuf_XSlope]

			sar	eax,8

			shl	ebx,32-8
			and	eax,0FFFFh

			or	eax,ebx

			mov	[DecZBuf_AddGour],eax
			mov	[DecZBuf_AddGour1],eax

			mov	[DecZBuf_AddGour2],eax
			mov	[DecZBuf_AddGour3],eax

			mov	ebx,[Log]
			mov	eax,[Fill_ZBuf_XSlope]

			mov	[PtrLog],ebx
			mov	[PtrLog2],ebx

			mov	[PtrLog4],ebx
			inc	ebx

			mov	[PtrLog_1],ebx
			mov	[PtrLog2_1],ebx

			shr	eax,8

			mov	[IntZBuf],eax
			mov	[IntZBuf1],eax

			mov	[IntZBuf2],eax
			mov	[IntZBuf3],eax

			pop	ebx
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_Gouraud_LeftSlope]

			add	esi,eax
			push	ebx

			mov	eax,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


				; *** Gouraud dither loop ***
			push	edx
			push	ebp
			push	ebx
			push	esi

			mov	dl,[edi]	; Load 1st dst cache line

			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache
			jb	@@NoPreload

				; *** Load dst in WB cache ***
			push	edi
			lea	edx,[edi+ecx]

			and	edi,not 31
			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edi
@@NoPreload:
			shr	esi,8
			mov	ebx,[Fill_CurZBufMin]

			and	esi,0FFFFh
			mov	ebp,ebx

			shr	ebx,8
			mov	eax,[Log]

			shl	ebp,32-8
			sub	edi,eax

			mov	eax,esi
			or	esi,ebp

			mov	ebp,[PtrZBuffer]
			and	eax,0FFh

			add	eax,esi
			xor	edx,edx

			mov	dl,ah

			shr	ecx,1
			jnc	@@Pas_Add1
			je	@@Add_One

			rol	al,cl
@@Each_X:
			cmp	word ptr [edi*2+ebp],bx
			jb	@@NoDisp0
			mov	dl,[edx+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],dl
		pop ebx

@@NoDisp0:
			and	eax,0FFh
	add	esi,[DecZBuf_AddGour]

		adc	ebx,[IntZBuf]
			add	eax,esi

			mov	dl,ah

			cmp	word ptr [edi*2+ebp+2],bx
			jb	@@NoDisp1
			mov	dl,[edx+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog_1]
		mov	[edi+ebx],dl
		pop ebx

@@NoDisp1:
			and	eax,0FFh
	add	esi,[DecZBuf_AddGour1]

		adc	ebx,[IntZBuf1]
			add	eax,esi

			add	edi,2
			mov	dl,ah

			rol	al,cl

			dec	ecx
			jne	@@Each_X
@@Add_One:
			cmp	word ptr [edi*2+ebp],bx
			jb	@@End_X
			mov	dl,[edx+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog4]
		mov	[edi+ebx],dl
		pop ebx

			jmp	@@End_X
@@Pas_Add1:
			je	@@End_X
@@Each_X2:
			cmp	word ptr [edi*2+ebp],bx
			jb	@@NoDisp2
			mov	dl,[edx+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog2]
		mov	[edi+ebx],dl
		pop ebx

@@NoDisp2:
			and	eax,0FFh
	add	esi,[DecZBuf_AddGour2]

		adc	ebx,[IntZBuf2]
			add	eax,esi

			mov	dl,ah

			cmp	word ptr [edi*2+ebp+2],bx
			jb	@@NoDisp3
			mov	dl,[edx+Fill_Logical_Palette]

		push ebx
		mov ebx,[PtrLog2_1]
		mov	[edi+ebx],dl
		pop ebx

@@NoDisp3:
			and	eax,0FFh
	add	esi,[DecZBuf_AddGour3]

		adc	ebx,[IntZBuf3]
			add	eax,esi

			add	edi,2
			mov	dl,ah

			rol	al,cl

			dec	ecx
			jne	@@Each_X2
@@End_X:
			pop	esi
			pop	ebx
			pop	ebp
			pop	edx

@@EndScanLine:		pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurGouraudMin],esi

			jmp	Triangle_ReadNextEdge
Filler_DitherFogNZW	ENDP

_TEXT			ENDS

;			The
			End
