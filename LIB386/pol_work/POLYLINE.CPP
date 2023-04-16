/*
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
*/

#include <SVGA/FIL_LINE.H>
#include <POLYGON/POLY.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <stdio.h>

S32 Z1 = 0;			// For the ZBuffer
S32 Z2 = 0;
S32 DZ = 0;
U8 Color = 0;

// Runtime code patch refactor, uses memory instead of instruction substitution
U32 LinePatch = 0x12345678;
U32 PtrLog = 0x12345678;
U32 PtrZBuf = 0x12345678;
U32 PtrZBuf1 = 0x12345678;
U32 YPtrZBuf = 0x12345678;
U32 YPtrZBuf1 = 0x12345678;
U32 YPtrLog = 0x12345678;
U32 Row = 0x12345678;
U32 YDX = 0x12345678;
U32 YDY = 0x12345678;
U32 YDY1 = 0x12345678;
U32 YDZ = 0x12345678;
U32 DXPatch = 0x12345678;
U32 DY = 0x12345678;
U32 DZPatch = 0x12345678;
U32 DX1 = 0x12345678;
U32 IZ = 0x12345678;
U32 YIZ = 0x12345678;
U8 ColorX = 0x12;
U8 ColorY = 0x12;


void Line_ZBuffer(S32 x0, S32 y0, S32 x1, S32 y1, S32 col, S32 z1, S32 z2)
{
	/*
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
	*/
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}

void Line_ZBuffer_NZW(S32 x0, S32 y0, S32 x1, S32 y1, S32 col, S32 z1, S32 z2)
{
	/*
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
	*/
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}

void Line_Entry(S32 x0, S32 y0, S32 x1, S32 y1, S32 col)
{
  S32 color = col;
  col &= 0xFF;
  if (Fill_Flag_Fog)
  {
    color = Fill_Logical_Palette[col];
  }
  ColorX = color;
  ColorY = color;
  Color = color;

  while (true)
  {
    // @@Trace_Still:
    S32 diffX = x1 - x0;
    if (diffX == 0)
    {
      // *** DeltaX nul ***
      // @@DX_Nul:
      if (x0 < ClipXMin || x0 > ClipXMax)
      {
        return;
      }

      S32 diffY = y1 - y0;
      if (diffY == 0)
      {
        // *** Plot ***
        // @@Plot:
        if (y0 < ClipYMin || y0 > ClipYMax)
        {
          return;
        }

        U8 *dst = (U8 *)Log + PTR_TabOffLine[y0] + x0;
        *dst = Color;
        return;
      }

      if (y0 < ClipYMin)
      {
        if (y1 < ClipYMin)
        {
          return;
        }

        y0 = ClipYMin;
      }

      // @@DX_Y1:
      if (y0 > ClipYMax)
      {
        if (y1 > ClipYMax)
        {
          return;
        }

        y0 = ClipYMax;
      }

      // @@DX_Y1b:
      if (y1 < ClipYMin)
      {
        y1 = ClipYMin;
      }

      // @@DX_Y2:
      if (y1 > ClipYMax)
      {
        y1 = ClipYMax;
      }

      // @@DX_Y2b:
      S32 newDiffY = y1 - y0;
      if (newDiffY < 0)
      {
        newDiffY = -newDiffY;
        y0 = y1;
      }

      // @@DX_Ok:
      U8 *dst = (U8 *)Log + PTR_TabOffLine[y0] + x1;
      newDiffY++;
      do
      {
        *dst = Color;
        dst += ScreenPitch;
        newDiffY--;
      } while (newDiffY > 0);

      return;
    }

    S32 diffY = y1 - y0;
    if (diffY == 0)
    {
      // *** DeltaY nul ***
      // @@DY_Nul:
      if (y0 < ClipYMin || y0 > ClipYMax)
      {
        return;
      }

      if (x0 < ClipXMin)
      {
        if (x1 < ClipXMin)
        {
          return;
        }

        x0 = ClipXMin;
      }

      // @@DY_X1:
      if (x0 > ClipXMax)
      {
        if (x1 > ClipXMax)
        {
          return;
        }

        x0 = ClipXMax;
      }

      // @@DY_X1b:
      if (x1 < ClipXMin)
      {
        x1 = ClipXMin;
      }

      // @@DY_X2:
      if (x1 > ClipXMax)
      {
        x1 = ClipXMax;
      }

      // @@DY_X2b:
      S32 newDiffX = x1 - x0;
      if (newDiffX < 0)
      {
        newDiffX = -newDiffX;
        x0 = x1;
      }

      // @@DY_Ok:
      U8 *dst = (U8 *)Log + PTR_TabOffLine[y0] + x0;
      newDiffX++;
      do
      {
        *dst = Color;
        dst++;
        newDiffX--;
      } while (newDiffX > 0);
    }

    if (x0 < ClipXMin)
    {
      if (x1 < ClipXMin)
      {
        return;
      }

      y0 = y0 - (x0 - ClipXMin) * diffY / diffX;
      x0 = ClipXMin;
    }
    // @@Clip_X0:
    else if (x0 > ClipXMax)
    {
      if (x1 > ClipXMax)
      {
        return;
      }

      y0 = y0 - (x0 - ClipXMax) * diffY / diffX;
      x0 = ClipXMax;
    }
    // @@Clip_X1:
    else if (x1 < ClipXMin)
    {
      y1 = y1 - (x1 - ClipXMin) * diffY / diffX;
      x1 = ClipXMin;
    }
    // @@Clip_X2:
    else if (x1 > ClipXMax)
    {
      y1 = y1 - (x1 - ClipXMax) * diffY / diffX;
      x1 = ClipXMax;
    }
    // @@Clip_X3:
    else if (y0 < ClipYMin)
    {
      if (y1 < ClipYMin)
      {
        return;
      }
      x0 = (ClipYMin - y0) * diffX / diffY + x0;
      y0 = ClipYMin;
    }
    // @@Clip_Y0:
    else if (y0 > ClipYMax)
    {
      if (y1 > ClipYMax)
      {
        return;
      }

      x0 = (ClipYMax - y0) * diffX / diffY + x0;
      y0 = ClipYMax;
    }
    // @@Clip_Y1:
    else if (y1 < ClipYMin)
    {
      x1 = x1 + (ClipYMin - y1) * diffX / diffY;
      y1 = ClipYMin;
    }
    // @@Clip_Y2:
    else if (y1 > ClipYMax)
    {
      x1 = x1 + (ClipYMax - y1) * diffX / diffY;
      y1 = ClipYMax;
    }
    // @@Clip_Y3:
    else
    {
      S32 incrY = ScreenPitch;
      S32 incrX = 1;
      S32 deltaX = x1 - x0;
      if (deltaX < 0)
      {
        deltaX = -deltaX;
        incrX = -incrX;
      }

      S32 deltaY = y1 - y0;
      if (deltaY < 0)
      {
        deltaY = -deltaY;
        incrY = -incrY;
      }

      U8 *dst = (U8 *)Log + PTR_TabOffLine[y0] + x0;

      if (deltaX > deltaY)
      {
        S32 halfX = deltaX / 2;
        S32 x = deltaX;

        do
        {
          // @@Do_X:
          *dst = ColorX;

          halfX += deltaY;
          if (halfX >= deltaX)
          {
            halfX -= deltaX;
            dst += incrY;
          }

          dst += incrX;
          x--;
        } while (x >= 0);

        return;
      }
      else
      {
        // @@Draw_Y:
        S32 halfY = deltaY / 2;
        S32 y = deltaY;

        do
        {
          // @@Do_Y:
          *dst = ColorY;

          halfY += deltaX;
          if (halfY >= deltaY)
          {
            halfY -= deltaY;
            dst += incrX;
          }

          dst += incrY;
          y--;
        } while (y >= 0);

        return;
      }
    }
  }
}

void Line(S32 x0, S32 y0, S32 x1, S32 y1, S32 col)
{
	/*
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
	*/

	Line_Entry(x0, y0, x1, y1, col);
}

void Line_A(S32 x0, S32 y0, S32 x1, S32 y1, S32 col, S32 z1, S32 z2)
{
	/*
; ╔════════════════════════════════════════════════════════════════════════╗
; ║   Trace de droite                                                      ║
; ╟────────────────────────────────────────────────────────────────────────╢
; ║ Appel : (EAX,EBX), (ECX,EDX) : points extremes                         ║
; ║         EBP = Color                                                    ║
; ║         EDI = Z1                                                       ║
; ║         ESI = Z2                                                       ║
; ║                                                                        ║
; ╚════════════════════════════════════════════════════════════════════════╝
			ALIGN	4
Line_A			PROC
			cmp	[Fill_Flag_NZW],TRUE
			je	Line_ZBuffer_NZW

			cmp	[Fill_Flag_ZBuffer],TRUE
			je	Line_ZBuffer
	*/

	if (Fill_Flag_NZW)
	{
		Line_ZBuffer_NZW(x0, y0, x1, y1, col, z1, z2);
		return;
	}

	if (Fill_Flag_ZBuffer)
	{
		Line_ZBuffer(x0, y0, x1, y1, col, z1, z2);
		return;
	}

	Line_Entry(x0, y0, x1, y1, col);
}