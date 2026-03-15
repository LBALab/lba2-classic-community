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
PUBLIC	C		Filler_Flat
PUBLIC	C		Filler_Transparent
PUBLIC	C		Filler_Trame
PUBLIC	C		Filler_FlatZBuf
PUBLIC	C		Filler_TransparentZBuf
PUBLIC	C		Filler_TrameZBuf
PUBLIC	C		Filler_FlatNZW
PUBLIC	C		Filler_TransparentNZW
PUBLIC	C		Filler_TrameNZW
PUBLIC	C		Filler_FlagZBuffer


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
Extrn	C		Fill_Filler		:	DWORD
Extrn	C		Fill_Color		:	DWORD
Extrn	C		Fill_Trame_Parity	:	DWORD
Extrn	C		Fill_Patch		:	DWORD

Extrn	C		Fill_FirstPoint		:	DWORD
Extrn	C		Fill_LastPoint		:	DWORD
Extrn	C		Fill_LeftPoint		:	DWORD
Extrn	C		Fill_RightPoint		:	DWORD

Extrn	C		Fill_LeftSlope		:	DWORD
Extrn	C		Fill_RightSlope		:	DWORD

Extrn	C		Fill_CurY		:	DWORD
Extrn	C		Fill_CurXMin		:	DWORD
Extrn	C		Fill_CurXMax		:	DWORD


Extrn	C		Fill_ClipFlag		:	DWORD
Extrn	C		Fill_ReadFlag		:	DWORD

Extrn	C		PTR_TabOffLine		:	DWORD
Extrn	C		Fill_CurOffLine		:	DWORD
Extrn	C		Log			:	DWORD
Extrn	C		PtrZBuffer		:	DWORD
Extrn	C		ScreenPitch		:	DWORD


Extrn	C		ClipXMin		:	DWORD
Extrn	C		ClipXMax		:	DWORD
Extrn	C		ClipYMin		:	DWORD
Extrn	C		ClipYMax		:	DWORD

Extrn	C		Fill_CurZBufMin		:	DWORD
Extrn	C		Fill_ZBuf_LeftSlope	:	DWORD
Extrn	C		Fill_ZBuf_XSlope	:	DWORD

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







;				******************
;				*** Local data ***
;				******************
			ALIGN	4
; Runtime code patch refactor, uses memory instead of instruction substitution
PtrLog              DWORD 12345678h
PtrLog1             DWORD 12345678h
PtrLog2             DWORD 12345678h
PtrZBuffer1         DWORD 12345678h
PtrZBuffer2         DWORD 12345678h
PtrZBuffer3         DWORD 12345678h
PtrZBufferPatch     DWORD 12345678h

		EXTRN	C	printf	:NEAR

fmt_f_flat_1	DB	"[ASM][Filler_Flat][1] entry", 10, 0
fmt_f_transparent_1	DB	"[ASM][Filler_Transparent][1] entry", 10, 0
fmt_f_trame_1	DB	"[ASM][Filler_Trame][1] entry", 10, 0
fmt_f_flatzbuf_1	DB	"[ASM][Filler_FlatZBuf][1] entry", 10, 0
fmt_f_transparentzbu_1	DB	"[ASM][Filler_TransparentZBuf][1] entry", 10, 0
fmt_f_tramezbuf_1	DB	"[ASM][Filler_TrameZBuf][1] entry", 10, 0
fmt_f_flatnzw_1	DB	"[ASM][Filler_FlatNZW][1] entry", 10, 0
fmt_f_transparentnzw_1	DB	"[ASM][Filler_TransparentNZW][1] entry", 10, 0
fmt_f_tramenzw_1	DB	"[ASM][Filler_TrameNZW][1] entry", 10, 0


fmt_filler_flat_1b	DB	"[ASM][Filler_Flat][1b] LeftSlope=%d RightSlope=%d curY=%d", 10, 0
fmt_filler_flat_1c	DB	"[ASM][Filler_Flat][1c] U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d", 10, 0
fmt_filler_flat_1d	DB	"[ASM][Filler_Flat][1d] U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d", 10, 0
fmt_filler_flat_1e	DB	"[ASM][Filler_Flat][1e] curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u", 10, 0
fmt_filler_trans_1b	DB	"[ASM][Filler_Transparent][1b] LeftSlope=%d RightSlope=%d curY=%d", 10, 0
fmt_filler_trans_1c	DB	"[ASM][Filler_Transparent][1c] U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d", 10, 0
fmt_filler_trans_1d	DB	"[ASM][Filler_Transparent][1d] U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d", 10, 0
fmt_filler_trans_1e	DB	"[ASM][Filler_Transparent][1e] curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u", 10, 0
fmt_filler_trame_1b	DB	"[ASM][Filler_Trame][1b] LeftSlope=%d RightSlope=%d curY=%d", 10, 0
fmt_filler_trame_1c	DB	"[ASM][Filler_Trame][1c] U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d", 10, 0
fmt_filler_trame_1d	DB	"[ASM][Filler_Trame][1d] U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d", 10, 0
fmt_filler_trame_1e	DB	"[ASM][Filler_Trame][1e] curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u", 10, 0
fmt_filler_flatz_1b	DB	"[ASM][Filler_FlatZBuf][1b] LeftSlope=%d RightSlope=%d curY=%d", 10, 0
fmt_filler_flatz_1c	DB	"[ASM][Filler_FlatZBuf][1c] U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d", 10, 0
fmt_filler_flatz_1d	DB	"[ASM][Filler_FlatZBuf][1d] U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d", 10, 0
fmt_filler_flatz_1e	DB	"[ASM][Filler_FlatZBuf][1e] curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u", 10, 0
fmt_filler_flatn_1b	DB	"[ASM][Filler_FlatNZW][1b] LeftSlope=%d RightSlope=%d curY=%d", 10, 0
fmt_filler_flatn_1c	DB	"[ASM][Filler_FlatNZW][1c] U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d", 10, 0
fmt_filler_flatn_1d	DB	"[ASM][Filler_FlatNZW][1d] U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d", 10, 0
fmt_filler_flatn_1e	DB	"[ASM][Filler_FlatNZW][1e] curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u", 10, 0

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
Extrn	C		Fill_ClipXMin		:	PROC
Extrn	C		Fill_ClipXMax		:	PROC
Extrn	C		Fill_ClipYMin		:	PROC
Extrn	C		Fill_ClipYMax		:	PROC

Extrn	C		Triangle_ReadNextEdge	:	PROC



;				*******************
;				*** Global proc ***
;				*******************

;				******************
;				*** Local proc ***
;				******************


; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_Flat		PROC

		; --- debug trace [ASM][Filler_Flat][1] entry ---
		pushad
		push	offset fmt_f_flat_1
		call	printf
		add	esp, 4
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TrameNZW][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trame_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TrameNZW][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trame_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TransparentNZW][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trans_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TransparentNZW][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trans_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_FlatNZW][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_flatn_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_FlatNZW][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_flatn_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TrameZBuf][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trame_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TrameZBuf][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trame_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TransparentZBuf][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trans_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TransparentZBuf][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trans_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_FlatZBuf][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_flatz_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_FlatZBuf][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_flatz_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Trame][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trame_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Trame][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trame_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Transparent][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trans_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Transparent][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trans_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Flat][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_flat_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Flat][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_flat_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TrameNZW][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trame_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TrameNZW][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trame_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TransparentNZW][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trans_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TransparentNZW][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trans_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_FlatNZW][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_flatn_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_FlatNZW][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_flatn_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TrameZBuf][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trame_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TrameZBuf][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trame_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TransparentZBuf][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trans_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_TransparentZBuf][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trans_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_FlatZBuf][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_flatz_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_FlatZBuf][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_flatz_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Trame][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trame_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Trame][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trame_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Transparent][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_trans_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Transparent][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_trans_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Flat][1b] slopes ---
		pushad
		push	dword ptr [Fill_CurY]
		push	dword ptr [Fill_RightSlope]
		push	dword ptr [Fill_LeftSlope]
		push	offset fmt_filler_flat_1b
		call	printf
		add	esp, 16
		popad
		; --- end debug trace ---
		; --- debug trace [ASM][Filler_Flat][1e] curvals ---
		pushad
		push	dword ptr [Fill_Patch]
		push	dword ptr [Fill_CurZBufMin]
		push	offset fmt_filler_flat_1e
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---

					lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	esi,[Fill_LeftSlope]
			mov	eax,[Fill_RightSlope]

			add	ebx,esi
			add	edx,eax

			add	edi,[ScreenPitch]
@@EnterLoop:
			mov	esi,ebx
			mov	ecx,edx

			shr	esi,16
			push	edi

			shr	ecx,16
			add	edi,esi

			sub	ecx,esi
			jle	@@EndScanLine

			mov	al,[edi]	; Load 1st dst cache line
			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache

			jb	@@SmallScanLine

				; *** Load dst in WB cache ***
			push	edi
			push	edx

			lea	edx,[edi+ecx]
			and	edi,not 31

			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edx
			pop	edi

			mov	eax,[Fill_Color]	; Color
			test	edi,1b			; alignment test
			je	@@ALIGN_Ok

			mov	[edi],al
			inc	edi
			dec	ecx
@@ALIGN_Ok:
			shr	ecx,1
			rep	stosw			; Should we make a rep stosd (and making the code more complicated for small polys ?)
			jnc	@@EndScanLine
			mov	[edi],al
			jmp	@@EndScanLine

				; *** Flat loop ***
@@SmallScanLine:
			mov	eax,[Fill_Color]	; Color
@@LoopX:
			mov	[edi],al
			inc	edi

			dec	ecx
			jne	@@LoopX

@@EndScanLine:
			pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			jmp	Triangle_ReadNextEdge
Filler_Flat		ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_Transparent	PROC

		; --- debug trace [ASM][Filler_Transparent][1] entry ---
		pushad
		push	offset fmt_f_transparent_1
		call	printf
		add	esp, 4
		popad
		; --- end debug trace ---

					lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	esi,[Fill_LeftSlope]
			mov	eax,[Fill_RightSlope]

			add	ebx,esi
			add	edx,eax

			add	edi,[ScreenPitch]
@@EnterLoop:
			mov	esi,ebx
			mov	ecx,edx

			shr	esi,16
			push	edi

			shr	ecx,16
			add	edi,esi

			sub	ecx,esi
			jle	@@EndScanLine

			mov	ah,byte ptr [Fill_Color]; Color
@@LoopX:
			mov	al,[edi]

			and	al,0Fh

			or	al,ah

			mov	[edi],al
			inc	edi

			dec	ecx
			jne	@@LoopX

@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			jmp	Triangle_ReadNextEdge
Filler_Transparent	ENDP



; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_Trame		PROC

		; --- debug trace [ASM][Filler_Trame][1] entry ---
		pushad
		push	offset fmt_f_trame_1
		call	printf
		add	esp, 4
		popad
		; --- end debug trace ---

					lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_Trame_Parity]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


			mov	al,[edi]	; Load 1st dst cache line
			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache

			jb	@@SmallScanLine

				; *** Load dst in WB cache ***
			push	edi
			push	edx

			lea	edx,[edi+ecx]
			and	edi,not 31

			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edx
			pop	edi
@@SmallScanLine:
			shr	ecx,1
			je	@@EndScanLine

			xor	esi,1
			mov	eax,edi

			and	eax,1
			inc	ecx

			xor	eax,esi

			mov	eax,[Fill_Color]
			je	@@Pair

			inc	edi
@@Pair:
			shr	ecx,1
			jnc	@@Odd
@@Each_X:
			mov	[edi],al
			add	edi,2
@@Odd:
			mov	[edi],al
			add	edi,2

			dec	ecx
			jne	@@Each_X


@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			xor	esi,1

			mov	[Fill_Trame_Parity],esi
			jmp	Triangle_ReadNextEdge
Filler_Trame		ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_FlatZBuf		PROC

		; --- debug trace [ASM][Filler_FlatZBuf][1] entry ---
		pushad
		push	offset fmt_f_flatzbuf_1
		call	printf
		add	esp, 4
		popad
		; --- end debug trace ---

					lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurZBufMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax
			mov	eax,[Log]

			mov	[PtrLog],eax
			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_ZBuf_LeftSlope]

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

			push	edx

			mov	al,[edi]	; Load 1st dst cache line
			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache

			jb	@@SmallScanLine

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
@@SmallScanLine:
				; *** Flat loop ***
			push	esi
			push	ebx

			mov	ebx,[Fill_ZBuf_XSlope]

			push	ebp
			mov	edx,esi

			shl	esi,32-8
			mov	ebp,ebx

			shl	ebp,32-8
			mov	eax,[Log]

			shr	ebx,8
			sub	edi,eax

			shr	edx,8
			mov	eax,[Fill_Color]	; Color
@@LoopX:
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
			inc	edi
			add	esi,ebp

			adc	edx,ebx
			dec	ecx

			jne	@@LoopX

			pop	ebp
			pop	ebx

			pop	esi
			pop	edx
@@EndScanLine:
			pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurZBufMin],esi
			mov	[Fill_CurXMin],ebx

			mov	[Fill_CurXMax],edx
			mov	[Fill_CurOffLine],edi

			jmp	Triangle_ReadNextEdge
Filler_FlatZBuf		ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TransparentZBuf	PROC

		; --- debug trace [ASM][Filler_TransparentZBuf][1] entry ---
		pushad
		push	offset fmt_f_transparentzbu_1
		call	printf
		add	esp, 4
		popad
		; --- end debug trace ---

					lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			dec	eax
			mov	esi,[Fill_CurZBufMin]

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	eax,[Log]

			mov	[PtrLog],eax
			mov	[PtrLog1],eax

			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_ZBuf_LeftSlope]

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

			push	ebx
			push	edx

			push	esi
			push	ebp

			mov	eax,[Log]
			mov	edx,esi

			shr	edx,8
			sub	edi,eax

			shl	esi,32-8
			mov	ebx,[Fill_ZBuf_XSlope]

			mov	ebp,ebx
			mov	ah,byte ptr [Fill_Color]; Color

			shr	ebx,8

			shl	ebp,32-8
@@LoopX:
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
		mov	al,[edi+ebx]
		pop ebx

			and	al,0Fh

			or	al,ah

		push ebx
		mov ebx,[PtrLog1]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp:		inc	edi
			add	esi,ebp

			adc	edx,ebx
			dec	ecx

			jne	@@LoopX

			pop	ebp
			pop	esi

			pop	edx
			pop	ebx
@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurZBufMin],esi
			jmp	Triangle_ReadNextEdge
Filler_TransparentZBuf	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TrameZBuf	PROC

		; --- debug trace [ASM][Filler_TrameZBuf][1] entry ---
		pushad
		push	offset fmt_f_tramezbuf_1
		call	printf
		add	esp, 4
		popad
		; --- end debug trace ---

					lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_Trame_Parity]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrZBuffer2],eax
			mov	[PtrZBuffer3],eax

			mov	eax,[Log]

			mov	[PtrLog],eax
			mov	[PtrLog2],eax

			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			push	ebx

			mov	ebx,[Fill_CurZBufMin]
			mov	eax,[Fill_ZBuf_LeftSlope]

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


			mov	al,[edi]	; Load 1st dst cache line
			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache

			jb	@@SmallScanLine

				; *** Load dst in WB cache ***
			push	edi
			push	edx

			lea	edx,[edi+ecx]
			and	edi,not 31

			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edx
			pop	edi
@@SmallScanLine:
			shr	ecx,1
			je	@@EndScanLine

			xor	esi,1
			mov	eax,edi

			and	eax,1
			inc	ecx

			xor	eax,esi
			je	@@Pair

			inc	edi
@@Pair:
			push	esi
			push	ebx

			push	edx
			push	ebp

			mov	esi,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_XSlope]

			add	ebx,ebx
			mov	edx,esi

			shl	esi,32-8
			mov	ebp,ebx

			shl	ebp,32-8
			mov	eax,[Log]

			shr	edx,8
			sub	edi,eax

			shr	ebx,8
			mov	eax,[Fill_Color]

			shr	ecx,1
			jnc	@@Odd
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp0

	push eax
	mov eax,[PtrZBuffer1]
	mov	[edi*2+eax],dx
	pop eax

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp0:		add	edi,2
			add	esi,ebp

			adc	edx,ebx
@@Odd:
	push eax
	mov eax,[PtrZBuffer2]
	cmp	[edi*2+eax],dx
	pop eax

			jb	@@NoDisp2

	push eax
	mov eax,[PtrZBuffer3]
	mov	[edi*2+eax],dx
	pop eax

		push ebx
		mov ebx,[PtrLog2]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp2:		add	edi,2

			dec	ecx
			jne	@@Each_X

			pop	ebp
			pop	edx

			pop	ebx
			pop	esi

@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			xor	esi,1

			mov	[Fill_Trame_Parity],esi
			jmp	Triangle_ReadNextEdge
Filler_TrameZBuf	ENDP








; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_FlatNZW		PROC

		; --- debug trace [ASM][Filler_FlatNZW][1] entry ---
		pushad
		push	offset fmt_f_flatnzw_1
		call	printf
		add	esp, 4
		popad
		; --- end debug trace ---

					lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurZBufMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	eax,[Log]

			mov	[PtrLog],eax
			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_ZBuf_LeftSlope]

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

			push	edx

			mov	al,[edi]	; Load 1st dst cache line
			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache

			jb	@@SmallScanLine

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
@@SmallScanLine:
				; *** Flat loop ***
			push	esi
			push	ebx

			mov	ebx,[Fill_ZBuf_XSlope]

			push	ebp
			mov	edx,esi

			shl	esi,32-8
			mov	ebp,ebx

			shl	ebp,32-8
			mov	eax,[Log]

			shr	ebx,8
			sub	edi,eax

			shr	edx,8
			mov	eax,[Fill_Color]	; Color
@@LoopX:
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
			inc	edi
			add	esi,ebp

			adc	edx,ebx
			dec	ecx

			jne	@@LoopX

			pop	ebp
			pop	ebx

			pop	esi
			pop	edx
@@EndScanLine:
			pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurZBufMin],esi
			mov	[Fill_CurXMin],ebx

			mov	[Fill_CurXMax],edx
			mov	[Fill_CurOffLine],edi

			jmp	Triangle_ReadNextEdge
Filler_FlatNZW		ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TransparentNZW	PROC

		; --- debug trace [ASM][Filler_TransparentNZW][1] entry ---
		pushad
		push	offset fmt_f_transparentnzw_1
		call	printf
		add	esp, 4
		popad
		; --- end debug trace ---

					lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			dec	eax
			mov	esi,[Fill_CurZBufMin]

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	eax,[Log]

			mov	[PtrLog],eax
			mov	[PtrLog1],eax

			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_ZBuf_LeftSlope]

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

			push	ebx
			push	edx

			push	esi
			push	ebp

			mov	eax,[Log]
			mov	edx,esi

			shr	edx,8
			sub	edi,eax

			shl	esi,32-8
			mov	ebx,[Fill_ZBuf_XSlope]

			mov	ebp,ebx
			mov	ah,byte ptr [Fill_Color]; Color

			shr	ebx,8

			shl	ebp,32-8
@@LoopX:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ebx
		mov ebx,[PtrLog]
		mov	al,[edi+ebx]
		pop ebx

			and	al,0Fh

			or	al,ah


		push ebx
		mov ebx,[PtrLog1]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp:		inc	edi
			add	esi,ebp

			adc	edx,ebx
			dec	ecx

			jne	@@LoopX

			pop	ebp
			pop	esi

			pop	edx
			pop	ebx
@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurZBufMin],esi
			jmp	Triangle_ReadNextEdge
Filler_TransparentNZW	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TrameNZW		PROC

		; --- debug trace [ASM][Filler_TrameNZW][1] entry ---
		pushad
		push	offset fmt_f_tramenzw_1
		call	printf
		add	esp, 4
		popad
		; --- end debug trace ---

					lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_Trame_Parity]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer2],eax

			mov	eax,[Log]

			mov	[PtrLog],eax
			mov	[PtrLog2],eax

			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			push	ebx

			mov	ebx,[Fill_CurZBufMin]
			mov	eax,[Fill_ZBuf_LeftSlope]

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


			mov	al,[edi]	; Load 1st dst cache line
			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache

			jb	@@SmallScanLine

				; *** Load dst in WB cache ***
			push	edi
			push	edx

			lea	edx,[edi+ecx]
			and	edi,not 31

			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edx
			pop	edi
@@SmallScanLine:
			shr	ecx,1
			je	@@EndScanLine

			xor	esi,1
			mov	eax,edi

			and	eax,1
			inc	ecx

			xor	eax,esi
			je	@@Pair

			inc	edi
@@Pair:
			push	esi
			push	ebx

			push	edx
			push	ebp

			mov	esi,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_XSlope]

			add	ebx,ebx
			mov	edx,esi

			shl	esi,32-8
			mov	ebp,ebx

			shl	ebp,32-8
			mov	eax,[Log]

			shr	edx,8
			sub	edi,eax

			shr	ebx,8
			mov	eax,[Fill_Color]

			shr	ecx,1
			jnc	@@Odd
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp0

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp0:		add	edi,2
			add	esi,ebp

			adc	edx,ebx
@@Odd:
	push eax
	mov eax,[PtrZBuffer2]
	cmp	[edi*2+eax],dx
	pop eax

			jb	@@NoDisp2

		push ebx
		mov ebx,[PtrLog2]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp2:		add	edi,2

			dec	ecx
			jne	@@Each_X

			pop	ebp
			pop	edx

			pop	ebx
			pop	esi

@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			xor	esi,1

			mov	[Fill_Trame_Parity],esi
			jmp	Triangle_ReadNextEdge
Filler_TrameNZW		ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_FlagZBuffer	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurZBufMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_ZBuf_LeftSlope]

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

			push	edx


				; *** Flat loop ***
			push	esi
			push	ebx

			mov	ebx,[Fill_ZBuf_XSlope]

			push	ebp
			mov	edx,esi

			shl	esi,32-8
			mov	ebp,ebx

			shl	ebp,32-8
			mov	eax,[Log]

			shr	ebx,8
			sub	edi,eax

			shr	edx,8
			xor	eax,eax
@@LoopX:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp
			mov	[IsPolygonHidden],eax
@@NoDisp:
			inc	edi
			add	esi,ebp

			adc	edx,ebx
			dec	ecx

			jne	@@LoopX

			pop	ebp
			pop	ebx

			pop	esi
			pop	edx
@@EndScanLine:
			pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurZBufMin],esi
			mov	[Fill_CurXMin],ebx

			mov	[Fill_CurXMax],edx
			mov	[Fill_CurOffLine],edi

			jmp	Triangle_ReadNextEdge
Filler_FlagZBuffer	ENDP

_TEXT			ENDS

;			The
			End
