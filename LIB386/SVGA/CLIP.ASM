;----------------------------------------------------------------------------
            OPTION SCOPED
			.386p
			.model FLAT, C

;----------------------------------------------------------------------------
			.DATA

;----------------------------------------------------------------------------
			EXTRN	C	ModeResX:DWORD
			EXTRN	C	ModeResY:DWORD

;----------------------------------------------------------------------------
			PUBLIC	C ClipXMin
			PUBLIC	C ClipYMin
			PUBLIC	C ClipXMax
			PUBLIC	C ClipYMax

;----------------------------------------------------------------------------
			PUBLIC	C ClipWindowXMin
			PUBLIC	C ClipWindowYMin
			PUBLIC	C ClipWindowXMax
			PUBLIC	C ClipWindowYMax

;----------------------------------------------------------------------------
			PUBLIC	C MemoClipXMin
			PUBLIC	C MemoClipYMin
			PUBLIC	C MemoClipXMax
			PUBLIC	C MemoClipYMax

;----------------------------------------------------------------------------
			PUBLIC	C MemoClipWindowXMin
			PUBLIC	C MemoClipWindowYMin
			PUBLIC	C MemoClipWindowXMax
			PUBLIC	C MemoClipWindowYMax

;----------------------------------------------------------------------------
ClipXMin		dd	0
ClipYMin		dd	0
ClipXMax		dd	0
ClipYMax		dd	0

;----------------------------------------------------------------------------
ClipWindowXMin		dd	0
ClipWindowYMin		dd	0
ClipWindowXMax		dd	0
ClipWindowYMax		dd	0

;----------------------------------------------------------------------------
MemoClipXMin		dd	0
MemoClipYMin		dd	0
MemoClipXMax		dd	0
MemoClipYMax		dd	0

;----------------------------------------------------------------------------
MemoClipWindowXMin	dd	0
MemoClipWindowYMin	dd	0
MemoClipWindowXMax	dd	0
MemoClipWindowYMax	dd	0

;----------------------------------------------------------------------------
				.CODE

;----------------------------------------------------------------------------
			PUBLIC	C	SetClipWindow
			PUBLIC	C	UnsetClipWindow
			PUBLIC	C	SetClip
			PUBLIC	C	UnsetClip
			PUBLIC	C	MemoClip
			PUBLIC	C	RestoreClip

;----------------------------------------------------------------------------
;void	SetClipWindow(S32 x0, S32 y0, S32 x1, S32 y1)	;

;#pragma aux SetClipWindow	"*"	\
;	parm	[eax] [ebx] [ecx] [edx]

SetClipWindow		PROC \
			uses eax ebx ecx edx edi esi ebp\
			x0: DWORD, y0: DWORD, x1: DWORD, y1: DWORD
			mov eax, x0
			mov ebx, y0
			mov ecx, x1
			mov edx, y1

			test	eax, eax
			jae	OkLeft
			xor	eax, eax
OkLeft:			mov	ClipWindowXMin, eax

			test	ebx, ebx
			jae	OkUp
			xor	ebx, ebx
OkUp:			mov	ClipWindowYMin, ebx

			cmp	ecx, ModeResX
			jb	OkRight
			mov	ecx, ModeResX
			dec	ecx
OkRight:		mov	ClipWindowXMax, ecx

			cmp	edx, ModeResY
			jb	OkDown
			mov	edx, ModeResY
			dec	edx
OkDown:			mov	ClipWindowYMax, edx

			mov     eax, ClipXMin
			mov	ebx, ClipYMin
			mov     ecx, ClipXMax
			mov	edx, ClipYMax

			push edx
			push ecx
			push ebx
			push eax
			call SetClip
			add esp, 4*4
			ret

SetClipWindow		endp

;----------------------------------------------------------------------------
;void	UnsetClipWindow()				;

;#pragma aux UnsetClipWindow	"*"	\
;	modify	[eax ebx]

UnsetClipWindow		PROC \
			uses eax ebx ecx edx edi esi ebp\
			

			mov     eax, ModeResX
			mov	ebx, ModeResY
			dec	eax
			dec	ebx
			mov	ClipWindowXMax, eax
			mov	ClipXMax, eax
			xor	eax, eax
			mov	ClipWindowYMax, ebx
			mov	ClipYMax, ebx
			mov	ClipWindowXMin, eax
			mov	ClipXMin, eax
			mov	ClipWindowYMin, eax
			mov	ClipYMin, eax

			ret

UnsetClipWindow		endp

;----------------------------------------------------------------------------
;void	MemoClipWindow()					;

;#pragma aux MemoClipWindow		"*"	\
;	modify	[eax ebx]

MemoClipWindow		PROC \
			uses eax ebx ecx edx edi esi ebp\
			

			mov	eax,ClipWindowXMin
			mov	ebx,ClipWindowYMin
			mov	MemoClipWindowXMin, eax
			mov	MemoClipWindowYMin, ebx
			mov	eax,ClipWindowXMax
			mov	ebx,ClipWindowYMax
			mov	MemoClipWindowXMax, eax
			mov	MemoClipWindowYMax, ebx

			ret

MemoClipWindow		endp

;----------------------------------------------------------------------------
;void	RestoreClipWindow()					;

;#pragma aux RestoreClipWindow	"*"	\
;	modify	[eax ebx ecx edx]

RestoreClipWindow	PROC \
			uses eax ebx ecx edx edi esi ebp\
			

			mov	eax,MemoClipWindowXMin
			mov	ebx,MemoClipWindowYMin
			mov	ecx,MemoClipWindowXMax
			mov	edx,MemoClipWindowYMax

			push edx
			push ecx
			push ebx
			push eax
			call SetClipWindow
			add esp, 4*4

			ret

RestoreClipWindow	endp

;----------------------------------------------------------------------------
;void	SetClip(S32 x0, S32 y0, S32 x1, S32 y1)		;

;#pragma aux SetClip		"*"	\
;	parm	[eax] [ebx] [ecx] [edx]

SetClip			PROC \
			uses eax ebx ecx edx edi esi ebp\
			x0: DWORD, y0: DWORD, x1: DWORD, y1: DWORD
			mov eax, x0
			mov ebx, y0
			mov ecx, x1
			mov edx, y1

			cmp	eax, ClipWindowXMin
			jge	OkLeft
			mov	eax, ClipWindowXMin
OkLeft:			mov	ClipXMin, eax

			cmp	ebx, ClipWindowYMin
			jge	OkUp
			mov	ebx, ClipWindowYMin
OkUp:			mov	ClipYMin, ebx

			cmp	ecx, ClipWindowXMax
			jle	OkRight
			mov	ecx, ClipWindowXMax
OkRight:		mov	ClipXMax, ecx

			cmp	edx, ClipWindowYMax
			jle	OkDown
			mov	edx, ClipWindowYMax
OkDown:			mov	ClipYMax, edx

			ret

SetClip			endp

;----------------------------------------------------------------------------
;void	UnsetClip()					;

;#pragma aux UnsetClip		"*"	\
;	modify	[eax ebx]

UnsetClip		PROC \
			uses eax ebx ecx edx edi esi ebp\
			x0: DWORD, y0: DWORD, x1: DWORD, y1: DWORD

			mov     eax, ClipWindowXMin
			mov	ebx, ClipWindowYMin
			mov	ClipXMin, eax
			mov	ClipYMin, ebx
			mov     eax, ClipWindowXMax
			mov	ebx, ClipWindowYMax
			mov	ClipXMax, eax
			mov	ClipYMax, ebx

			ret

UnsetClip		endp

;----------------------------------------------------------------------------
;void	MemoClip()					;

;#pragma aux MemoClip		"*"	\
;	modify	[eax ebx]

MemoClip		PROC \
			uses eax ebx ecx edx edi esi ebp\
			x0: DWORD, y0: DWORD, x1: DWORD, y1: DWORD

			mov	eax,ClipXMin
			mov	ebx,ClipYMin
			mov	MemoClipXMin, eax
			mov	MemoClipYMin, ebx
			mov	eax,ClipXMax
			mov	ebx,ClipYMax
			mov	MemoClipXMax, eax
			mov	MemoClipYMax, ebx

			ret

MemoClip		endp

;----------------------------------------------------------------------------
;void	RestoreClip()					;

;#pragma aux RestoreClip	"*"	\
;	modify	[eax ebx ecx edx]

RestoreClip		PROC \
			uses eax ebx ecx edx edi esi ebp\

			mov	eax,MemoClipXMin
			mov	ebx,MemoClipYMin
			mov	ecx,MemoClipXMax
			mov	edx,MemoClipYMax

			push edx
			push ecx
			push ebx
			push eax
			call SetClip
			add esp, 4*4

			ret

RestoreClip		endp

;----------------------------------------------------------------------------
;				The
				End
