/*

        OPTION SCOPED
		.386p

		.MODEL	FLAT, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.DATA

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	TransFctBody	:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	Lib3DBufferAnim
		PUBLIC	C	EndLib3DBufferAnim
		PUBLIC	C	PtrLib3DBufferAnim

		PUBLIC	C	TransFctAnim

		ASSUME  DS:SEG Lib3DBufferAnim

;*──────────────────────────────────────────────────────────────────────────*
;	Buffer
MallocBuffer		dd	00
Lib3DBufferAnim		dd	00
EndLib3DBufferAnim	dd	00
PtrLib3DBufferAnim	dd	00

;	Translation Function
TransFctAnim		dd	00

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
*/

#include <ANIM/LIBINIT.H>
#include <OBJECT/AFF_OBJ.H>
#include <stdlib.h>
#include <stdio.h>

Func_TransNumPtr *TransFctAnim = NULL;
U8 *Lib3DBufferAnim = NULL;
U8 *EndLib3DBufferAnim = NULL;
U8 *PtrLib3DBufferAnim = NULL;
U8 *MallocBuffer = NULL;

void ClearObjects()
{
	/*
	ClearObjects	proc

			mov	eax, MallocBuffer
			test	eax, eax
			jz	exit

	IFDEF	REG_CALL
			call	free_
	ELSE
			push	eax
			call	free
			pop	eax
	ENDIF
			xor	eax, eax
			mov	MallocBuffer, eax
			mov	Lib3DBufferAnim, eax
	exit:
			ret

	ClearObjects	endp
	*/

	if (MallocBuffer != NULL)
	{
		free(MallocBuffer);
		MallocBuffer = NULL;
		Lib3DBufferAnim = NULL;
	}
}

void InitObjects(void *buffer, U32 size, Func_TransNumPtr *fctbody, Func_TransNumPtr *fctanim)
{
	/*
;void	InitObjects(void *buffer, U32 size, TRANS_FUNC *fctbody, TRANS_FUNC *fctanim)

;#pragma aux InitObjects	       		\
;	parm		[eax] [ecx] [edx] [ebx]	\
;	modify exact 	[eax ecx edx]

InitObjects	PROC \
			uses eax ebx ecx edx edi esi ebp\
			buffer: DWORD, \
			size_: DWORD, \
			fctbody: DWORD, \
			fctanim: DWORD
			mov eax, buffer
			mov ecx, size_
			mov edx, fctbody
			mov ebx, fctanim

		mov	TransFctBody, edx
		mov	TransFctAnim, ebx

		test	eax, eax
		jz	alloc
returnalloc:
		mov	Lib3DBufferAnim, eax
		lea	ecx, [eax+ecx-31*(SIZEOF GROUP_INFO)+1]
		mov	PtrLib3DBufferAnim, eax
		mov	EndLib3DBufferAnim, ecx
exit:
		ret
alloc:
		mov	eax, [Lib3DBufferAnim]

		test	eax, eax
		jnz	exit
IFDEF	REG_CALL
		mov	eax, ecx
		call	malloc_
ELSE
		push	ecx
		call	malloc
		pop	ecx
ENDIF
		test	eax, eax
		jz	error

		mov	Lib3DBufferAnim, eax
		lea	ecx, [eax+ecx-31*(SIZEOF GROUP_INFO)+1]
		mov	PtrLib3DBufferAnim, eax
		mov	EndLib3DBufferAnim, ecx

		push	eax
IFDEF	REG_CALL
		mov	eax, offset ClearObjects; register ClearObjects
		call	atexit_		       	; for exit
ELSE
		push	offset ClearObjects	; register ClearObjects
		call	atexit		       	; for exit
		pop	edx
ENDIF
		pop	eax

		mov	MallocBuffer, eax

error:
		ret

InitObjects	endp
	*/
	TransFctBody = fctbody;
	TransFctAnim = fctanim;

	if (buffer == NULL)
	{
		if (Lib3DBufferAnim == NULL)
		{
			Lib3DBufferAnim = (U8 *)malloc(size);
			if (Lib3DBufferAnim == NULL)
			{
				return;
			}

			PtrLib3DBufferAnim = Lib3DBufferAnim;
			EndLib3DBufferAnim = Lib3DBufferAnim + size - 31 * sizeof(T_GROUP_INFO) + 1;

			atexit(ClearObjects);

			MallocBuffer = Lib3DBufferAnim;
		}
	}
	else
	{
		Lib3DBufferAnim = (U8 *)buffer;
		PtrLib3DBufferAnim = Lib3DBufferAnim;
		EndLib3DBufferAnim = Lib3DBufferAnim + size - 31 * sizeof(T_GROUP_INFO) + 1;
	}
}
