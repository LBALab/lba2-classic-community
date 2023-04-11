#include <SVGA/FONT.H>

#include <unistd.h>

void *PtrFont = NULL;
S32 InterLeave = 1;
S32 InterSpace = 10;

S32 SizeFont(char *str)
{
	/*
;S32	SizeFont(char *str)	;

;#pragma aux SizeFont	"*"	\
;	parm	[esi]		\
;	modify	[eax ecx edx edi]

SizeFont		PROC \
			uses ebx ecx edx edi esi ebp\
			string: DWORD
			mov esi, string

			mov	edi, [PtrFont]
			xor	eax, eax
			xor	edx, edx
;			--------------------------------
NextCar:
			mov	dl,[esi]
			inc	esi

			test	edx, edx
			jz	EndSizeFont

			cmp	edx, 32
			jne	PlusLoin0

			add	eax, [InterSpace]
			jmp	NextCar
PlusLoin0:
			mov	ecx, [edi+edx*4]
			add	eax, [InterLeave]
			mov	dl,  [edi+ecx]
			add	eax, edx
			jmp	NextCar

;			--------------------------------
EndSizeFont:
			ret
SizeFont		endp
	*/
	U32 size = 0;
	while (*str != '\0')
	{
		U8 c = *str;
		if (c == 32)
		{
			size += InterSpace;
		}
		else
		{
			U32 charSize = *(U32 *)((U8 *)PtrFont + c * 4);
			size += InterLeave;
			size += *((U8 *)PtrFont + charSize);
		}
		str++;
	}

	return size;
}

S32 CarFont(S32 x, S32 y, char c)
{
	/*
;S32	CarFont(S32 x, S32 y, char c)	;

;#pragma aux CarFont	"*"		\
;	parm	[ebx] [ecx] [eax]	\
;	modify	[edx esi edi]

CarFont			PROC \
			uses eax ebx ecx edx edi esi ebp\
			x: DWORD, y: DWORD, character:DWORD
			mov ebx, x
			mov ecx, y
			mov eax, character

			cmp	eax, 32			;' SPACE
			je	Space

			mov	edi, [PtrFont]

			mov	esi, edi
			add	edi, [edi+eax*4]

			movzx	edx, byte ptr[edi]

			add	edx, [InterLeave]

			push	edx

			Call	AffMask

			pop	eax

			ret
Space:
			mov	eax, [InterSpace]
			ret

CarFont			endp
	*/
	if (c == 32)
	{
		return InterSpace;
	}

	S32 charSize = *(S32 *)((U8 *)PtrFont + c * 4);
	S32 size = InterLeave + *((U8 *)PtrFont + charSize);
	AffMask(c, x, y, PtrFont);
	return size;
}

S32 Font(S32 x, S32 y, const char *str)
{
	/*
;S32	Font(S32 x, S32 y, char *str);

;#pragma aux Font	"*"		\
;	parm	[ebx] [ecx] [esi]	\
;	modify	[edx esi edi]

Font			PROC \
			uses eax ebx ecx edx edi esi ebp\
			x: DWORD, y: DWORD, string:DWORD
			mov ebx, x
			mov ecx, y
			mov esi, string

			mov	ebp, [PtrFont]
			xor	eax, eax

			test	ebp, ebp
			jz	EndFont

			jmp	NextCar

;			--------------------------------
Space:			add	ebx, [InterSpace]
NextCar:
			mov	al,[esi]
			inc	esi

			test	al, al
			je	short EndFont

			cmp	al, 32
			je	Space
PasSpace:
			mov	edi, [ebp+eax*4]
			push	esi

			mov	esi, [PtrFont]
			push	edi

			push	ecx
			push	ebx

			Call	AffMask

			pop	ebx
			pop	ecx

			pop	edi
			pop	esi

			xor	eax, eax
			mov	edx, [InterLeave]

			mov	al, [ebp+edi]
			add	ebx, edx

			add	ebx, eax
			jmp	short NextCar
;			--------------------------------
EndFont:
			ret

Font			endp
	*/
	if (PtrFont == NULL)
	{
		return 0;
	}

	while (*str != '\0')
	{
		char c = *str;
		if (c == 32)
		{
			x += InterSpace;
		}
		else
		{
			AffMask(c, x, y, PtrFont);
			S32 charSize = *(S32 *)((U8 *)PtrFont + c * 4);
			x += InterLeave;
			x += *((U8 *)PtrFont + charSize);
		}
		str++;
	}

	return 0; // TODO: make this void
}
