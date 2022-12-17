; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                       (Adeline Software Intl.)                     ***
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
PUBLIC	C		First_POLYTEXTUREZPatch
PUBLIC	C		Last_POLYTEXTUREZPatch

PUBLIC	C		Filler_TextureZ
PUBLIC	C		Filler_TextureZFlat
PUBLIC	C		Filler_TextureZChromaKey
PUBLIC	C		Filler_TextureZFlatChromaKey
PUBLIC	C		Filler_TextureZFog
PUBLIC	C		Filler_TextureZChromaKeyFog
PUBLIC	C		Filler_TextureZZBuf
PUBLIC	C		Filler_TextureZChromaKeyZBuf
PUBLIC	C		Filler_TextureZFlatZBuf
PUBLIC	C		Filler_TextureZFlatChromaKeyZBuf
PUBLIC	C		Filler_TextureZFogZBuf
PUBLIC	C		Filler_TextureZChromaKeyFogZBuf
PUBLIC	C		Filler_TextureZNZW
PUBLIC	C		Filler_TextureZChromaKeyNZW
PUBLIC	C		Filler_TextureZFlatNZW
PUBLIC	C		Filler_TextureZFlatChromaKeyNZW
PUBLIC	C		Filler_TextureZFogNZW
PUBLIC	C		Filler_TextureZChromaKeyFogNZW




;					*****************
;					*** VARIABLES ***
;					*****************

;					*** PUBLIC DATA ***
PUBLIC	C		Fill_VLTextureZ1
PUBLIC	C		Fill_VLTextureZ2
PUBLIC	C		Fill_VLTextureZ3
PUBLIC	C		Fill_VLTextureZ4
PUBLIC	C		Fill_VLTextureZ5
PUBLIC	C		Fill_VLTextureZ6
PUBLIC	C		Fill_VLTextureZ7
PUBLIC	C		Fill_VLTextureZ8
PUBLIC	C		Fill_VLTextureZ9
PUBLIC	C		Fill_VLTextureZ10
PUBLIC	C		Fill_VLTextureZ11
PUBLIC	C		Fill_VLTextureZ12
PUBLIC	C		Fill_VLTextureZ13
PUBLIC	C		Fill_VLTextureZ14
PUBLIC	C		Fill_VLTextureZ15
PUBLIC	C		Fill_VLTextureZ16
PUBLIC	C		Fill_VLTextureZ17
PUBLIC	C		Fill_VLTextureZ18


PUBLIC	C		Fill_Init_Perspective
PUBLIC	C		Fill_Loop_Perspective


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

Extrn	C		Fill_CurWMin		:	DWORD
Extrn	C		Fill_W_LeftSlope	:	DWORD
Extrn	C		Fill_W_XSlope		:	DWORD

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

Extrn	C		F_1			:	DWORD
Extrn	C		F_256			:	DWORD

Extrn	C		Fill_UseFPU		:	BYTE





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
Fill_Init_Perspective	dd	Offset Choose_Init
Fill_Loop_Perspective	dd	Offset Choose_Loop

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
First_POLYTEXTUREZPatch	Label	DWORD



Choose_Init		PROC
			mov	[Fill_Init_Perspective],Offset Init_PerspINT

			cmp	[Fill_UseFPU],TRUE
			jne	@@End

			mov	[Fill_Init_Perspective],Offset Init_PerspFPU
@@End:
			jmp	[Fill_Init_Perspective]
Choose_Init		ENDP



Choose_Loop		PROC
			mov	[Fill_Loop_Perspective],Offset Loop_PerspINT

			cmp	[Fill_UseFPU],TRUE
			jne	@@End

			mov	[Fill_Loop_Perspective],Offset Loop_PerspFPU
@@End:
			jmp	[Fill_Loop_Perspective]
Choose_Loop		ENDP







; ╔═════════════════════════════════════════════════════════════╗
; ║ Initialize the gradiants of a scanline                      ║
; ╚═════════════════════════════════════════════════════════════╝

Init_PerspINT		PROC
			mov	ebx,[Fill_Cur_W]		; Current W

			test	ebx,ebx
			je	@@W_Nul1
			cmp	ebx,-1
			je	@@W_Nul1
			cmp	ebx,1
			je	@@W_Nul1
@@Return_Error1:
			mov	eax,-1
			xor	edx,edx

			idiv	ebx

			mov	ebx,eax
			mov	eax,esi
			imul	ebx

			shr	eax,24
			mov	[Fill_Cur_MapUOverW],esi

			shl	edx,8
			mov	[Fill_Cur_MapVOverW],ebp

			or	eax,edx

			mov	[Fill_Cur_MapU],eax		; Current U
			mov	eax,ebp

			imul	ebx

			shr	eax,24

			shl	edx,8

			or	eax,edx

			mov	[Fill_Cur_MapV],eax		; Current V

			ret
@@W_Nul1:
			mov	ebx,2
			jmp	@@Return_Error1
Init_PerspINT		ENDP





; ╔═════════════════════════════════════════════════════════════╗
; ║ Initialize the gradiants of a section of PERSP_VALUE pixels ║
; ╚═════════════════════════════════════════════════════════════╝

Loop_PerspINT		PROC
			mov	edx,[Fill_W_XSlope]
			mov	ebx,[Fill_Cur_W]

			shl	edx,PERSP_SHIFT
			mov	eax,-1

			add	ebx,edx
			je	@@W_Nul
			cmp	ebx,-1
			je	@@W_Nul
			cmp	ebx,1
			je	@@W_Nul
@@Return_Error:
			mov	[Fill_Next_W],ebx		; Next W
			xor	edx,edx

			idiv	ebx

			mov	ebx,eax
			mov	eax,[Fill_MapU_XSlope]

			shl	eax,PERSP_SHIFT
			mov	edx,[Fill_Cur_MapUOverW]

			add	eax,edx

			mov	[Fill_Next_MapUOverW],eax

			imul	ebx

			shr	eax,24
			mov	esi,[Fill_MapV_XSlope]

			shl	edx,8
			mov	ebp,[Fill_Cur_MapVOverW]

			shl	esi,PERSP_SHIFT
			or	eax,edx

			mov	[Fill_Next_MapU],eax		; EBP = Next MapU
			add	esi,ebp

			mov	[Fill_Next_MapVOverW],esi
			mov	eax,esi

			imul	ebx

			shr	eax,24

			shl	edx,8

			or	eax,edx

			mov	[Fill_Next_MapV],eax		; EAX = Next MapV

			ret
@@W_Nul:
			mov	ebx,2
			jmp	@@Return_Error
Loop_PerspINT		ENDP






; ╔═════════════════════════════════════════════════════════════╗
; ║ Initialize the gradiants of a scanline                      ║
; ╚═════════════════════════════════════════════════════════════╝

Init_PerspFPU		PROC
			mov	ebx,[Fill_Cur_W]		; Current W

			test	ebx,ebx
			je	@@W_Nul1
			cmp	ebx,-1
			je	@@W_Nul1
			cmp	ebx,1
			je	@@W_Nul1
@@Return_Error1:
			fild	[Fill_Cur_W]	; W

			mov	[Fill_Cur_MapUOverW],esi
			mov	edx,[Fill_W_XSlope]

			shl	edx,PERSP_SHIFT
			mov	[Fill_Cur_MapVOverW],ebp

			fdivr	[F_256]		; 1/W

			; Here, we precalc an array in the stack. This array
			; contains valid Cur_W values

			pop	ebp		; EBP = Return address
			lea	eax,[ecx+PERSP_VALUE-1]

			shr	eax,PERSP_SHIFT-2
			xor	esi,esi

			and	eax,not 11b

			sub	esp,eax
@@CreateStack:
			add	ebx,edx
			je	@@W_Nul
			cmp	ebx,-1
			je	@@W_Nul
			cmp	ebx,1
			je	@@W_Nul
@@Return_Error:
			mov	[esp+esi],ebx
			add	esi,4

			cmp	esi,eax
			jne	@@CreateStack

			pop	ebx

			mov	[Fill_Cur_W],ebx

			fild	[Fill_Cur_MapUOverW]; MapU/Z  1/W

			fild	[Fill_Cur_MapVOverW]; MapV/Z  MapU/Z  1/W

			fxch	st(1)		; MapU/Z  MapV/Z  1/W

			fmul	st,st(2)	; MapU  MapV/Z  1/W

			fild	[Fill_Cur_W]	; W' MapU  MapV/Z  1/W

			fxch	st(2)		; MapV/Z  MapU  W' 1/W

			fmulp	st(3),st	; MapU  W' MapV

			fistp	[Fill_Cur_MapU]	; W' MapV

			fxch	st(1)		; MapV  W'

			fistp	[Fill_Cur_MapV]	;

			fdivr	[F_256]		; 1/W'

			jmp	ebp		; RET
@@W_Nul1:
			mov	ebx,2
			mov	[Fill_Cur_W],ebx
			jmp	@@Return_Error1
@@W_Nul:
			mov	ebx,2
			jmp	@@Return_Error
Init_PerspFPU		ENDP








; ╔═════════════════════════════════════════════════════════════╗
; ║ Initialize the gradiants of a section of PERSP_VALUE pixels ║
; ╚═════════════════════════════════════════════════════════════╝

Loop_PerspFPU		PROC
			mov	eax,[Fill_MapU_XSlope]
			mov	esi,[Fill_MapV_XSlope]

			shl	eax,PERSP_SHIFT
			mov	edx,[Fill_Cur_MapUOverW]

			add	eax,edx
			mov	ebp,[Fill_Cur_MapVOverW]

			shl	esi,PERSP_SHIFT
			mov	[Fill_Next_MapUOverW],eax

			add	esi,ebp
			pop	ebp		; EBP = Return address

			mov	[Fill_Next_MapVOverW],esi

			fild	[Fill_Next_MapUOverW]; U/Z  1/W

			fild	[Fill_Next_MapVOverW]; V/Z  U/Z  1/W

			fxch	st(1)		; U/Z  V/Z  1/W

			fmul	st,st(2)	; U  V/Z  1/W
			fxch	st(1)		; V/Z  U  1/W

			; **

			fmulp	st(2),st	; U  V

			fistp	[Fill_Next_MapU]

			fistp	[Fill_Next_MapV]

			cmp	ecx,PERSP_VALUE
			jbe	@@LessPERSP_VALUE

			fild	dword ptr [esp]	; W

			pop	ebx

			mov	[Fill_Next_W],ebx	; Next W

			fdivr	[F_256]		; 1/W
@@LessPERSP_VALUE:
			jmp	ebp		; RET
Loop_PerspFPU		ENDP














; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZ		PROC
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
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ1::	add	edi,SIZE_VIDEOLINE

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
			pop	ebx

			mov	[Fill_CurWMin],eax

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
			push	edi

			imul	ebx

			shl	edx,16
			and	edi,not 31

			shr	eax,16

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
					; *** Init Scanline ***
			call	[Fill_Init_Perspective]
@@FastFill:
			call	[Fill_Loop_Perspective]

			mov	ebp,[Fill_Next_MapU]
			mov	ebx,[Fill_Cur_MapU]		; EBX = CurMapU

			sub	ebp,ebx
			mov	edx,[Fill_Cur_MapV]		; EDX = CurMapV

			sar	ebp,PERSP_SHIFT			; EBP = IncMapU
			mov	eax,[Fill_Next_MapV]

			sub	eax,edx
			and	ebp,0FFFFh

			sar	eax,PERSP_SHIFT			; EAX = IncMapV
			and	ebx,0FFFFh

			shl	eax,16

			shl	edx,16
			or	ebp,eax				; EBP = IncV | IncU

			or	ebx,edx				; EBX = MapV | MapU
			mov	edx,[RepMask]

			mov	esi,[PtrMap]
			mov	eax,ebx

			shr	eax,16

			mov	al,bh

			and	eax,edx

			cmp	ecx,PERSP_VALUE
			jge	@@DoPERSP_VALUE

@@LoopX:
			mov	al,[eax+esi]
			add	ebx,ebp

			mov	[edi],al
			mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh

			and	eax,edx
			dec	ecx

			jne	@@LoopX

			jmp	@@End_X

@@DoPERSP_VALUE:
			push	ecx
			add	ebx,ebp

			mov	ecx,ebx

			shr	ecx,16
			mov	al,[eax+esi]

			mov	[edi],al
			lea	eax,[ebx+ebp]
AA			=	1
REPT	(PERSP_VALUE-4)/2
			shr	eax,16
			mov	cl,bh

			add	ebx,ebp
			and	ecx,edx

			mov	al,bh
			add	ebx,ebp

			mov	cl,[ecx+esi]
			and	eax,edx

			mov	[edi+AA],cl
			mov	ecx,ebx

			shr	ecx,16
			mov	al,[eax+esi]

			mov	[edi+AA+1],al
			lea	eax,[ebx+ebp]
AA			=	AA+2
ENDM
			shr	eax,16
			mov	cl,bh

			add	ebx,ebp
			and	ecx,edx

			mov	al,bh
			add	ebx,ebp

			mov	cl,[ecx+esi]
			and	eax,edx

			mov	[edi+AA],cl
			mov	ecx,ebx

			shr	ecx,16
			mov	al,[eax+esi]

			mov	[edi+AA+1],al
			mov	cl,bh

			and	ecx,edx
			add	edi,PERSP_VALUE

			mov	al,[ecx+esi]
			pop	ecx

			mov	[edi+AA+2-PERSP_VALUE],al
			mov	eax,[Fill_Next_MapU]

			mov	ebx,[Fill_Next_MapV]
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
Filler_TextureZ		ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlat	PROC
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
			mov	eax,[Fill_Color]

			mov	dword ptr [@@PtrCLUT+2],eax
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ2::	add	edi,SIZE_VIDEOLINE

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
			pop	ebx

			mov	[Fill_CurWMin],eax

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

			sar	ebp,PERSP_SHIFT		; EBP = IncMapU
			mov	eax,[Fill_Next_MapV]	; EAX = NextMapV

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

			mov	esi,[PtrMap]
			mov	al,bh

			and	eax,edx
			push	ecx

			cmp	ecx,PERSP_VALUE
			jle	@@LessPERSP_VALUE

			mov	ecx,-PERSP_VALUE			; Rasterize PERSP_VALUE pix
			add	edi,PERSP_VALUE

			xor	edx,edx
			jmp	@@LoopX
@@LessPERSP_VALUE:
			lea	edi,[edi+ecx]		; Rasterize less than PERSP_VALUE pix
			xor	ecx,-1

			inc	ecx
			xor	edx,edx
@@LoopX:
			mov	dl,[eax+esi]
			lea	eax,[ebx+ebp]

			shr	eax,16
			add	ebx,ebp

@@PtrCLUT:		mov	dl,[edx+12345678h]
			mov	al,bh

			mov	[edi+ecx],dl
@@RepMask:		and	eax,12345678h

			inc	ecx
			jne	@@LoopX

			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			mov	ebx,[Fill_Next_MapV]
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
Filler_TextureZFlat	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKey PROC
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
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ3::	add	edi,SIZE_VIDEOLINE

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
			pop	ebx

			mov	[Fill_CurWMin],eax

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

			sar	ebp,PERSP_SHIFT		; EBP = IncMapU
			mov	eax,[Fill_Next_MapV]	; EAX = NextMapV

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

			mov	esi,[PtrMap]
			mov	al,bh

			and	eax,edx
			push	ecx

			cmp	ecx,PERSP_VALUE
			jle	@@LoopX
			mov	ecx,PERSP_VALUE
@@LoopX:
			mov	al,[eax+esi]
			add	ebx,ebp

			test	al,al
			je	@@NotVisible

			mov	[edi],al
@@NotVisible:		mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh

			and	eax,edx

			dec	ecx
			jne	@@LoopX

			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			mov	ebx,[Fill_Next_MapV]
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
Filler_TextureZChromaKey ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlatChromaKey PROC
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
			mov	eax,[Fill_Color]

			mov	dword ptr [@@PtrCLUT+2],eax
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	dword ptr [@@RepMask1+1],eax

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ4::	add	edi,SIZE_VIDEOLINE

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
			pop	ebx

			mov	[Fill_CurWMin],eax

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
			mov	eax,[Fill_Next_MapV]	; EAX = NextMapV

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

			mov	esi,[PtrMap]
			mov	al,bh

			and	eax,edx
			push	ecx

			cmp	ecx,PERSP_VALUE
			jle	@@LessPERSP_VALUE

			mov	ecx,-PERSP_VALUE			; Rasterize PERSP_VALUE pix
			add	edi,PERSP_VALUE

			xor	edx,edx
			jmp	@@LoopX
@@LessPERSP_VALUE:
			lea	edi,[edi+ecx]		; Rasterize less than PERSP_VALUE pix
			xor	ecx,-1

			inc	ecx
			xor	edx,edx
@@LoopX:
			mov	dl,[eax+esi]
			lea	eax,[ebx+ebp]

			shr	eax,16
			add	ebx,ebp

			test	dl,dl
			je	@@NotVisible
@@Visible:
@@PtrCLUT:		mov	dl,[edx+12345678h]
			mov	al,bh

			mov	[edi+ecx],dl
@@RepMask:		and	eax,12345678h

			inc	ecx
			jne	@@LoopX
			jmp	@@EndDrawPERSP_VALUE

@@LoopX1:
			mov	dl,[eax+esi]
			lea	eax,[ebx+ebp]

			shr	eax,16
			add	ebx,ebp

			test	dl,dl
			jne	@@Visible
@@NotVisible:
			mov	al,bh

@@RepMask1:		and	eax,12345678h

			inc	ecx
			jne	@@LoopX1

@@EndDrawPERSP_VALUE:
			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			mov	ebx,[Fill_Next_MapV]
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
Filler_TextureZFlatChromaKey ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFog	PROC
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
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ5::	add	edi,SIZE_VIDEOLINE

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
			pop	ebx

			mov	[Fill_CurWMin],eax

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

			mov	esi,[PtrMap]
			push	ecx

			mov	al,bh
			xor	ecx,-1

			and	eax,edx
			inc	ecx

			xor	edx,edx

			cmp	ecx,-PERSP_VALUE
			jnle	@@LoopXStart
			mov	ecx,-PERSP_VALUE
@@LoopXStart:		sub	edi,ecx
@@LoopX:
			mov	dl,[eax+esi]
			lea	eax,[ebx+ebp]

			shr	eax,16
			add	ebx,ebp

			mov	dl,[Fill_Logical_Palette+edx]
			mov	al,bh

			mov	[edi+ecx],dl
@@RepMask:		and	eax,12345678h

			inc	ecx
			jne	@@LoopX

			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			mov	ebx,[Fill_Next_MapV]
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
Filler_TextureZFog	ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKeyFog PROC
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
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ6::	add	edi,SIZE_VIDEOLINE

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
			pop	ebx

			mov	[Fill_CurWMin],eax

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

			mov	esi,[PtrMap]
			mov	al,bh

			and	eax,edx
			xor	edx,edx
			push	ecx

			cmp	ecx,PERSP_VALUE
			jle	@@LoopX
			mov	ecx,PERSP_VALUE
@@LoopX:
			mov	dl,[eax+esi]
			add	ebx,ebp

			test	dl,dl
			je	@@NotVisible

			mov	dl,[Fill_Logical_Palette+edx]

			mov	[edi],dl
@@NotVisible:		mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh

@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX

			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			mov	ebx,[Fill_Next_MapV]
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
Filler_TextureZChromaKeyFog ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZZBuf	PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax
			mov	dword ptr [@@PtrZBuffer1+4],eax

			mov	dword ptr [@@PtrLog+2],ebx
			mov	eax,[PtrMap]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@DecZBuf+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	eax,[Fill_CurZBufMin]

			pop	ebx
			mov	[Fill_CurZBuf],eax

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ7::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp
@@PtrZBuffer1:		mov	[edi*2+12345678h],dx
@@PtrMap:		mov	al,[eax+12345678h]

@@PtrLog:		mov	[edi+12345678h],al
@@NoDisp:
			lea	eax,[ebx+ebp]
			add	ebx,ebp

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX


			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZZBuf	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKeyZBuf PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax
			mov	dword ptr [@@PtrZBuffer1+4],eax

			mov	dword ptr [@@PtrLog+2],ebx
			mov	eax,[PtrMap]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@DecZBuf+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	eax,[Fill_CurZBufMin]

			pop	ebx
			mov	[Fill_CurZBuf],eax

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ8::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrMap:		mov	al,[eax+12345678h]

			test	al,al
			je	@@NoDisp

@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp

@@PtrZBuffer1:		mov	[edi*2+12345678h],dx
@@PtrLog:		mov	[edi+12345678h],al

@@NoDisp:
			lea	eax,[ebx+ebp]
			add	ebx,ebp

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX


			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZChromaKeyZBuf ENDP









; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlatZBuf	PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax
			mov	dword ptr [@@PtrZBuffer1+4],eax

			mov	dword ptr [@@PtrZBuffer2+4],eax
			mov	dword ptr [@@PtrLog+2],ebx

			mov	eax,[PtrMap]
			mov	ebx,[Fill_ZBuf_XSlope]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@IntZBuf2+2],eax
			mov	dword ptr [@@DecZBuf+2],ebx

			mov	dword ptr [@@DecZBuf2+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	dword ptr [@@RepMask2+1],eax

			mov	eax,[Fill_CurZBufMin]
			pop	ebx

			mov	[Fill_CurZBuf],eax
			mov	eax,[Fill_Color]

			mov	dword ptr [@@PtrCLUT+2],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ9::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp
@@Disp:
@@PtrMap:		mov	al,[eax+12345678h]
			add	ebx,ebp

			and	eax,0FFh

@@PtrZBuffer1:		mov	[edi*2+12345678h],dx
@@PtrCLUT:		mov	al,[eax+12345678h]

@@PtrLog:		mov	[edi+12345678h],al
			mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX

			jmp	@@EndPERSP_VALUE

@@LoopX2:
@@PtrZBuffer2:		cmp	[edi*2+12345678h],dx
			jae	@@Disp
@@NoDisp:
			lea	eax,[ebx+ebp]
			add	ebx,ebp

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf2:		add	esi,12345678h

@@IntZBuf2:		adc	edx,12345678h
@@RepMask2:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX2
@@EndPERSP_VALUE:
			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZFlatZBuf	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlatChromaKeyZBuf PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax
			mov	dword ptr [@@PtrZBuffer1+4],eax

			mov	dword ptr [@@PtrLog+2],ebx
			mov	eax,[PtrMap]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@DecZBuf+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	eax,[Fill_CurZBufMin]

			mov	[Fill_CurZBuf],eax
			mov	eax,[Fill_Color]

			mov	dword ptr [@@PtrCLUT+2],eax
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ10::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrMap:		mov	al,[eax+12345678h]
			add	ebx,ebp

			and	eax,0FFh
			je	@@NoDisp

@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp

@@PtrZBuffer1:		mov	[edi*2+12345678h],dx
@@PtrCLUT:		mov	al,[eax+12345678h]

@@PtrLog:		mov	[edi+12345678h],al
@@NoDisp:		mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX


			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZFlatChromaKeyZBuf ENDP











; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFogZBuf	PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax
			mov	dword ptr [@@PtrZBuffer1+4],eax

			mov	dword ptr [@@PtrZBuffer2+4],eax
			mov	dword ptr [@@PtrLog+2],ebx

			mov	eax,[PtrMap]
			mov	ebx,[Fill_ZBuf_XSlope]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@IntZBuf2+2],eax
			mov	dword ptr [@@DecZBuf+2],ebx

			mov	dword ptr [@@DecZBuf2+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	dword ptr [@@RepMask2+1],eax

			mov	eax,[Fill_CurZBufMin]
			pop	ebx

			mov	[Fill_CurZBuf],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ11::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp
@@Disp:
@@PtrMap:		mov	al,[eax+12345678h]
			add	ebx,ebp

			and	eax,0FFh

@@PtrZBuffer1:		mov	[edi*2+12345678h],dx
			mov	al,[eax+Fill_Logical_Palette]

@@PtrLog:		mov	[edi+12345678h],al
			mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX

			jmp	@@EndPERSP_VALUE

@@LoopX2:
@@PtrZBuffer2:		cmp	[edi*2+12345678h],dx
			jae	@@Disp
@@NoDisp:
			lea	eax,[ebx+ebp]
			add	ebx,ebp

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf2:		add	esi,12345678h

@@IntZBuf2:		adc	edx,12345678h
@@RepMask2:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX2
@@EndPERSP_VALUE:
			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZFogZBuf	ENDP









; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKeyFogZBuf PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax
			mov	dword ptr [@@PtrZBuffer1+4],eax

			mov	dword ptr [@@PtrLog+2],ebx
			mov	eax,[PtrMap]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@DecZBuf+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	eax,[Fill_CurZBufMin]

			mov	[Fill_CurZBuf],eax
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ12::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrMap:		mov	al,[eax+12345678h]
			add	ebx,ebp

			and	eax,0FFh
			je	@@NoDisp

@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp

@@PtrZBuffer1:		mov	[edi*2+12345678h],dx
			mov	al,[eax+Fill_Logical_Palette]

@@PtrLog:		mov	[edi+12345678h],al
@@NoDisp:		mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX


			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZChromaKeyFogZBuf ENDP











; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZNZW	PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax

			mov	dword ptr [@@PtrLog+2],ebx
			mov	eax,[PtrMap]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@DecZBuf+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	eax,[Fill_CurZBufMin]

			pop	ebx
			mov	[Fill_CurZBuf],eax

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ13::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp
@@PtrMap:		mov	al,[eax+12345678h]

@@PtrLog:		mov	[edi+12345678h],al
@@NoDisp:
			lea	eax,[ebx+ebp]
			add	ebx,ebp

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX


			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZNZW	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKeyNZW PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax

			mov	dword ptr [@@PtrLog+2],ebx
			mov	eax,[PtrMap]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@DecZBuf+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	eax,[Fill_CurZBufMin]

			pop	ebx
			mov	[Fill_CurZBuf],eax

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ14::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrMap:		mov	al,[eax+12345678h]

			test	al,al
			je	@@NoDisp

@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp

@@PtrLog:		mov	[edi+12345678h],al
@@NoDisp:
			lea	eax,[ebx+ebp]
			add	ebx,ebp

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX


			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZChromaKeyNZW ENDP









; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlatNZW	PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax
			mov	dword ptr [@@PtrZBuffer2+4],eax

			mov	dword ptr [@@PtrLog+2],ebx
			mov	eax,[PtrMap]

			mov	ebx,[Fill_ZBuf_XSlope]
			mov	dword ptr [@@PtrMap+2],eax

			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@IntZBuf2+2],eax
			mov	dword ptr [@@DecZBuf+2],ebx

			mov	dword ptr [@@DecZBuf2+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	dword ptr [@@RepMask2+1],eax

			mov	eax,[Fill_CurZBufMin]
			pop	ebx

			mov	[Fill_CurZBuf],eax
			mov	eax,[Fill_Color]

			mov	dword ptr [@@PtrCLUT+2],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ15::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp
@@Disp:
@@PtrMap:		mov	al,[eax+12345678h]
			add	ebx,ebp

			and	eax,0FFh

@@PtrCLUT:		mov	al,[eax+12345678h]

@@PtrLog:		mov	[edi+12345678h],al
			mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX

			jmp	@@EndPERSP_VALUE

@@LoopX2:
@@PtrZBuffer2:		cmp	[edi*2+12345678h],dx
			jae	@@Disp
@@NoDisp:
			lea	eax,[ebx+ebp]
			add	ebx,ebp

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf2:		add	esi,12345678h

@@IntZBuf2:		adc	edx,12345678h
@@RepMask2:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX2
@@EndPERSP_VALUE:
			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZFlatNZW	ENDP







; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlatChromaKeyNZW PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax

			mov	dword ptr [@@PtrLog+2],ebx
			mov	eax,[PtrMap]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@DecZBuf+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	eax,[Fill_CurZBufMin]

			mov	[Fill_CurZBuf],eax
			mov	eax,[Fill_Color]

			mov	dword ptr [@@PtrCLUT+2],eax
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ16::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrMap:		mov	al,[eax+12345678h]
			add	ebx,ebp

			and	eax,0FFh
			je	@@NoDisp

@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp

@@PtrCLUT:		mov	al,[eax+12345678h]

@@PtrLog:		mov	[edi+12345678h],al
@@NoDisp:		mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX


			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZFlatChromaKeyNZW ENDP











; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFogNZW	PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax

			mov	dword ptr [@@PtrZBuffer2+4],eax
			mov	dword ptr [@@PtrLog+2],ebx

			mov	eax,[PtrMap]
			mov	ebx,[Fill_ZBuf_XSlope]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@IntZBuf2+2],eax
			mov	dword ptr [@@DecZBuf+2],ebx

			mov	dword ptr [@@DecZBuf2+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	dword ptr [@@RepMask2+1],eax

			mov	eax,[Fill_CurZBufMin]
			pop	ebx

			mov	[Fill_CurZBuf],eax
			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ17::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp
@@Disp:
@@PtrMap:		mov	al,[eax+12345678h]
			add	ebx,ebp

			and	eax,0FFh

			mov	al,[eax+Fill_Logical_Palette]

@@PtrLog:		mov	[edi+12345678h],al
			mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX

			jmp	@@EndPERSP_VALUE

@@LoopX2:
@@PtrZBuffer2:		cmp	[edi*2+12345678h],dx
			jae	@@Disp
@@NoDisp:
			lea	eax,[ebx+ebp]
			add	ebx,ebp

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf2:		add	esi,12345678h

@@IntZBuf2:		adc	edx,12345678h
@@RepMask2:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX2
@@EndPERSP_VALUE:
			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZFogNZW	ENDP









; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKeyFogNZW PROC
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

			mov	dword ptr [@@PtrZBuffer+4],eax

			mov	dword ptr [@@PtrLog+2],ebx
			mov	eax,[PtrMap]

			mov	dword ptr [@@PtrMap+2],eax
			mov	eax,[Fill_ZBuf_XSlope]

			shr	eax,8
			mov	ebx,[Fill_ZBuf_XSlope]

			shl	ebx,32-8
			mov	dword ptr [@@IntZBuf+2],eax

			mov	dword ptr [@@DecZBuf+2],ebx
			mov	eax,[RepMask]

			mov	dword ptr [@@RepMask+1],eax
			mov	eax,[Fill_CurZBufMin]

			mov	[Fill_CurZBuf],eax
			pop	ebx

			jmp	@@EnterLoop
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
Fill_VLTextureZ18::	add	edi,SIZE_VIDEOLINE

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
			mov	ebx,[Fill_ZBuf_LeftSlope]

			mov	[Fill_CurWMin],eax
			mov	eax,[Fill_CurZBufMin]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
			mov	[Fill_CurZBuf],eax
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

			sub	edi,eax			; EDI becomes an offset instead of a pointer
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

			mov	esi,[Fill_CurZBuf]
			mov	al,bh

			shl	esi,32-8
			and	eax,edx

			mov	edx,[Fill_CurZBuf]
			push	ecx

			shr	edx,8
			cmp	ecx,PERSP_VALUE

			jle	@@LoopX
			mov	ecx,PERSP_VALUE

@@LoopX:
@@PtrMap:		mov	al,[eax+12345678h]
			add	ebx,ebp

			and	eax,0FFh
			je	@@NoDisp

@@PtrZBuffer:		cmp	[edi*2+12345678h],dx
			jb	@@NoDisp

			mov	al,[eax+Fill_Logical_Palette]

@@PtrLog:		mov	[edi+12345678h],al
@@NoDisp:		mov	eax,ebx

			shr	eax,16
			inc	edi

			mov	al,bh
@@DecZBuf:		add	esi,12345678h

@@IntZBuf:		adc	edx,12345678h
@@RepMask:		and	eax,12345678h

			dec	ecx
			jne	@@LoopX


			pop	ecx
			mov	eax,[Fill_Next_MapU]

			cmp	ecx,PERSP_VALUE
			jle	@@End_X

			shr	esi,32-8
			mov	ebx,[Fill_Next_MapV]

			shl	edx,8
			mov	[Fill_Cur_MapU],eax

			or	esi,edx
			mov	[Fill_Cur_MapV],ebx

			mov	[Fill_CurZBuf],esi
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
Filler_TextureZChromaKeyFogNZW ENDP



;				*******************
;				*** Local proc ***
;				*******************





Last_POLYTEXTUREZPatch	Label	DWORD

_TEXT			ENDS

;			The
			End
