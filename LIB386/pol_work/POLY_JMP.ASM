; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                     (Adeline Software Intl.)                       ***
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




;					*****************
;					*** VARIABLES ***
;					*****************

;					*** PUBLIC DATA ***
PUBLIC	C		Fill_N_Table_Jumps
PUBLIC	C		Fill_Fog_Table_Jumps
PUBLIC	C		Fill_ZBuf_Table_Jumps
PUBLIC	C		Fill_FogZBuf_Table_Jumps
PUBLIC	C		Fill_NZW_Table_Jumps
PUBLIC	C		Fill_FogNZW_Table_Jumps

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
Extrn	C		Fill_Filler		:	DWORD
Extrn	C		Fill_ClipFlag		:	DWORD
Extrn	C		Fill_Color		:	DWORD
Extrn	C		Fill_Trame_Parity	:	DWORD
Extrn	C		PtrCLUTGouraud		:	DWORD
Extrn	C		Fill_Logical_Palette	:	BYTE
Extrn	C		IsPolygonHidden		:	DWORD





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

; ************************************************************************
; ************************************************************************
; ***                                                                  ***
; ***           JUMP TABLE (DEFAULT POLYGONS)                          ***
; ***                                                                  ***
; ************************************************************************
; ************************************************************************
Fill_N_Table_Jumps	Label	DWORD
						; **** FLAT + SPECIAUX ****
			dd	Jmp_Solid		; 0 Solid
			dd	Jmp_Solid		; 1 Flat
			dd	Jmp_Transparent		; 2 Transparent
			dd	Jmp_Trame		; 3 Trame

						; **** GOURAUD ****
			dd	Jmp_Gouraud		; 4 Gouraud
			dd	Jmp_Dither		; 5 Gouraud Dither
			dd	Jmp_GouraudTable	; 6 Gouraud table
			dd	Jmp_DitherTable		; 7 Gouraud Dither table

						; **** MAPPING ****
								; ** Without Chromakey **
			dd	Jmp_Texture		; 8 Texture Solid
			dd	Jmp_TextureFlat		; 9 Texture Flat

			dd	Jmp_GouraudTexture	; 10 Texture Gouraud
			dd	Jmp_DitherTexture	; 11 Texture Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureChromaKey	; 12 Texture Solid Chromakey
			dd	Jmp_TextureFlatChromaKey; 13 Texture Flat Chromakey

			dd	Jmp_GouraudTextureChromaKey; 14 Texture Gouraud Chromakey
			dd	Jmp_DitherTextureChromaKey; 15 Texture Gouraud Dither Chromakey


						; **** MAPPING + PERSPECTIVE CORRECTED ****
								; ** Without Chromakey **
			dd	Jmp_TextureZ		; 16 Texture Z Solid
			dd	Jmp_TextureZFlat	; 17 Texture Z Flat

			dd	Jmp_TextureZGouraud	; 18 Texture Z Gouraud
			dd	Jmp_TextureZGouraud	; 19 Texture Z Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureZChromaKey	; 20  Texture Z Solid Chromakey
			dd	Jmp_TextureZFlatChromaKey;21  Texture Z Flat Chromakey

			dd	Jmp_TextureZGouraudChromaKey; 22  Texture Z Gouraud Chromakey
			dd	Jmp_TextureZGouraudChromaKey; 23  Texture Z Gouraud Dither Chromakey

			dd	Jmp_TextureZFogSmooth	; 24 Texture Z Fog Smooth
			dd	Jmp_FlagZBuffer		; 25 Flag if object totally hidden




; ************************************************************************
; ************************************************************************
; ***                                                                  ***
; ***           JUMP TABLE (FOGGED  POLYGONS)                          ***
; ***                                                                  ***
; ************************************************************************
; ************************************************************************
Fill_Fog_Table_Jumps	Label	DWORD
						; **** FLAT + SPECIAUX ****
			dd	Jmp_SolidFog		; 0 Solid
			dd	Jmp_SolidFog		; 1 Flat
			dd	Jmp_Transparent		; 2 Transparent
			dd	Jmp_TrameFog		; 3 Trame

						; **** GOURAUD ****
			dd	Jmp_GouraudFog		; 4 Gouraud
			dd	Jmp_DitherFog		; 5 Gouraud Dither
			dd	Jmp_GouraudTable	; 6 Gouraud table
			dd	Jmp_DitherTable		; 7 Gouraud Dither table

						; **** MAPPING ****
								; ** Without Chromakey **
			dd	Jmp_TextureFog		; 8 Texture Solid
			dd	Jmp_TextureFlat		; 9 Texture Flat

			dd	Jmp_GouraudTexture	; 10 Texture Gouraud
			dd	Jmp_DitherTexture	; 11 Texture Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureChromaKeyFog	; 12 Texture Solid Chromakey
			dd	Jmp_TextureFlatChromaKey; 13 Texture Flat Chromakey

			dd	Jmp_GouraudTextureChromaKey; 14 Texture Gouraud Chromakey
			dd	Jmp_DitherTextureChromaKey; 15 Texture Gouraud Dither Chromakey


						; **** MAPPING + PERSPECTIVE CORRECTED ****
								; ** Without Chromakey **
			dd	Jmp_TextureZFog		; 16 Texture Z Solid
			dd	Jmp_TextureZFlat	; 17 Texture Z Flat

			dd	Jmp_TextureZGouraud	; 18 Texture Z Gouraud
			dd	Jmp_TextureZGouraud	; 19 Texture Z Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureZChromaKeyFog; 20  Texture Z Solid Chromakey
			dd	Jmp_TextureZFlatChromaKey;21  Texture Z Flat Chromakey

			dd	Jmp_TextureZGouraudChromaKey; 22  Texture Z Gouraud Chromakey
			dd	Jmp_TextureZGouraudChromaKey; 23  Texture Z Gouraud Dither Chromakey

			dd	Jmp_TextureZFogSmooth	; 24 Texture Z Fog Smooth
			dd	Jmp_FlagZBuffer		; 25 Flag if object totally hidden



; ************************************************************************
; ************************************************************************
; ***                                                                  ***
; ***           JUMP TABLE (ZBUFFERED POLYGONS)                        ***
; ***                                                                  ***
; ************************************************************************
; ************************************************************************
Fill_ZBuf_Table_Jumps	Label	DWORD
						; **** FLAT + SPECIAUX ****
			dd	Jmp_SolidZBuf		; 0 Solid
			dd	Jmp_SolidZBuf		; 1 Flat
			dd	Jmp_TransparentZBuf	; 2 Transparent
			dd	Jmp_TrameZBuf		; 3 Trame

						; **** GOURAUD ****
			dd	Jmp_GouraudZBuf		; 4 Gouraud
			dd	Jmp_DitherZBuf		; 5 Gouraud Dither
			dd	Jmp_GouraudTableZBuf	; 6 Gouraud table
			dd	Jmp_DitherTableZBuf	; 7 Gouraud Dither table

						; **** MAPPING ****
								; ** Without Chromakey **
			dd	Jmp_TextureZBuf		; 8 Texture Solid
			dd	Jmp_TextureFlatZBuf	; 9 Texture Flat

			dd	Jmp_GouraudTextureZBuf	; 10 Texture Gouraud
			dd	Jmp_GouraudTextureZBuf	; 11 Texture Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureChromaKeyZBuf; 12 Texture Solid Chromakey
			dd	Jmp_TextureFlatChromaKeyZBuf; 13 Texture Flat Chromakey

			dd	Jmp_GouraudTextureChromaKeyZBuf; 14 Texture Gouraud Chromakey
			dd	Jmp_GouraudTextureChromaKeyZBuf; 15 Texture Gouraud Dither Chromakey


						; **** MAPPING + PERSPECTIVE CORRECTED ****
								; ** Without Chromakey **
			dd	Jmp_TextureZZBuf	; 16 Texture Z Solid
			dd	Jmp_TextureZFlatZBuf	; 17 Texture Z Flat

			dd	Jmp_TextureZGouraudZBuf	; 18 Texture Z Gouraud
			dd	Jmp_TextureZGouraudZBuf	; 19 Texture Z Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureZChromaKeyZBuf; 20  Texture Z Solid Chromakey
			dd	Jmp_TextureZFlatChromaKeyZBuf;21  Texture Z Flat Chromakey

			dd	Jmp_TextureZGouraudChromaKeyZBuf; 22  Texture Z Gouraud Chromakey
			dd	Jmp_TextureZGouraudChromaKeyZBuf; 23  Texture Z Gouraud Dither Chromakey

			dd	Jmp_TextureZFogSmoothZBuf; 24 Texture Z Fog Smooth
			dd	Jmp_FlagZBuffer		; 25 Flag if object totally hidden


; ************************************************************************
; ************************************************************************
; ***                                                                  ***
; ***           JUMP TABLE (ZBUFFERED POLYGONS)                        ***
; ***                                                                  ***
; ************************************************************************
; ************************************************************************
Fill_FogZBuf_Table_Jumps Label	DWORD
						; **** FLAT + SPECIAUX ****
			dd	Jmp_SolidFogZBuf	; 0 Solid
			dd	Jmp_SolidFogZBuf	; 1 Flat
			dd	Jmp_TransparentZBuf	; 2 Transparent
			dd	Jmp_TrameFogZBuf	; 3 Trame

						; **** GOURAUD ****
			dd	Jmp_GouraudFogZBuf	; 4 Gouraud
			dd	Jmp_DitherFogZBuf	; 5 Gouraud Dither
			dd	Jmp_GouraudTableZBuf	; 6 Gouraud table
			dd	Jmp_DitherTableZBuf	; 7 Gouraud Dither table

						; **** MAPPING ****
								; ** Without Chromakey **
			dd	Jmp_TextureFogZBuf	; 8 Texture Solid
			dd	Jmp_TextureFlatZBuf	; 9 Texture Flat

			dd	Jmp_GouraudTextureZBuf	; 10 Texture Gouraud
			dd	Jmp_GouraudTextureZBuf	; 11 Texture Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureChromaKeyFogZBuf; 12 Texture Solid Chromakey
			dd	Jmp_TextureFlatChromaKeyZBuf; 13 Texture Flat Chromakey

			dd	Jmp_GouraudTextureChromaKeyZBuf; 14 Texture Gouraud Chromakey
			dd	Jmp_GouraudTextureChromaKeyZBuf; 15 Texture Gouraud Dither Chromakey


						; **** MAPPING + PERSPECTIVE CORRECTED ****
								; ** Without Chromakey **
			dd	Jmp_TextureZFogZBuf	; 16 Texture Z Solid
			dd	Jmp_TextureZFlatZBuf	; 17 Texture Z Flat

			dd	Jmp_TextureZGouraudZBuf	; 18 Texture Z Gouraud
			dd	Jmp_TextureZGouraudZBuf	; 19 Texture Z Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureZChromaKeyFogZBuf; 20  Texture Z Solid Chromakey
			dd	Jmp_TextureZFlatChromaKeyZBuf;21  Texture Z Flat Chromakey

			dd	Jmp_TextureZGouraudChromaKeyZBuf; 22  Texture Z Gouraud Chromakey
			dd	Jmp_TextureZGouraudChromaKeyZBuf; 23  Texture Z Gouraud Dither Chromakey

			dd	Jmp_TextureZFogSmoothZBuf; 24 Texture Z Fog Smooth
			dd	Jmp_FlagZBuffer		; 25 Flag if object totally hidden



; ************************************************************************
; ************************************************************************
; ***                                                                  ***
; ***           JUMP TABLE (ZBUFFERED without ZBuf update POLYGONS)    ***
; ***                                                                  ***
; ************************************************************************
; ************************************************************************
Fill_NZW_Table_Jumps	Label	DWORD
						; **** FLAT + SPECIAUX ****
			dd	Jmp_SolidNZW		; 0 Solid
			dd	Jmp_SolidNZW		; 1 Flat
			dd	Jmp_TransparentNZW	; 2 Transparent
			dd	Jmp_TrameNZW		; 3 Trame

						; **** GOURAUD ****
			dd	Jmp_GouraudNZW		; 4 Gouraud
			dd	Jmp_DitherNZW		; 5 Gouraud Dither
			dd	Jmp_GouraudTableNZW	; 6 Gouraud table
			dd	Jmp_DitherTableNZW	; 7 Gouraud Dither table

						; **** MAPPING ****
								; ** Without Chromakey **
			dd	Jmp_TextureNZW		; 8 Texture Solid
			dd	Jmp_TextureFlatNZW	; 9 Texture Flat

			dd	Jmp_GouraudTextureNZW	; 10 Texture Gouraud
			dd	Jmp_GouraudTextureNZW	; 11 Texture Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureChromaKeyNZW; 12 Texture Solid Chromakey
			dd	Jmp_TextureFlatChromaKeyNZW; 13 Texture Flat Chromakey

			dd	Jmp_GouraudTextureChromaKeyNZW; 14 Texture Gouraud Chromakey
			dd	Jmp_GouraudTextureChromaKeyNZW; 15 Texture Gouraud Dither Chromakey


						; **** MAPPING + PERSPECTIVE CORRECTED ****
								; ** Without Chromakey **
			dd	Jmp_TextureZNZW	; 16 Texture Z Solid
			dd	Jmp_TextureZFlatNZW	; 17 Texture Z Flat

			dd	Jmp_TextureZGouraudNZW	; 18 Texture Z Gouraud
			dd	Jmp_TextureZGouraudNZW	; 19 Texture Z Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureZChromaKeyNZW; 20  Texture Z Solid Chromakey
			dd	Jmp_TextureZFlatChromaKeyNZW;21  Texture Z Flat Chromakey

			dd	Jmp_TextureZGouraudChromaKeyNZW; 22  Texture Z Gouraud Chromakey
			dd	Jmp_TextureZGouraudChromaKeyNZW; 23  Texture Z Gouraud Dither Chromakey

			dd	Jmp_TextureZFogSmoothNZW; 24 Texture Z Fog Smooth
			dd	Jmp_FlagZBuffer		; 25 Flag if object totally hidden


; ************************************************************************
; ************************************************************************
; ***                                                                  ***
; ***           JUMP TABLE (ZBUFFERED without ZBuf update POLYGONS)    ***
; ***                                                                  ***
; ************************************************************************
; ************************************************************************
Fill_FogNZW_Table_Jumps Label	DWORD
						; **** FLAT + SPECIAUX ****
			dd	Jmp_SolidFogNZW		; 0 Solid
			dd	Jmp_SolidFogNZW		; 1 Flat
			dd	Jmp_TransparentNZW	; 2 Transparent
			dd	Jmp_TrameFogNZW		; 3 Trame

						; **** GOURAUD ****
			dd	Jmp_GouraudFogNZW	; 4 Gouraud
			dd	Jmp_DitherFogNZW	; 5 Gouraud Dither
			dd	Jmp_GouraudTableNZW	; 6 Gouraud table
			dd	Jmp_DitherTableNZW	; 7 Gouraud Dither table

						; **** MAPPING ****
								; ** Without Chromakey **
			dd	Jmp_TextureFogNZW	; 8 Texture Solid
			dd	Jmp_TextureFlatNZW	; 9 Texture Flat

			dd	Jmp_GouraudTextureNZW	; 10 Texture Gouraud
			dd	Jmp_GouraudTextureNZW	; 11 Texture Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureChromaKeyFogNZW; 12 Texture Solid Chromakey
			dd	Jmp_TextureFlatChromaKeyNZW; 13 Texture Flat Chromakey

			dd	Jmp_GouraudTextureChromaKeyNZW; 14 Texture Gouraud Chromakey
			dd	Jmp_GouraudTextureChromaKeyNZW; 15 Texture Gouraud Dither Chromakey


						; **** MAPPING + PERSPECTIVE CORRECTED ****
								; ** Without Chromakey **
			dd	Jmp_TextureZFogNZW	; 16 Texture Z Solid
			dd	Jmp_TextureZFlatNZW	; 17 Texture Z Flat

			dd	Jmp_TextureZGouraudNZW	; 18 Texture Z Gouraud
			dd	Jmp_TextureZGouraudNZW	; 19 Texture Z Gouraud Dither

								; ** With Chromakey **
			dd	Jmp_TextureZChromaKeyFogNZW; 20  Texture Z Solid Chromakey
			dd	Jmp_TextureZFlatChromaKeyNZW;21  Texture Z Flat Chromakey

			dd	Jmp_TextureZGouraudChromaKeyNZW; 22  Texture Z Gouraud Chromakey
			dd	Jmp_TextureZGouraudChromaKeyNZW; 23  Texture Z Gouraud Dither Chromakey

			dd	Jmp_TextureZFogSmoothNZW	; 24 Texture Z Fog Smooth
			dd	Jmp_FlagZBuffer		; 25 Flag if object totally hidden









;				******************
;				*** Local data ***
;				******************
			ALIGN	4




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


				; ** POLY.ASM **
Extrn	C		Fill_PolyClip		:	PROC


				; ** POLYFLAT.ASM **
;Extrn	C		Fill_PolyFlat		:	PROC
Extrn	C		Filler_Flat		:	PROC
Extrn	C		Filler_Transparent	:	PROC
Extrn	C		Filler_Trame		:	PROC
Extrn	C		Filler_FlatZBuf		:	PROC
Extrn	C		Filler_TransparentZBuf	:	PROC
Extrn	C		Filler_TrameZBuf	:	PROC
Extrn	C		Filler_FlatNZW		:	PROC
Extrn	C		Filler_TransparentNZW	:	PROC
Extrn	C		Filler_TrameNZW		:	PROC
Extrn	C		Filler_FlagZBuffer	:	PROC


				; ** POLYGOUR.ASM **
Extrn	C		Filler_Gouraud		:	PROC
Extrn	C		Filler_Dither		:	PROC
Extrn	C		Filler_GouraudTable	:	PROC
Extrn	C		Filler_DitherTable	:	PROC
Extrn	C		Filler_GouraudFog	:	PROC
Extrn	C		Filler_DitherFog	:	PROC
Extrn	C		Filler_GouraudZBuf	:	PROC
Extrn	C		Filler_DitherZBuf	:	PROC
Extrn	C		Filler_GouraudTableZBuf	:	PROC
Extrn	C		Filler_DitherTableZBuf	:	PROC
Extrn	C		Filler_GouraudFogZBuf	:	PROC
Extrn	C		Filler_DitherFogZBuf	:	PROC
Extrn	C		Filler_GouraudNZW	:	PROC
Extrn	C		Filler_DitherNZW	:	PROC
Extrn	C		Filler_GouraudTableNZW	:	PROC
Extrn	C		Filler_DitherTableNZW	:	PROC
Extrn	C		Filler_GouraudFogNZW	:	PROC
Extrn	C		Filler_DitherFogNZW	:	PROC


				; ** POLYTEXT.ASM **
Extrn	C		Filler_Texture		:	PROC
Extrn	C		Filler_TextureFlat	:	PROC
Extrn	C		Filler_TextureChromaKey	:	PROC
Extrn	C		Filler_TextureFlatChromaKey:	PROC
Extrn	C		Filler_TextureFog	:	PROC
Extrn	C		Filler_TextureChromaKeyFog:	PROC
Extrn	C		Filler_TextureZBuf	:	PROC
Extrn	C		Filler_TextureChromaKeyZBuf:	PROC
Extrn	C		Filler_TextureFlatZBuf	:	PROC
Extrn	C		Filler_TextureFlatChromaKeyZBuf:PROC
Extrn	C		Filler_TextureFogZBuf	:	PROC
Extrn	C		Filler_TextureChromaKeyFogZBuf:	PROC
Extrn	C		Filler_TextureNZW	:	PROC
Extrn	C		Filler_TextureChromaKeyNZW:	PROC
Extrn	C		Filler_TextureFlatNZW	:	PROC
Extrn	C		Filler_TextureFlatChromaKeyNZW:PROC
Extrn	C		Filler_TextureFogNZW	:	PROC
Extrn	C		Filler_TextureChromaKeyFogNZW:	PROC


				; ** POLYGTEX.ASM **
Extrn	C		Filler_TextureGouraud	:	PROC
Extrn	C		Filler_TextureDither	:	PROC
Extrn	C		Filler_TextureGouraudChromaKey:	PROC
Extrn	C		Filler_TextureDitherChromaKey:	PROC
Extrn	C		Filler_TextureGouraudZBuf:	PROC
Extrn	C		Filler_TextureGouraudChromaKeyZBuf:PROC
Extrn	C		Filler_TextureGouraudNZW:	PROC
Extrn	C		Filler_TextureGouraudChromaKeyNZW:PROC

				; ** POLYTEXZ.ASM **
Extrn	C		Filler_TextureZ		:	PROC
Extrn	C		Filler_TextureZFlat	:	PROC
Extrn	C		Filler_TextureZChromaKey:	PROC
Extrn	C		Filler_TextureZFlatChromaKey:	PROC
Extrn	C		Filler_TextureZFog	:	PROC
Extrn	C		Filler_TextureZChromaKeyFog:	PROC
Extrn	C		Filler_TextureZZBuf	:	PROC
Extrn	C		Filler_TextureZFlatZBuf	:	PROC
Extrn	C		Filler_TextureZChromaKeyZBuf:	PROC
Extrn	C		Filler_TextureZFlatChromaKeyZBuf:PROC
Extrn	C		Filler_TextureZFogZBuf	:	PROC
Extrn	C		Filler_TextureZChromaKeyFogZBuf:PROC
Extrn	C		Filler_TextureZNZW	:	PROC
Extrn	C		Filler_TextureZFlatNZW	:	PROC
Extrn	C		Filler_TextureZChromaKeyNZW:	PROC
Extrn	C		Filler_TextureZFlatChromaKeyNZW:PROC
Extrn	C		Filler_TextureZFogNZW	:	PROC
Extrn	C		Filler_TextureZChromaKeyFogNZW:PROC


				; ** POLYTZG.ASM **
Extrn	C		Filler_TextureZGouraud	:	PROC
Extrn	C		Filler_TextureZGouraudChromaKey:PROC
Extrn	C		Filler_TextureZGouraudZBuf:	PROC
Extrn	C		Filler_TextureZGouraudChromaKeyZBuf:PROC
Extrn	C		Filler_TextureZGouraudNZW:	PROC
Extrn	C		Filler_TextureZGouraudChromaKeyNZW:PROC


				; ** POLYTZF.ASM **
Extrn	C		Filler_TextureZFogSmooth:	PROC
Extrn	C		Filler_TextureZFogSmoothZBuf:	PROC
Extrn	C		Filler_TextureZFogSmoothNZW:	PROC




;				*******************
;				*** Global proc ***
;				*******************

;				******************
;				*** Local proc ***
;				******************


;			**************
;			***  FLAT  ***
;			**************


Jmp_Solid		PROC
			mov	eax,Offset Filler_Flat
			mov	edx,ebx

			shl	edx,8
			mov	[Fill_Filler],eax

			or	ebx,edx
			mov	edx,CLIP_FLAT

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_Solid		ENDP




Jmp_Transparent		PROC
			mov	eax,Offset Filler_Transparent
			and	ebx,0F0h

			mov	[Fill_Filler],eax
			mov	edx,CLIP_FLAT

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_Transparent		ENDP





Jmp_Trame		PROC
			mov	eax,Offset Filler_Trame
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			mov	edx,CLIP_FLAT

			mov	[Fill_ClipFlag],edx
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			jmp	Fill_PolyClip
Jmp_Trame		ENDP




Jmp_SolidFog		PROC
			mov	bl,[Fill_Logical_Palette+ebx]
			mov	eax,Offset Filler_Flat
			mov	edx,ebx

			shl	edx,8
			mov	[Fill_Filler],eax

			or	ebx,edx
			mov	edx,CLIP_FLAT

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_SolidFog		ENDP




Jmp_TrameFog		PROC
			mov	bl,[Fill_Logical_Palette+ebx]
			mov	eax,Offset Filler_Trame
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			mov	edx,CLIP_FLAT

			mov	[Fill_ClipFlag],edx
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			jmp	Fill_PolyClip
Jmp_TrameFog		ENDP





Jmp_SolidZBuf		PROC
			mov	eax,Offset Filler_FlatZBuf
			mov	edx,ebx

			shl	edx,8
			mov	[Fill_Filler],eax

			or	ebx,edx
			mov	edx,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_SolidZBuf		ENDP



Jmp_TransparentZBuf	PROC
			mov	eax,Offset Filler_TransparentZBuf
			and	ebx,0F0h

			mov	[Fill_Filler],eax
			mov	[Fill_Color],ebx
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],eax
			jmp	Fill_PolyClip
Jmp_TransparentZBuf	ENDP





Jmp_TrameZBuf		PROC
			mov	eax,Offset Filler_TrameZBuf
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],eax
			jmp	Fill_PolyClip
Jmp_TrameZBuf		ENDP




Jmp_SolidFogZBuf	PROC
			mov	bl,[Fill_Logical_Palette+ebx]
			mov	eax,Offset Filler_FlatZBuf

			mov	edx,ebx
			mov	[Fill_Filler],eax

			shl	edx,8

			or	ebx,edx
			mov	edx,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_SolidFogZBuf	ENDP






Jmp_TrameFogZBuf	PROC
			mov	bl,[Fill_Logical_Palette]
			mov	eax,Offset Filler_TrameZBuf

			mov	[Fill_Filler],eax
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_Color],ebx
			mov	[Fill_ClipFlag],eax

			jmp	Fill_PolyClip
Jmp_TrameFogZBuf	ENDP





Jmp_SolidNZW		PROC
			mov	eax,Offset Filler_FlatNZW
			mov	edx,ebx

			shl	edx,8
			mov	[Fill_Filler],eax

			or	ebx,edx
			mov	edx,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_SolidNZW		ENDP



Jmp_TransparentNZW	PROC
			mov	eax,Offset Filler_TransparentNZW
			and	ebx,0F0h

			mov	[Fill_Filler],eax
			mov	[Fill_Color],ebx
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],eax
			jmp	Fill_PolyClip
Jmp_TransparentNZW	ENDP





Jmp_TrameNZW		PROC
			mov	eax,Offset Filler_TrameNZW
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],eax
			jmp	Fill_PolyClip
Jmp_TrameNZW		ENDP




Jmp_SolidFogNZW		PROC
			mov	bl,[Fill_Logical_Palette+ebx]
			mov	eax,Offset Filler_FlatNZW

			mov	edx,ebx
			mov	[Fill_Filler],eax

			shl	edx,8

			or	ebx,edx
			mov	edx,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_SolidFogNZW		ENDP






Jmp_TrameFogNZW		PROC
			mov	bl,[Fill_Logical_Palette]
			mov	eax,Offset Filler_TrameNZW

			mov	[Fill_Filler],eax
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_Color],ebx
			mov	[Fill_ClipFlag],eax

			jmp	Fill_PolyClip
Jmp_TrameFogNZW		ENDP













;			*****************
;			***  GOURAUD  ***
;			*****************

Jmp_Gouraud		PROC
			shl	ebx,16
			mov	eax,Offset Filler_Gouraud

			mov	edx,CLIP_FLAT+CLIP_LIGHT
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_Gouraud		ENDP


Jmp_Dither		PROC
			shl	ebx,16
			mov	eax,Offset Filler_Dither

			mov	edx,CLIP_FLAT+CLIP_LIGHT
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_Dither		ENDP



Jmp_GouraudTable	PROC
			mov	eax,Offset Filler_GouraudTable
			mov	edx,CLIP_FLAT+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_GouraudTable	ENDP




Jmp_DitherTable		PROC
			mov	eax,Offset Filler_DitherTable
			mov	edx,CLIP_FLAT+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_DitherTable		ENDP




Jmp_GouraudFog		PROC
			shl	ebx,16
			mov	eax,Offset Filler_GouraudFog

			mov	edx,CLIP_FLAT+CLIP_LIGHT
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_GouraudFog		ENDP


Jmp_DitherFog		PROC
			shl	ebx,16
			mov	eax,Offset Filler_DitherFog

			mov	edx,CLIP_FLAT+CLIP_LIGHT
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_DitherFog		ENDP




Jmp_GouraudZBuf		PROC
			shl	ebx,16
			mov	eax,Offset Filler_GouraudZBuf

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_GouraudZBuf		ENDP


Jmp_DitherZBuf		PROC
			shl	ebx,16
			mov	eax,Offset Filler_DitherZBuf

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_DitherZBuf		ENDP




Jmp_GouraudTableZBuf	PROC
			mov	eax,Offset Filler_GouraudTableZBuf
			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_GouraudTableZBuf	ENDP




Jmp_DitherTableZBuf	PROC
			mov	eax,Offset Filler_DitherTableZBuf
			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_DitherTableZBuf	ENDP



Jmp_GouraudFogZBuf	PROC
			shl	ebx,16
			mov	eax,Offset Filler_GouraudFogZBuf

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_GouraudFogZBuf	ENDP


Jmp_DitherFogZBuf	PROC
			shl	ebx,16
			mov	eax,Offset Filler_DitherFogZBuf

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_DitherFogZBuf	ENDP





Jmp_GouraudNZW		PROC
			shl	ebx,16
			mov	eax,Offset Filler_GouraudNZW

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_GouraudNZW		ENDP


Jmp_DitherNZW		PROC
			shl	ebx,16
			mov	eax,Offset Filler_DitherNZW

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_DitherNZW		ENDP




Jmp_GouraudTableNZW	PROC
			mov	eax,Offset Filler_GouraudTableNZW
			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_GouraudTableNZW	ENDP




Jmp_DitherTableNZW	PROC
			mov	eax,Offset Filler_DitherTableNZW
			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_DitherTableNZW	ENDP



Jmp_GouraudFogNZW	PROC
			shl	ebx,16
			mov	eax,Offset Filler_GouraudFogNZW

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_GouraudFogNZW	ENDP


Jmp_DitherFogNZW	PROC
			shl	ebx,16
			mov	eax,Offset Filler_DitherFogNZW

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_DitherFogNZW	ENDP






;			*****************
;			***  TEXTURE  ***
;			*****************

Jmp_Texture		PROC
			mov	eax,Offset Filler_Texture
			mov	edx,CLIP_FLAT+CLIP_TEXTURE

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_Texture		ENDP


Jmp_TextureFlat		PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureFlat

			mov	edx,CLIP_FLAT+CLIP_TEXTURE
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFlat		ENDP


Jmp_TextureChromaKey	PROC
			mov	eax,Offset Filler_TextureChromaKey
			mov	edx,CLIP_FLAT+CLIP_TEXTURE

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureChromaKey	ENDP



Jmp_TextureFlatChromaKey PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureFlatChromaKey

			mov	edx,CLIP_FLAT+CLIP_TEXTURE
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFlatChromaKey ENDP




Jmp_TextureFog		PROC
			mov	eax,Offset Filler_TextureFog
			mov	edx,CLIP_FLAT+CLIP_TEXTURE

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFog		ENDP



Jmp_TextureChromaKeyFog	PROC
			mov	eax,Offset Filler_TextureChromaKeyFog
			mov	edx,CLIP_FLAT+CLIP_TEXTURE

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureChromaKeyFog	ENDP




Jmp_TextureZBuf		PROC
			mov	eax,Offset Filler_TextureZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZBuf		ENDP



Jmp_TextureChromaKeyZBuf PROC
			mov	eax,Offset Filler_TextureChromaKeyZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureChromaKeyZBuf ENDP


Jmp_TextureFlatZBuf	PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureFlatZBuf

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Color],ebx
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFlatZBuf	ENDP





Jmp_TextureFlatChromaKeyZBuf PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureFlatChromaKeyZBuf

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Color],ebx
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFlatChromaKeyZBuf ENDP



Jmp_TextureFogZBuf	PROC
			mov	eax,Offset Filler_TextureFogZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFogZBuf	ENDP



Jmp_TextureChromaKeyFogZBuf PROC
			mov	eax,Offset Filler_TextureChromaKeyFogZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureChromaKeyFogZBuf ENDP



Jmp_TextureNZW		PROC
			mov	eax,Offset Filler_TextureNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureNZW		ENDP



Jmp_TextureChromaKeyNZW PROC
			mov	eax,Offset Filler_TextureChromaKeyNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureChromaKeyNZW ENDP


Jmp_TextureFlatNZW	PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureFlatNZW

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Color],ebx
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFlatNZW	ENDP





Jmp_TextureFlatChromaKeyNZW PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureFlatChromaKeyNZW

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Color],ebx
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFlatChromaKeyNZW ENDP



Jmp_TextureFogNZW	PROC
			mov	eax,Offset Filler_TextureFogNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFogNZW	ENDP



Jmp_TextureChromaKeyFogNZW PROC
			mov	eax,Offset Filler_TextureChromaKeyFogNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureChromaKeyFogNZW ENDP






;			*************************
;			***  GOURAUD TEXTURE  ***
;			*************************

Jmp_GouraudTexture	PROC
			mov	eax,Offset Filler_TextureGouraud
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_GouraudTexture	ENDP


Jmp_DitherTexture	PROC
			mov	eax,Offset Filler_TextureDither
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_DitherTexture	ENDP


Jmp_GouraudTextureChromaKey PROC
			mov	eax,Offset Filler_TextureGouraudChromaKey
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_GouraudTextureChromaKey ENDP


Jmp_DitherTextureChromaKey PROC
			mov	eax,Offset Filler_TextureDitherChromaKey
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_DitherTextureChromaKey ENDP



Jmp_GouraudTextureZBuf	PROC
			mov	eax,Offset Filler_TextureGouraudZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_GouraudTextureZBuf	ENDP



Jmp_GouraudTextureChromaKeyZBuf PROC
			mov	eax,Offset Filler_TextureGouraudChromaKeyZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_GouraudTextureChromaKeyZBuf ENDP





Jmp_GouraudTextureNZW	PROC
			mov	eax,Offset Filler_TextureGouraudNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_GouraudTextureNZW	ENDP



Jmp_GouraudTextureChromaKeyNZW PROC
			mov	eax,Offset Filler_TextureGouraudChromaKeyNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_GouraudTextureChromaKeyNZW ENDP





;			********************************************
;			***  TEXTURE with PERSPECTIVE CORRECTION ***
;			********************************************

Jmp_TextureZ		PROC
			mov	eax,Offset Filler_TextureZ
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZ		ENDP


Jmp_TextureZFlat	PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureZFlat

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ
			add	eax,ebx

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],eax

			jmp	Fill_PolyClip
Jmp_TextureZFlat	ENDP




Jmp_TextureZChromaKey	PROC
			mov	eax,Offset Filler_TextureZChromaKey
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKey	ENDP



Jmp_TextureZFlatChromaKey PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureZFlatChromaKey

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ
			add	eax,ebx

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],eax

			jmp	Fill_PolyClip
Jmp_TextureZFlatChromaKey ENDP



Jmp_TextureZFog		PROC
			mov	eax,Offset Filler_TextureZFog
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZFog		ENDP




Jmp_TextureZChromaKeyFog PROC
			mov	eax,Offset Filler_TextureZChromaKeyFog
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKeyFog ENDP




Jmp_TextureZZBuf	PROC
			mov	eax,Offset Filler_TextureZZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZZBuf	ENDP



Jmp_TextureZFlatZBuf	PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureZFlatZBuf

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER
			add	eax,ebx

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],eax

			jmp	Fill_PolyClip
Jmp_TextureZFlatZBuf	ENDP




Jmp_TextureZChromaKeyZBuf PROC
			mov	eax,Offset Filler_TextureZChromaKeyZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKeyZBuf ENDP



Jmp_TextureZFlatChromaKeyZBuf PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureZFlatChromaKeyZBuf

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER
			add	eax,ebx

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],eax

			jmp	Fill_PolyClip
Jmp_TextureZFlatChromaKeyZBuf ENDP





Jmp_TextureZFogZBuf	PROC
			mov	eax,Offset Filler_TextureZFogZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZFogZBuf	ENDP






Jmp_TextureZChromaKeyFogZBuf PROC
			mov	eax,Offset Filler_TextureZChromaKeyFogZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKeyFogZBuf ENDP






Jmp_TextureZNZW	PROC
			mov	eax,Offset Filler_TextureZNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZNZW	ENDP



Jmp_TextureZFlatNZW	PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureZFlatNZW

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER
			add	eax,ebx

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],eax

			jmp	Fill_PolyClip
Jmp_TextureZFlatNZW	ENDP




Jmp_TextureZChromaKeyNZW PROC
			mov	eax,Offset Filler_TextureZChromaKeyNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKeyNZW ENDP



Jmp_TextureZFlatChromaKeyNZW PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureZFlatChromaKeyNZW

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER
			add	eax,ebx

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],eax

			jmp	Fill_PolyClip
Jmp_TextureZFlatChromaKeyNZW ENDP





Jmp_TextureZFogNZW	PROC
			mov	eax,Offset Filler_TextureZFogNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZFogNZW	ENDP






Jmp_TextureZChromaKeyFogNZW PROC
			mov	eax,Offset Filler_TextureZChromaKeyFogNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKeyFogNZW ENDP






;			****************************************************************
;			***  TEXTURE with PERSPECTIVE CORRECTION and GOURAUD SHADING ***
;			****************************************************************

Jmp_TextureZGouraud	PROC
			mov	eax,Offset Filler_TextureZGouraud
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraud	ENDP


Jmp_TextureZGouraudChromaKey PROC
			mov	eax,Offset Filler_TextureZGouraudChromaKey
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraudChromaKey ENDP




Jmp_TextureZGouraudZBuf	PROC
			mov	eax,Offset Filler_TextureZGouraudZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraudZBuf	ENDP



Jmp_TextureZGouraudChromaKeyZBuf PROC
			mov	eax,Offset Filler_TextureZGouraudChromaKeyZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraudChromaKeyZBuf ENDP



Jmp_TextureZGouraudNZW	PROC
			mov	eax,Offset Filler_TextureZGouraudNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraudNZW	ENDP



Jmp_TextureZGouraudChromaKeyNZW PROC
			mov	eax,Offset Filler_TextureZGouraudChromaKeyNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraudChromaKeyNZW ENDP








;			************************************************
;			***  TEXTURE FOG with PERSPECTIVE CORRECTION ***
;			************************************************

Jmp_TextureZFogSmooth	PROC
			mov	eax,Offset Filler_TextureZFogSmooth
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZFogSmooth	ENDP





Jmp_TextureZFogSmoothZBuf PROC
			mov	eax,Offset Filler_TextureZFogSmoothZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZFogSmoothZBuf ENDP



Jmp_TextureZFogSmoothNZW PROC
			mov	eax,Offset Filler_TextureZFogSmoothNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZFogSmoothNZW ENDP




Jmp_FlagZBuffer		PROC
			mov	eax,Offset Filler_FlagZBuffer
			mov	edx,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	eax,1

			mov	[Fill_ClipFlag],edx
			mov	[IsPolygonHidden],eax

			jmp	Fill_PolyClip
Jmp_FlagZBuffer		ENDP





_TEXT			ENDS

;			The
			End
