; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         BOX Z BUFFER                               ***
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


			PUBLIC	C	ZBufBoxOverWrite2


;					*****************
;					*** VARIABLES ***
;					*****************

;					*** PUBLIC DATA ***


;					*** MODULE DATA ***




;			***************
;			*** INCLUDE ***
;			***************



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
Extrn	C		ScreenPitch	:	DWORD
Extrn	C		Fill_ZBuffer_Factor	:	DWORD
Extrn	C		Screen			:	DWORD
Extrn	C		Log			:	DWORD
Extrn	C		PtrZBuffer		:	DWORD
Extrn	C		TabOffLine		:	DWORD





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
IncZ			dd	0




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


;				*******************
;				*** Global proc ***
;				*******************

; ECX, EBX = XMin, XMax
; EDX, EAX = YMin, YMax
; ESI = Z High
; EDI = Z Bottom
ZBufBoxOverWrite2	PROC \
					uses ebx ecx edx edi esi ebp\
					z0:DWORD, z1:DWORD, xmin:DWORD, ymin:DWORD, xmax:DWORD, ymax:DWORD \

					mov edi, z0
					mov esi, z1
					mov ecx, xmin
					mov edx, ymin
					mov ebx, xmax
					mov eax, ymax

			push	ebp
			sub	ebx,ecx		; EBX = DeltaX-1

			sub	eax,edx
			inc	eax		; EAX = DeltaY

			imul	esi,[Fill_ZBuffer_Factor]; GET_ZO(Z0)
			imul	edi,[Fill_ZBuffer_Factor]; GET_ZO(Z1)

			mov	edx,[TabOffLine+edx*4]
			inc	ebx		; EBX = DeltaX

			push	eax		; Save DeltaY
			push	edx

			sub	edi,esi		; DeltaZBuf
			mov	ebp,eax

			mov	edx,edi
			mov	eax,edi

			sar	edx,31
			add	ecx,ebx		; ECX = XMin+DeltaX

			idiv	ebp

			mov	[IncZ],eax	; IncZBuf
			pop	edx

			pop	eax		; EAX = DeltaY
			mov	edi,[PtrZBuffer]

			add	edx,ecx
			mov	ebp,[Log]

			add	ebp,edx
			mov	ecx,[Screen]

			lea	edi,[edi+edx*2]
			xor	ebx,-1		; EBX = -DeltaX

			add	edx,ecx
			inc	ebx
						; EDI = PtrZBuffer
						; EDX = PtrScreen
						; EBP = PtrLog

			mov	ch,1		; CH our flag

						; ESI = Z 16:16
						; High word of EAX is obviously nul
						; (provided that the screen has less than 65535 lines)
@@LoopY:
			push	eax
			push	esi

			shr	esi,16
			push	ebx
@@LoopX:
			mov	ax,[edi+ebx*2]	; Read ZBuffer (no AGI)

			cmp	eax,esi		; Is our pixel hidden?
			jae	@@Hidden

			mov	cl,[edx+ebx]	; No: Copy it

			mov	[ebp+ebx],cl
						; Else perform next point
			inc	ebx
			jle	@@LoopX
			jmp	@@EndLoop

@@LoopXFast:
			mov	ax,[edi+ebx*2]	; Read ZBuffer (no AGI)

			cmp	eax,esi		; Is our pixel hidden?
			jae	@@Return

			mov	cl,[edx+ebx]	; No: Copy it
			mov	[ebp+ebx],cl
@@Return:					; Else perform next point
			inc	ebx
			jle	@@LoopXFast

@@EndLoop:
			pop	ebx
			mov	eax,[ScreenPitch]

			add	edx,eax		; Next line in our buffers
			add	ebp,eax

			lea	edi,[edi+2*eax]
			pop	esi

			mov	eax,[IncZ]

			add	esi,eax		; Next Z
			pop	eax

			dec	eax
			jne	@@LoopY


			xor	eax,eax
			pop	ebp

			mov	al,ch

			ret

@@Hidden:
			mov	ch,0
			jmp	@@Return
ZBufBoxOverWrite2	ENDP






;				*******************
;				*** Local proc ***
;				*******************



_TEXT			ENDS

;			The
			End
