;----------------------------------------------------------------------------
;				S_LINE.ASM 386
;			      (c) Adeline 1993-96
;----------------------------------------------------------------------------
            OPTION SCOPED

			.386
			.MODEL FLAT, C

			INCLUDE FILLER.INC

;----------------------------------------------------------------------------
			.DATA

;----------------------------------------------------------------------------
			extrn	C ClipXMin		:DWORD
			extrn	C ClipYMin		:DWORD
			extrn	C ClipXMax		:DWORD
			extrn	C ClipYMax		:DWORD
			extrn	C Log			:DWORD
			extrn	C PtrZBuffer		:DWORD
			extrn	C PTR_TabOffLine		:DWORD
			extrn	C ScreenPitch		:DWORD
			Extrn	C	Fill_Flag_Fog		:BYTE
			Extrn	C	Fill_Flag_ZBuffer	:BYTE
			Extrn	C	Fill_Flag_NZW		:BYTE
			Extrn	C	Fill_Logical_Palette	:BYTE

Z1			dd	0			; For the ZBuffer
Z2			dd	0
DZ			dd	0
Color			db	0

s_eax			dd	0
s_ebx			dd	0
s_ecx			dd	0
s_edx			dd	0
s_esi			dd	0
s_edi			dd	0
s_ebp			dd	0

; Runtime code patch refactor, uses memory instead of instruction substitution
LinePatch   DWORD 12345678h
PtrLog      DWORD 12345678h
PtrZBuf     DWORD 12345678h
PtrZBuf1    DWORD 12345678h
YPtrZBuf    DWORD 12345678h
YPtrZBuf1   DWORD 12345678h
YPtrLog     DWORD 12345678h
Row         DWORD 12345678h
YDX         DWORD 12345678h
YDY         DWORD 12345678h
YDY1        DWORD 12345678h
YDZ         DWORD 12345678h
DXPatch     DWORD 12345678h
DY          DWORD 12345678h
DZPatch     DWORD 12345678h
DX1         DWORD 12345678h
IZ          DWORD 12345678h
YIZ         DWORD 12345678h
ColorX      BYTE 12h
ColorY      BYTE 12h

;----------------------------------------------------------------------------
			.CODE

;----------------------------------------------------------------------------
			public	C	Line
			public	C	LineZBufNZW
			public	C	LineZBuf
			public	C	Line_A

;----------------------------------------------------------------------------
;		Line( x0, y0, x1, y1, coul )
;
Line			PROC	USES esi edi ebp ebx,\
			x0:DWORD, y0:DWORD,\
			x1:DWORD, y1:DWORD,\
			coul:DWORD

; 			X0:ax  Y0:bx
;			X1:cx  Y1:DX
;			couleur: bp

			mov	eax, x0
			mov	ebx, y0
			mov	ecx, x1
			mov	edx, y1

			mov	ebp, coul

			call	Line_Entry

			ret
Line			ENDP

;----------------------------------------------------------------------------
;		LineZBufNZW( x0, y0, z0, x1, y1, z1, coul )
;
LineZBufNZW		PROC	USES esi edi ebp ebx,\
			x0:DWORD, y0:DWORD, zb0:DWORD,\
			x1:DWORD, y1:DWORD, zb1:DWORD,\
			coul:DWORD

; 			X0:ax  Y0:bx
;			X1:cx  Y1:DX
;			couleur: bp

			mov	eax, x0
			mov	ebx, y0
			mov	edi, zb0
			mov	ecx, x1
			mov	edx, y1
			mov	esi, zb1
			mov	ebp, coul

			call	Line_ZBuffer_NZW

			ret
LineZBufNZW		ENDP


;----------------------------------------------------------------------------
;		LineZBufNZW( x0, y0, z0, x1, y1, z1, coul )
;
LineZBuf		PROC	USES esi edi ebp ebx,\
			x0:DWORD, y0:DWORD, zb0:DWORD,\
			x1:DWORD, y1:DWORD, zb1:DWORD,\
			coul:DWORD

; 			X0:ax  Y0:bx
;			X1:cx  Y1:DX
;			couleur: bp

			mov	eax, x0
			mov	ebx, y0
			mov	edi, zb0
			mov	ecx, x1
			mov	edx, y1
			mov	esi, zb1
			mov	ebp, coul

			call	Line_ZBuffer

			ret
LineZBuf		ENDP


; ╔════════════════════════════════════════════════════════════════════════╗
; ║   Trace de droite                                                      ║
; ╟────────────────────────────────────────────────────────────────────────╢
; ║ Appel : (EAX,EBX), (ECX,EDX) : points extremes                         ║
; ║         EBP = Color                                                    ║
; ║                                                                        ║
; ╚════════════════════════════════════════════════════════════════════════╝
			ALIGN	4
Line_A			PROC
			cmp	[Fill_Flag_NZW],TRUE
			je	Line_ZBuffer_NZW

			cmp	[Fill_Flag_ZBuffer],TRUE
			je	Line_ZBuffer
Line_Entry::
			push	eax
			mov	eax,ebp
			and	ebp,0FFh
			cmp	[Fill_Flag_Fog],TRUE
			jne	@@Pas_Coul_Convert
			mov	al,[Fill_Logical_Palette+ebp]
@@Pas_Coul_Convert:
			mov	[ColorX],al
			mov	[ColorY],al
			mov	[Color],al
			pop	eax


@@Trace_Still:
			mov	edi,ecx			; EDI = DX
			mov	esi,edx			; ESI = DY

			sub	edi,eax
			je	@@DX_Nul

			sub	esi,ebx
			je	@@DY_Nul

			cmp	eax,[ClipXMin]
			jge	@@Clip_X0
							; Case X1<XMin
			cmp	ecx,[ClipXMin]
			jl	@@End

			push	edx
			sub	eax,[ClipXMin]		; y=y1-(x1-XMin)*dy/dx
			imul	esi
			idiv	edi
			sub	ebx,eax
			mov	eax,[ClipXMin]
			pop	edx
			jmp	@@Trace_Still

@@Clip_X0:
			cmp	eax,[ClipXMax]
			jle	@@Clip_X1
							; Case X1>XMax
			cmp	ecx,[ClipXMax]
			jg	@@End

			push	edx
			sub	eax,[ClipXMax]		; y=y1-(x1-XMax)*dy/dx
			imul	esi
			idiv	edi
			sub	ebx,eax
			pop	edx
			mov	eax,[ClipXMax]
			jmp	@@Trace_Still

@@Clip_X1:
			cmp	ecx,[ClipXMin]
			jge	@@Clip_X2
							; Case X2<XMin

			push	eax
			mov	eax,ecx
			push	edx
			sub	eax,[ClipXMin]		; y=y1-(x1-XMin)*dy/dx
			imul	esi
			idiv	edi
			pop	edx
			sub	edx,eax
			mov	ecx,[ClipXMin]
			pop	eax
			jmp	@@Trace_Still

@@Clip_X2:
			cmp	ecx,[ClipXMax]
			jle	@@Clip_X3
							; Case X2>XMax
			push	eax
			push	edx
			mov	eax,ecx
			sub	eax,[ClipXMax]
			imul	esi
			idiv	edi
			pop	edx
			sub	edx,eax
			mov	ecx,[ClipXMax]
			pop	eax
			jmp	@@Trace_Still

@@Clip_X3:
			cmp	ebx,[ClipYMin]
			jge	@@Clip_Y0
							; Case Y1<YMin
			cmp	edx,[ClipYMin]
			jl	@@End

			push	edx
			push	eax
			mov	eax,[ClipYMin]
			sub	eax,ebx
			imul	edi
			idiv	esi
			pop	edx
			add	eax,edx
			mov	ebx,[ClipYMin]
			pop	edx
			jmp	@@Trace_Still


@@Clip_Y0:
			cmp	ebx,[ClipYMax]
			jle	@@Clip_Y1
							; Case Y1>YMax
			cmp	edx,[ClipYMax]
			jg	@@End

			push	edx
			push	eax
			mov	eax,[ClipYMax]
			sub	eax,ebx
			imul	edi
			idiv	esi
			pop	edx
			add	eax,edx
			mov	ebx,[ClipYMax]
			pop	edx
			jmp	@@Trace_Still

@@Clip_Y1:
			cmp	edx,[ClipYMin]
			jge	@@Clip_Y2
							; Case Y2<YMin

			push	eax
			mov	eax,[ClipYMin]
			sub	eax,edx
			imul	edi
			idiv	esi
			add	ecx,eax
			mov	edx,[ClipYMin]
			pop	eax
			jmp	@@Trace_Still


@@Clip_Y2:
			cmp	edx,[ClipYMax]
			jle	@@Clip_Y3
							; Case Y2>YMax
			push	eax
			mov	eax,[ClipYMax]
			sub	eax,edx
			imul	edi
			idiv	esi
			add	ecx,eax
			mov	edx,[ClipYMax]
			pop	eax
			jmp	@@Trace_Still

@@Clip_Y3:
			mov	edi,ecx			; EDI = ABS(Delta_X)

			mov	ebp,[ScreenPitch]; EBP = Incr_Y
			mov	ecx,1			; ECX = Incr_X

			sub	edi,eax
			jns	@@DeltaX_Pos
			neg	edi
			neg	ecx
@@DeltaX_Pos:
			mov	esi,edx			; ESI = ABS(Delta_Y)
			sub	esi,ebx
			jns	@@DeltaY_Pos
			neg	esi
			neg	ebp
@@DeltaY_Pos:
			shl	ebx,2
			add	ebx,[PTR_TabOffLine]

			mov	ebx,[ebx]

			add	ebx,eax
			add	ebx,[Log]

			cmp	edi,esi
			jle	@@Draw_Y

							; Delta_X > Delta_Y
			mov	edx,edi
			mov	eax,edi
			shr	edx,1
@@Do_X:
			push eax
			mov al,[ColorX]
			mov	byte ptr [ebx],al
			pop eax

			add	edx,esi

			cmp	edx,edi
			jb	@@Plot_X

			sub	edx,edi
			add	ebx,ebp
@@Plot_X:
			add	ebx,ecx
			dec	eax

			jge	@@Do_X

			ret


@@Draw_Y:
			mov	edx,esi
			shr	edx,1
			mov	eax,esi

@@Do_Y:
			push eax
			mov al,[ColorY]
			mov	byte ptr [ebx],al
			pop eax
			add	edx,edi

			cmp	edx,esi
			jb	@@Plot_Y

			sub	edx,esi
			add	ebx,ecx
@@Plot_Y:
			add	ebx,ebp
			dec	eax
			jge	@@Do_Y
@@End:
			ret


					; *** DeltaX nul ***
@@DX_Nul:
			cmp	eax,[ClipXMin]
			jl	@@End
			cmp	eax,[ClipXMax]
			jg	@@End

			sub	esi,ebx
			je	@@Plot

			cmp	ebx,[ClipYMin]
			jge	@@DX_Y1
			cmp	edx,[ClipYMin]
			jl	@@End
			mov	ebx,[ClipYMin]
@@DX_Y1:
			cmp	ebx,[ClipYMax]
			jle	@@DX_Y1b
			cmp	edx,[ClipYMax]
			jg	@@End
			mov	ebx,[ClipYMax]
@@DX_Y1b:

			cmp	edx,[ClipYMin]
			jge	@@DX_Y2
			mov	edx,[ClipYMin]
@@DX_Y2:
			cmp	edx,[ClipYMax]
			jle	@@DX_Y2b
			mov	edx,[ClipYMax]
@@DX_Y2b:
			mov	esi,edx
			sub	esi,ebx
			jns	@@DX_Ok
			neg	esi
			mov	ebx,edx
@@DX_Ok:
			shl	ebx,2
			add	ebx,[PTR_TabOffLine]
			mov	ebp,[ScreenPitch]
			mov	al,[Color]
			mov	ebx,[ebx]
			inc	esi
			add	ebx,[Log]
			add	ebx,ecx
@@Burst_Y:
			mov	[ebx],al
			add	ebx,ebp
			dec	esi
			jne	@@Burst_Y

			ret

					; *** DeltaY nul ***
@@DY_Nul:
			cmp	ebx,[ClipYMin]
			jl	@@End
			cmp	ebx,[ClipYMax]
			jg	@@End

			cmp	eax,[ClipXMin]
			jge	@@DY_X1
			cmp	ecx,[ClipXMin]
			jl	@@End
			mov	eax,[ClipXMin]
@@DY_X1:
			cmp	eax,[ClipXMax]
			jle	@@DY_X1b
			cmp	ecx,[ClipXMax]
			jg	@@End
			mov	eax,[ClipXMax]
@@DY_X1b:

			cmp	ecx,[ClipXMin]
			jge	@@DY_X2
			mov	ecx,[ClipXMin]
@@DY_X2:
			cmp	ecx,[ClipXMax]
			jle	@@DY_X2b
			mov	ecx,[ClipXMax]
@@DY_X2b:
			mov	edi,ecx
			sub	edi,eax
			jns	@@DY_Ok
			neg	edi
			mov	eax,ecx
@@DY_Ok:
			shl	ebx,2
			add	ebx,[PTR_TabOffLine]
			mov	cl,[Color]
			mov	ebx,[ebx]
			inc	edi
			add	ebx,eax
			add	ebx,[Log]
@@Burst_X:
			mov	[ebx],cl
			inc	ebx
			dec	edi
			jne	@@Burst_X
			ret

					; *** Plot ***
@@Plot:
			cmp	ebx,[ClipYMin]
			jl	@@End
			cmp	ebx,[ClipYMax]
			jg	@@End

			shl	ebx,2
			add	ebx,[PTR_TabOffLine]
			mov	ebx,[ebx]
			add	ebx,eax
			mov	al,[Color]
			add	ebx,[Log]
			mov	[ebx],al

			ret
Line_A			ENDP






; ╔════════════════════════════════════════════════════════════════════════╗
; ║   Trace de droite Zbuffer                                              ║
; ╟────────────────────────────────────────────────────────────────────────╢
; ║ Appel : (EAX,EBX), (ECX,EDX) : points extremes                         ║
; ║         EBP = Color                                                    ║
; ║         EDI = Z1                                                       ║
; ║         ESI = Z2                                                       ║
; ╚════════════════════════════════════════════════════════════════════════╝
			ALIGN	4

Line_ZBuffer		PROC

;			mov	s_eax, eax
;			mov	s_ebx, ebx
;			mov	s_ecx, ecx
;			mov	s_edx, edx
;			mov	s_esi, esi
;			mov	s_edi, edi
;			mov	s_ebp, ebp

			shl	esi,16
			shl	edi,16
			mov	[Z1],edi
			mov	[Z2],esi


			push	eax
			mov	eax,ebp
			and	ebp,0FFh
			cmp	[Fill_Flag_Fog],TRUE
			jne	@@Pas_Coul_Convert
			mov	al,[Fill_Logical_Palette+ebp]
@@Pas_Coul_Convert:
			mov	[Color],al
			pop	eax

@@Trace_Still:
			mov	edi,[Z1]
			mov	esi,[Z2]

			sub	esi,edi

        		mov	[DZ],esi

			mov	edi,ecx			; EDI = DX
			mov	esi,edx			; ESI = DY

			sub	edi,eax
			je	@@DX_Nul

			sub	esi,ebx
			je	@@DY_Nul


			cmp	eax,[ClipXMin]
			jge	@@Clip_X0
							; Case X1<XMin
			cmp	ecx,[ClipXMin]
			jl	@@End


			push	edx
			sub	eax,[ClipXMin]		; y=y1-(x1-XMin)*dy/dx
			push	eax
			imul	[DZ]
			idiv	edi
			sub	[Z1],eax
			pop	eax
			imul	esi
			idiv	edi
			sub	ebx,eax
			mov	eax,[ClipXMin]
			pop	edx
			jmp	@@Trace_Still

@@Clip_X0:
			cmp	eax,[ClipXMax]
			jle	@@Clip_X1
							; Case X1>XMax
			cmp	ecx,[ClipXMax]
			jg	@@End


			push	edx
			sub	eax,[ClipXMax]		; y=y1-(x1-XMax)*dy/dx
			push	eax
			imul	[DZ]
			idiv	edi
			sub	[Z1],eax
			pop	eax
			imul	esi
			idiv	edi
			sub	ebx,eax
			pop	edx
			mov	eax,[ClipXMax]
			jmp	@@Trace_Still

@@Clip_X1:
			cmp	ecx,[ClipXMin]
			jge	@@Clip_X2
							; Case X2<XMin

			push	eax
			push	edx
			mov	eax,ecx
			sub	eax,[ClipXMin]		; y=y1-(x1-XMin)*dy/dx
			push	eax
			imul	[DZ]
			idiv	edi
			sub	[Z2],eax
			pop	eax
			imul	esi
			idiv	edi
			pop	edx
			sub	edx,eax
			mov	ecx,[ClipXMin]
			pop	eax
			jmp	@@Trace_Still

@@Clip_X2:
			cmp	ecx,[ClipXMax]
			jle	@@Clip_X3
							; Case X2>XMax
			push	eax
			push	edx
			mov	eax,ecx
			sub	eax,[ClipXMax]
			push	eax
			imul	[DZ]
			idiv	edi
			sub	[Z2],eax
			pop	eax
			imul	esi
			idiv	edi
			pop	edx
			sub	edx,eax
			mov	ecx,[ClipXMax]
			pop	eax
			jmp	@@Trace_Still

@@Clip_X3:
			cmp	ebx,[ClipYMin]
			jge	@@Clip_Y0
							; Case Y1<YMin
			cmp	edx,[ClipYMin]
			jl	@@End

			push	edx
			push	eax
			mov	eax,[ClipYMin]
			sub	eax,ebx
			push	eax
			imul	[DZ]
			idiv	esi
			add	[Z1],eax
			pop	eax
			imul	edi
			idiv	esi
			pop	edx
			add	eax,edx
			mov	ebx,[ClipYMin]
			pop	edx
			jmp	@@Trace_Still


@@Clip_Y0:
			cmp	ebx,[ClipYMax]
			jle	@@Clip_Y1
							; Case Y1>YMax
			cmp	edx,[ClipYMax]
			jg	@@End


			push	edx
			push	eax
			mov	eax,[ClipYMax]
			sub	eax,ebx
			push	eax
			imul	[DZ]
			idiv	esi
			add	[Z1],eax
			pop	eax
			imul	edi
			idiv	esi
			pop	edx
			add	eax,edx
			mov	ebx,[ClipYMax]
			pop	edx
			jmp	@@Trace_Still

@@Clip_Y1:
			cmp	edx,[ClipYMin]
			jge	@@Clip_Y2
							; Case Y2<YMin

			push	eax
			mov	eax,[ClipYMin]
			sub	eax,edx
			push	eax
			imul	[DZ]
			idiv	esi
			add	[Z2],eax
			pop	eax
			imul	edi
			idiv	esi
			add	ecx,eax
			mov	edx,[ClipYMin]
			pop	eax
			jmp	@@Trace_Still


@@Clip_Y2:
			cmp	edx,[ClipYMax]
			jle	@@Clip_Y3

							; Case Y2>YMax
			push	eax
			mov	eax,[ClipYMax]
			sub	eax,edx
			push	eax
			imul	[DZ]
			idiv	esi
			add	[Z2],eax
			pop	eax
			imul	edi
			idiv	esi
			add	ecx,eax
			mov	edx,[ClipYMax]
			pop	eax
			jmp	@@Trace_Still

@@Clip_Y3:
@@Draw_Line:
			mov	edi,ecx			; EDI = ABS(Delta_X)

			mov	ebp,[ScreenPitch]; EBP = Incr_Y
			mov	ecx,1			; ECX = Incr_X

			sub	edi,eax
			jns	@@DeltaX_Pos
			neg	edi
			neg	ecx
@@DeltaX_Pos:
			mov	esi,edx			; ESI = ABS(Delta_Y)
			sub	esi,ebx
			jns	@@DeltaY_Pos
			neg	esi
			neg	ebp
@@DeltaY_Pos:
			shl	ebx,2
			add	ebx,[PTR_TabOffLine]

			mov	ebx,[ebx]

			add	ebx,eax

			cmp	edi,esi
			jl	@@Draw_Y

							; Delta_X > Delta_Y
			mov	[DY],esi
			mov	[DXPatch],edi
			mov	[DX1],edi

			mov	eax,[DZ]
			test	ecx,ecx
			jns	@@Pos1
			neg	eax
@@Pos1:
			mov	edx,eax
			sar	edx,31
			idiv	edi
			mov	edx,eax
			shl	eax,16
			sar	edx,16
			mov	[DZPatch],eax
			mov	[IZ],edx

			mov	[LinePatch],ebp

			mov	eax,[Log]
			mov	[PtrLog],eax
			mov	eax,[PtrZBuffer]
			mov	[PtrZBuf],eax
			mov	[PtrZBuf1],eax

			mov	edx,edi
			mov	ebp,edi
			shr	edx,1

			mov	al,[Color]
			mov	esi,[Z1]
			mov	edi,esi
			shl	esi,16
			sar	edi,16

			jmp	$+2

@@Do_X:

		push eax
		mov eax,[PtrZBuf]
		cmp	word ptr [ebx*2+eax],di
		pop eax

			jb	@@NoDisp

		push eax
		mov eax,[PtrZBuf1]
		mov	word ptr [ebx*2+eax],di
		pop eax

		push ecx
		mov ecx,[PtrLog]
		mov	byte ptr [ebx+ecx],al
		pop ecx

@@NoDisp:
			add	edx,[DY]

			cmp	edx,[DXPatch]
			jb	@@Plot_X

			sub	edx,[DX1]
			add	ebx,[LinePatch]
@@Plot_X:
			add	ebx,ecx

			add	esi,[DZPatch]
			adc	edi,[IZ]

			dec	ebp
			jge	@@Do_X

			ret


@@Draw_Y:
			mov	[YDX],edi
			mov	[YDY],esi
			mov	[YDY1],esi

			mov	eax,[DZ]
			test	ebp,ebp
			jns	@@Pos2
			neg	eax
@@Pos2:
			mov	edx,eax
			sar	edx,31
			idiv	esi
			mov	edx,eax
			shl	eax,16
			sar	edx,16
			mov	[YDZ],eax
			mov	[YIZ],edx

			mov	[Row],ecx

			mov	eax,[Log]
			mov	[YPtrLog],eax
			mov	eax,[PtrZBuffer]
			mov	[YPtrZBuf],eax
			mov	[YPtrZBuf1],eax

			mov	edx,esi
			mov	ecx,esi
			shr	edx,1

			mov	al,[Color]
			mov	esi,[Z1]
			mov	edi,esi
			shl	esi,16
			sar	edi,16

			jmp	$+2

@@Do_Y:
		push eax
		mov eax,[YPtrZBuf]
		cmp	word ptr [ebx*2+eax],di
		pop eax
			jb	@@YNoDisp

		push eax
		mov eax,[YPtrZBuf1]
		mov	word ptr [ebx*2+eax],di
		pop eax


		push ecx
		mov ecx,[YPtrLog]
		mov	byte ptr [ebx+ecx],al
		pop ecx

@@YNoDisp:
			add	edx,[YDX]

			cmp	edx,[YDY]
			jb	@@Plot_Y

			sub	edx,[YDY1]
			add	ebx,[Row]
@@Plot_Y:
			add	esi,[YDZ]
			adc	edi,[YIZ]

			add	ebx,ebp

			dec	ecx
			jge	@@Do_Y
@@End:
			ret




					; *** DeltaX nul ***
@@DX_Nul:
			cmp	eax,[ClipXMin]
			jl	@@End
			cmp	eax,[ClipXMax]
			jg	@@End

			sub	esi,ebx
			je	@@Plot

			cmp	ebx,[ClipYMin]
			jge	@@DX_Y1
			cmp	edx,[ClipYMin]
			jl	@@End

			push	edx
			push	eax
			mov	eax,[ClipYMin]
			sub	eax,ebx
			imul	[DZ]
			idiv	esi
			add	[Z1],eax
			mov	ebx,[ClipYMin]
			pop	eax
			pop	edx
			jmp	@@Trace_Still

@@DX_Y1:
			cmp	ebx,[ClipYMax]
			jle	@@DX_Y1b
			cmp	edx,[ClipYMax]
			jg	@@End

			push	edx
			push	eax
			mov	eax,[ClipYMax]
			sub	eax,ebx
			imul	[DZ]
			idiv	esi
			add	[Z1],eax
			pop	eax
			pop	edx
			mov	ebx,[ClipYMax]
			jmp	@@Trace_Still
@@DX_Y1b:

			cmp	edx,[ClipYMin]
			jge	@@DX_Y2

			push	eax
			mov	eax,[ClipYMin]
			sub	eax,edx
			imul	[DZ]
			idiv	esi
			add	[Z2],eax
			mov	edx,[ClipYMin]
			pop	eax
			jmp	@@Trace_Still

@@DX_Y2:
			cmp	edx,[ClipYMax]
			jle	@@Draw_Line

			push	eax
			mov	eax,[ClipYMax]
			sub	eax,edx
			imul	[DZ]
			idiv	esi
			add	[Z2],eax
			mov	edx,[ClipYMax]
			pop	eax
			jmp	@@Trace_Still



					; *** DeltaY nul ***
@@DY_Nul:
			cmp	ebx,[ClipYMin]
			jl	@@End
			cmp	ebx,[ClipYMax]
			jg	@@End

			cmp	eax,[ClipXMin]
			jge	@@DY_X1
			cmp	ecx,[ClipXMin]
			jl	@@End

			push	edx
			sub	eax,[ClipXMin]		; y=y1-(x1-XMin)*dy/dx
			imul	[DZ]
			idiv	edi
			sub	[Z1],eax
			mov	eax,[ClipXMin]
			pop	edx
			jmp	@@Trace_Still

@@DY_X1:
			cmp	eax,[ClipXMax]
			jle	@@DY_X1b
			cmp	ecx,[ClipXMin]
			jg	@@End

			push	edx
			sub	eax,[ClipXMax]		; y=y1-(x1-XMax)*dy/dx
			imul	[DZ]
			idiv	edi
			sub	[Z1],eax
			pop	edx
			mov	eax,[ClipXMax]
			jmp	@@Trace_Still
@@DY_X1b:

			cmp	ecx,[ClipXMin]
			jge	@@DY_X2

			push	eax
			push	edx
			mov	eax,ecx
			sub	eax,[ClipXMin]		; y=y1-(x1-XMin)*dy/dx
			imul	[DZ]
			idiv	edi
			sub	[Z2],eax
			pop	edx
			pop	eax
			mov	ecx,[ClipXMin]
			jmp	@@Trace_Still
@@DY_X2:
			cmp	ecx,[ClipXMax]
			jle	@@Draw_Line

			push	eax
			push	edx
			mov	eax,ecx
			sub	eax,[ClipXMax]
			imul	[DZ]
			idiv	edi
			sub	[Z2],eax
			pop	edx
			pop	eax
			mov	ecx,[ClipXMax]
			jmp	@@Trace_Still

					; *** Plot ***
@@Plot:
			cmp	ebx,[ClipYMin]
			jl	@@End
			cmp	ebx,[ClipYMax]
			jg	@@End

			shl	ebx,2
			add	ebx,[PTR_TabOffLine]
			mov	ebx,[ebx]
			add	ebx,eax
			mov	al,[Color]
			add	ebx,[Log]
			mov	[ebx],al

			ret

;			mov	eax, s_eax
;			mov	ebx, s_ebx
;			mov	ecx, s_ecx
;			mov	edx, s_edx
;			mov	esi, s_esi
;			mov	edi, s_edi
;			mov	ebp, s_ebp

Line_ZBuffer		ENDP




; ╔════════════════════════════════════════════════════════════════════════╗
; ║   Trace de droite Zbuffer No ZBuf write                                ║
; ╟────────────────────────────────────────────────────────────────────────╢
; ║ Appel : (EAX,EBX), (ECX,EDX) : points extremes                         ║
; ║         EBP = Color                                                    ║
; ║         EDI = Z1                                                       ║
; ║         ESI = Z2                                                       ║
; ╚════════════════════════════════════════════════════════════════════════╝
			ALIGN	4
Line_ZBuffer_NZW	PROC

;			mov	s_eax, eax
;			mov	s_ebx, ebx
;			mov	s_ecx, ecx
;			mov	s_edx, edx
;			mov	s_esi, esi
;			mov	s_edi, edi
;			mov	s_ebp, ebp

			shl	esi,16
			shl	edi,16
			mov	[Z1],edi
			mov	[Z2],esi


			push	eax
			mov	eax,ebp
			and	ebp,0FFh
			cmp	[Fill_Flag_Fog],TRUE
			jne	@@Pas_Coul_Convert
			mov	al,[Fill_Logical_Palette+ebp]
@@Pas_Coul_Convert:
			mov	[Color],al
			pop	eax

@@Trace_Still:
			mov	edi,[Z1]
			mov	esi,[Z2]

			sub	esi,edi

        		mov	[DZ],esi

			mov	edi,ecx			; EDI = DX
			mov	esi,edx			; ESI = DY

			sub	edi,eax
			je	@@DX_Nul

			sub	esi,ebx
			je	@@DY_Nul

			cmp	eax,[ClipXMin]
			jge	@@Clip_X0
							; Case X1<XMin
			cmp	ecx,[ClipXMin]
			jl	@@End


			push	edx
			sub	eax,[ClipXMin]		; y=y1-(x1-XMin)*dy/dx
			push	eax
			imul	[DZ]
			idiv	edi
			sub	[Z1],eax
			pop	eax
			imul	esi
			idiv	edi
			sub	ebx,eax
			mov	eax,[ClipXMin]
			pop	edx
			jmp	@@Trace_Still

@@Clip_X0:
			cmp	eax,[ClipXMax]
			jle	@@Clip_X1
							; Case X1>XMax
			cmp	ecx,[ClipXMax]
			jg	@@End


			push	edx
			sub	eax,[ClipXMax]		; y=y1-(x1-XMax)*dy/dx
			push	eax
			imul	[DZ]
			idiv	edi
			sub	[Z1],eax
			pop	eax
			imul	esi
			idiv	edi
			sub	ebx,eax
			pop	edx
			mov	eax,[ClipXMax]
			jmp	@@Trace_Still

@@Clip_X1:
			cmp	ecx,[ClipXMin]
			jge	@@Clip_X2
							; Case X2<XMin

			push	eax
			push	edx
			mov	eax,ecx
			sub	eax,[ClipXMin]		; y=y1-(x1-XMin)*dy/dx
			push	eax
			imul	[DZ]
			idiv	edi
			sub	[Z2],eax
			pop	eax
			imul	esi
			idiv	edi
			pop	edx
			sub	edx,eax
			mov	ecx,[ClipXMin]
			pop	eax
			jmp	@@Trace_Still

@@Clip_X2:
			cmp	ecx,[ClipXMax]
			jle	@@Clip_X3
							; Case X2>XMax
			push	eax
			push	edx
			mov	eax,ecx
			sub	eax,[ClipXMax]
			push	eax
			imul	[DZ]
			idiv	edi
			sub	[Z2],eax
			pop	eax
			imul	esi
			idiv	edi
			pop	edx
			sub	edx,eax
			mov	ecx,[ClipXMax]
			pop	eax
			jmp	@@Trace_Still

@@Clip_X3:
			cmp	ebx,[ClipYMin]
			jge	@@Clip_Y0
							; Case Y1<YMin
			cmp	edx,[ClipYMin]
			jl	@@End

			push	edx
			push	eax
			mov	eax,[ClipYMin]
			sub	eax,ebx
			push	eax
			imul	[DZ]
			idiv	esi
			add	[Z1],eax
			pop	eax
			imul	edi
			idiv	esi
			pop	edx
			add	eax,edx
			mov	ebx,[ClipYMin]
			pop	edx
			jmp	@@Trace_Still


@@Clip_Y0:
			cmp	ebx,[ClipYMax]
			jle	@@Clip_Y1
							; Case Y1>YMax
			cmp	edx,[ClipYMax]
			jg	@@End


			push	edx
			push	eax
			mov	eax,[ClipYMax]
			sub	eax,ebx
			push	eax
			imul	[DZ]
			idiv	esi
			add	[Z1],eax
			pop	eax
			imul	edi
			idiv	esi
			pop	edx
			add	eax,edx
			mov	ebx,[ClipYMax]
			pop	edx
			jmp	@@Trace_Still

@@Clip_Y1:
			cmp	edx,[ClipYMin]
			jge	@@Clip_Y2
							; Case Y2<YMin

			push	eax
			mov	eax,[ClipYMin]
			sub	eax,edx
			push	eax
			imul	[DZ]
			idiv	esi
			add	[Z2],eax
			pop	eax
			imul	edi
			idiv	esi
			add	ecx,eax
			mov	edx,[ClipYMin]
			pop	eax
			jmp	@@Trace_Still


@@Clip_Y2:
			cmp	edx,[ClipYMax]
			jle	@@Clip_Y3

							; Case Y2>YMax
			push	eax
			mov	eax,[ClipYMax]
			sub	eax,edx
			push	eax
			imul	[DZ]
			idiv	esi
			add	[Z2],eax
			pop	eax
			imul	edi
			idiv	esi
			add	ecx,eax
			mov	edx,[ClipYMax]
			pop	eax
			jmp	@@Trace_Still

@@Clip_Y3:
@@Draw_Line:
			mov	edi,ecx			; EDI = ABS(Delta_X)

			mov	ebp,[ScreenPitch]; EBP = Incr_Y
			mov	ecx,1			; ECX = Incr_X

			sub	edi,eax
			jns	@@DeltaX_Pos
			neg	edi
			neg	ecx
@@DeltaX_Pos:
			mov	esi,edx			; ESI = ABS(Delta_Y)
			sub	esi,ebx
			jns	@@DeltaY_Pos
			neg	esi
			neg	ebp
@@DeltaY_Pos:
			shl	ebx,2
			add	ebx,[PTR_TabOffLine]

			mov	ebx,[ebx]

			add	ebx,eax

			cmp	edi,esi
			jl	@@Draw_Y

							; Delta_X > Delta_Y
			mov	[DY],esi
			mov	[DXPatch],edi
			mov	[DX1],edi

			mov	eax,[DZ]
			test	ecx,ecx
			jns	@@Pos1
			neg	eax
@@Pos1:
			mov	edx,eax
			sar	edx,31
			idiv	edi
			mov	edx,eax
			shl	eax,16
			sar	edx,16
			mov	[DZPatch],eax
			mov	[IZ],edx

			mov	[LinePatch],ebp

			mov	eax,[Log]
			mov	[PtrLog],eax
			mov	eax,[PtrZBuffer]
			mov	[PtrZBuf],eax

			mov	edx,edi
			mov	ebp,edi
			shr	edx,1

			mov	esi,[Z1]
			mov	edi,esi
			shl	esi,16
			mov	al,[Color]
			sar	edi,16

			jmp	$+2

@@Do_X:

		push eax
		mov eax,[PtrZBuf]
		cmp	word ptr [ebx*2+eax],di
		pop eax

			jb	@@NoDisp

		push ecx
		mov ecx,[PtrLog]
		mov	byte ptr [ebx+ecx],al
		pop ecx

@@NoDisp:
			add	edx,[DY]

			cmp	edx,[DXPatch]
			jb	@@Plot_X

			sub	edx,[DX1]
			add	ebx,[LinePatch]
@@Plot_X:
			add	ebx,ecx

			add	esi,[DZPatch]
			adc	edi,[IZ]

			dec	ebp
			jge	@@Do_X

			ret


@@Draw_Y:
			mov	[YDX],edi
			mov	[YDY],esi
			mov	[YDY1],esi

			mov	eax,[DZ]
			test	ebp,ebp
			jns	@@Pos2
			neg	eax
@@Pos2:
			mov	edx,eax
			sar	edx,31
			idiv	esi
			mov	edx,eax
			shl	eax,16
			sar	edx,16
			mov	[YDZ],eax
			mov	[YIZ],edx

			mov	[Row],ecx

			mov	eax,[Log]
			mov	[YPtrLog],eax
			mov	eax,[PtrZBuffer]
			mov	[YPtrZBuf],eax

			mov	edx,esi
			mov	ecx,esi
			shr	edx,1

			mov	esi,[Z1]
			mov	al,[Color]
			mov	edi,esi
			shl	esi,16
			sar	edi,16

			jmp	$+2

@@Do_Y:
		push eax
		mov eax,[YPtrZBuf]
		cmp	word ptr [ebx*2+eax],di
		pop eax

			jb	@@YNoDisp

		push eax
		mov eax,[YPtrLog]
		mov	byte ptr [ebx+eax],al
		pop eax

@@YNoDisp:
			add	edx,[YDX]

			cmp	edx,[YDY]
			jb	@@Plot_Y

			sub	edx,[YDY1]
			add	ebx,[Row]
@@Plot_Y:
			add	esi,[YDZ]
			adc	edi,[YIZ]

			add	ebx,ebp

			dec	ecx
			jge	@@Do_Y
@@End:
			ret

					; *** DeltaX nul ***
@@DX_Nul:
			cmp	eax,[ClipXMin]
			jl	@@End
			cmp	eax,[ClipXMax]
			jg	@@End

			sub	esi,ebx
			je	@@Plot

			cmp	ebx,[ClipYMin]
			jge	@@DX_Y1
			cmp	edx,[ClipYMin]
			jl	@@End

			push	edx
			push	eax
			mov	eax,[ClipYMin]
			sub	eax,ebx
			imul	[DZ]
			idiv	esi
			add	[Z1],eax
			mov	ebx,[ClipYMin]
			pop	eax
			pop	edx
			jmp	@@Trace_Still
@@DX_Y1:
			cmp	ebx,[ClipYMax]
			jle	@@DX_Y1b
			cmp	edx,[ClipYMax]
			jg	@@End

			push	edx
			push	eax
			mov	eax,[ClipYMax]
			sub	eax,ebx
			imul	[DZ]
			idiv	esi
			add	[Z1],eax
			pop	eax
			pop	edx
			mov	ebx,[ClipYMax]
			jmp	@@Trace_Still
@@DX_Y1b:

			cmp	edx,[ClipYMin]
			jge	@@DX_Y2

			push	eax
			mov	eax,[ClipYMin]
			sub	eax,edx
			imul	[DZ]
			idiv	esi
			add	[Z2],eax
			mov	edx,[ClipYMin]
			pop	eax
			jmp	@@Trace_Still

@@DX_Y2:
			cmp	edx,[ClipYMax]
			jle	@@Draw_Line

			push	eax
			mov	eax,[ClipYMax]
			sub	eax,edx
			imul	[DZ]
			idiv	esi
			add	[Z2],eax
			mov	edx,[ClipYMax]
			pop	eax
			jmp	@@Trace_Still



					; *** DeltaY nul ***
@@DY_Nul:
			cmp	ebx,[ClipYMin]
			jl	@@End
			cmp	ebx,[ClipYMax]
			jg	@@End

			cmp	eax,[ClipXMin]
			jge	@@DY_X1
			cmp	ecx,[ClipXMin]
			jl	@@End

			push	edx
			sub	eax,[ClipXMin]		; y=y1-(x1-XMin)*dy/dx
			imul	[DZ]
			idiv	edi
			sub	[Z1],eax
			mov	eax,[ClipXMin]
			pop	edx
			jmp	@@Trace_Still
@@DY_X1:
			cmp	eax,[ClipXMax]
			jle	@@DY_X1b
			cmp	ecx,[ClipXMin]
			jg	@@End

			push	edx
			sub	eax,[ClipXMax]		; y=y1-(x1-XMax)*dy/dx
			imul	[DZ]
			idiv	edi
			sub	[Z1],eax
			pop	edx
			mov	eax,[ClipXMax]
			jmp	@@Trace_Still
@@DY_X1b:

			cmp	ecx,[ClipXMin]
			jge	@@DY_X2

			push	eax
			push	edx
			mov	eax,ecx
			sub	eax,[ClipXMin]		; y=y1-(x1-XMin)*dy/dx
			imul	[DZ]
			idiv	edi
			sub	[Z2],eax
			pop	edx
			pop	eax
			mov	ecx,[ClipXMin]
			jmp	@@Trace_Still
@@DY_X2:
			cmp	ecx,[ClipXMax]
			jle	@@Draw_Line

			push	eax
			push	edx
			mov	eax,ecx
			sub	eax,[ClipXMax]
			imul	[DZ]
			idiv	edi
			sub	[Z2],eax
			pop	edx
			pop	eax
			mov	ecx,[ClipXMax]
			jmp	@@Trace_Still

					; *** Plot ***
@@Plot:
			cmp	ebx,[ClipYMin]
			jl	@@End
			cmp	ebx,[ClipYMax]
			jg	@@End

			shl	ebx,2
			add	ebx,[PTR_TabOffLine]
			mov	ebx,[ebx]
			add	ebx,eax
			mov	al,[Color]
			add	ebx,[Log]
			mov	[ebx],al

			ret

Line_ZBuffer_NZW	ENDP

;			The
			End
