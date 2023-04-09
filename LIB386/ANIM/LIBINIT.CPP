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
	if (MallocBuffer != NULL)
	{
		free(MallocBuffer);
		MallocBuffer = NULL;
		Lib3DBufferAnim = NULL;
	}
}

void InitObjects(void *buffer, U32 size, Func_TransNumPtr *fctbody, Func_TransNumPtr *fctanim)
{
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
