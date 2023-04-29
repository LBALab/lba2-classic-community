/*
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
*/

#include <POLYGON/POLYTEXZ.H>

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
Fill_Init_Perspective	dd	Offset Init_PerspFPU
Fill_Loop_Perspective	dd	Offset Loop_PerspFPU

; Runtime code patch refactor, uses memory instead of instruction substitution
RepMask1        DWORD 12345678h
RepMask2        DWORD 12345678h
RepMaskPatch    DWORD 12345678h
IntZBuf         DWORD 12345678h
IntZBuf2        DWORD 12345678h
DecZBuf         DWORD 12345678h
DecZBuf2        DWORD 12345678h
PtrLog          DWORD 12345678h
PtrZBuffer1     DWORD 12345678h
PtrZBuffer2     DWORD 12345678h
PtrZBufferPatch DWORD 12345678h
PtrMapPatch     DWORD 12345678h
PtrCLUT         DWORD 12345678h

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

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZ		PROC
*/
S32 Filler_TextureZ(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlat	PROC
*/
S32 Filler_TextureZFlat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKey PROC
*/
S32 Filler_TextureZChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlatChromaKey PROC
*/
S32 Filler_TextureZFlatChromaKey(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFog	PROC
*/
S32 Filler_TextureZFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKeyFog PROC
*/
S32 Filler_TextureZChromaKeyFog(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZZBuf	PROC
*/
S32 Filler_TextureZZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKeyZBuf PROC
*/
S32 Filler_TextureZChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlatZBuf	PROC
*/
S32 Filler_TextureZFlatZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlatChromaKeyZBuf PROC
*/
S32 Filler_TextureZFlatChromaKeyZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFogZBuf	PROC
*/
S32 Filler_TextureZFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKeyFogZBuf PROC
*/
S32 Filler_TextureZChromaKeyFogZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZNZW	PROC
*/
S32 Filler_TextureZNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKeyNZW PROC
*/
S32 Filler_TextureZChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlatNZW	PROC
*/
S32 Filler_TextureZFlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFlatChromaKeyNZW PROC
*/
S32 Filler_TextureZFlatChromaKeyNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZFogNZW	PROC
*/
S32 Filler_TextureZFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TextureZChromaKeyFogNZW PROC
*/
S32 Filler_TextureZChromaKeyFogNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

