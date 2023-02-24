#include <SVGA/CLIP.H>
#include <SVGA/VIDEO.H>
#include <stdio.h>

S32 ClipXMin = 0; // ClipXMin		dd	0
S32 ClipYMin = 0; // ClipYMin		dd	0
S32 ClipXMax = 0; // ClipXMax		dd	0
S32 ClipYMax = 0; // ClipYMax		dd	0

S32 ClipWindowXMin = 0; // ClipWindowXMin		dd	0
S32 ClipWindowYMin = 0; // ClipWindowYMin		dd	0
S32 ClipWindowXMax = 0; // ClipWindowXMax		dd	0
S32 ClipWindowYMax = 0; // ClipWindowYMax		dd	0

S32 MemoClipXMin = 0; // MemoClipXMin		dd	0
S32 MemoClipYMin = 0; // MemoClipYMin		dd	0
S32 MemoClipXMax = 0; // MemoClipXMax		dd	0
S32 MemoClipYMax = 0; // MemoClipYMax		dd	0

S32 MemoClipWindowXMin = 0; // MemoClipWindowXMin	dd	0
S32 MemoClipWindowYMin = 0; // MemoClipWindowYMin	dd	0
S32 MemoClipWindowXMax = 0; // MemoClipWindowXMax	dd	0
S32 MemoClipWindowYMax = 0; // MemoClipWindowYMax	dd	0

void SetClipWindow(S32 x0, S32 y0, S32 x1, S32 y1)
{
 /*
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
 */
		if (x0 >= 0)
        ClipWindowXMin = x0;
    else
        ClipWindowXMin = 0;

    if (y0 >= 0)
        ClipWindowYMin = y0;
    else
        ClipWindowYMin = 0;

    if (x1 < ModeResX)
        ClipWindowXMax = x1;
    else
        ClipWindowXMax = ModeResX - 1;

    if (y1 < ModeResY)
        ClipWindowYMax = y1;
    else
        ClipWindowYMax = ModeResY - 1;

    SetClip(ClipXMin, ClipYMin, ClipXMax, ClipYMax);
}

void UnsetClipWindow()
{
/*
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
*/
	ClipWindowXMax = ModeResX - 1;
	ClipWindowYMax = ModeResY - 1;
	ClipWindowXMin = 0;
	ClipWindowYMin = 0;
	ClipXMax = ModeResX - 1;
	ClipYMax = ModeResY - 1;
	ClipXMin = 0;
	ClipYMin = 0;
}

void MemoClipWindow()
{
	/*
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
	*/
	MemoClipWindowXMin = ClipWindowXMin;
	MemoClipWindowYMin = ClipWindowYMin;
	MemoClipWindowXMax = ClipWindowXMax;
	MemoClipWindowYMax = ClipWindowYMax;
}

void RestoreClipWindow()
{
	/*
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
	*/
	SetClipWindow(MemoClipWindowXMin, MemoClipWindowYMin, MemoClipWindowXMax, MemoClipWindowYMax);
}

void SetClip(S32 x0, S32 y0, S32 x1, S32 y1)
{
	/*
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
	*/
	if (x0 >= ClipWindowXMin)
			ClipXMin = x0;
	else
			ClipXMin = ClipWindowXMin;

	if (y0 >= ClipWindowYMin)
			ClipYMin = y0;
	else
			ClipYMin = ClipWindowYMin;

	if (x1 <= ClipWindowXMax)
			ClipXMax = x1;
	else
			ClipXMax = ClipWindowXMax;

	if (y1 <= ClipWindowYMax)
			ClipYMax = y1;
	else
			ClipYMax = ClipWindowYMax;
}

void UnsetClip()
{
 /*
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
 */
	ClipXMin = ClipWindowXMin;
	ClipYMin = ClipWindowYMin;
	ClipXMax = ClipWindowXMax;
	ClipYMax = ClipWindowYMax;
}

void MemoClip()
{
	/*
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
	*/
	MemoClipXMin = ClipXMin;
	MemoClipYMin = ClipYMin;
	MemoClipXMax = ClipXMax;
	MemoClipYMax = ClipYMax;
}

void RestoreClip()
{
	/*
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
	*/
	SetClip(MemoClipXMin, MemoClipYMin, MemoClipXMax, MemoClipYMax);
}

