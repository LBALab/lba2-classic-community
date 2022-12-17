;----------------------------------------------------------------------------
			.386P
			.MODEL SMALL, C

;----------------------------------------------------------------------------
			.DATA

;----------------------------------------------------------------------------
			EXTRN	C	ClipXMin	:DWORD
			EXTRN	C	ClipYMin	:DWORD
			EXTRN	C	ClipXMax	:DWORD
			EXTRN	C	ClipYMax	:DWORD
			EXTRN	C	Log		:DWORD
			EXTRN	C	TabOffLine	:DWORD

			ASSUME DS:SEG Log

;----------------------------------------------------------------------------
			.CODE

;----------------------------------------------------------------------------
			PUBLIC	C Box

;----------------------------------------------------------------------------
;void	Box(S32 x0, S32 y0, S32 x1, S32 y1, S32 col)

;#pragma aux Box	"*"			\
;	parm	[eax] [ebx] [ecx] [edx] [esi]	\
;	modify	[edi]

Box			PROC \
			uses eax ebx ecx edx edi esi ebp\
			x0: DWORD, \
			y0: DWORD, \
			x1: DWORD, \
			y1: DWORD, \
			col: DWORD
			mov eax, x0
			mov ebx, y0
			mov ecx, x1
			mov edx, y1
			mov esi, col

;----------------------
			cmp	eax, ClipXMax
			jg	Box_End
			cmp     ecx, ClipXMin
			jl	Box_End
			cmp	ebx, ClipYMax
			jg	Box_End
			cmp	edx, ClipYMin
			jl	Box_End
TestLeft:
			cmp	eax, ClipXMin
			jge	TestRight
			mov	eax, ClipXMin
TestRight:
			cmp	ecx, ClipXMax
			jle	TestUp
			mov	ecx, ClipXMax
TestUp:
			cmp	ebx, ClipYMin
			jge	TestDown
			mov	ebx, ClipYMin
TestDown:
			cmp	edx, ClipYMax
			jle	OkNoClip
			mov	edx, ClipYMax
OkNoClip:
;----------------------
			mov	edi, TabOffLine[ebx*4]
			sub	edx, ebx		; Init Delta Y

			add	edi, Log
			sub	ecx, eax		; Init Delta X

			add	edi, eax		; EDI Offset Src
			mov	eax, esi

			shl	esi, 8
			inc	edx

			or	eax, esi
			inc	ecx

			mov	esi, eax
			mov	ebp, TabOffLine[1*4]

			shl	esi, 16
			sub	ebp, ecx		; ebp Delta Screen

			or	eax, esi
			mov	esi, ecx

			mov	ebx, ecx

			shr	esi, 2
			and	ebx, 11b
Again:
			mov	ecx, esi
			rep	stosd
			mov	ecx, ebx
			rep	stosb

			add	edi, ebp
			dec	edx
			jne	Again
Box_End:
			ret
Box			endp

;----------------------------------------------------------------------------
;			The
       			End

