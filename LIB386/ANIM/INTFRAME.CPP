#include <ANIM/INTFRAME.H>
#include <ANIM.H>
#include <stddef.h>

void	ObjectSetInterFrame(T_OBJ_3D *obj)
{
	/*
;void	ObjectSetInterFrame(T_OBJ *obj)

;#pragma aux ObjectSetInterFrame	\
;	parm		[ebx]		\
;	modify exact 	[eax ecx edx esi edi]

ObjectSetInterFrame PROC \
			uses eax ebx ecx edx edi esi ebp\
			obj: DWORD
			mov ebx, obj

		mov	eax, [ebx].OBJ_3D.Status

		test	eax, STATUS_FRAME	; something to do ?
		jnz	exit

		test	eax, STATUS_CHANGE	; something to do ?
		jz	exit

		xor	eax, eax
		mov	esi, [ebx].OBJ_3D.Anim

		mov	[ebx].OBJ_3D.Status, eax	; ok nothing to do next time
		mov	eax, [TransFctAnim]

		test	eax, eax
		jz	skipTrans

		push	esi
		call	eax
		mov		esi, eax
		add		esp,4
skipTrans:
		mov	eax, [ebx].OBJ_3D.Status
		mov	edi, [ebx].OBJ_3D.LastOfsFrame

		cmp	[ebx].OBJ_3D.LastOfsIsPtr, 1
		je	skipofs
		add	edi, esi
skipofs:
		add	esi, [ebx].OBJ_3D.NextOfsFrame

		mov	ecx, [ebx].OBJ_3D.NbGroups
		add	edi, 16			; -> group 1
		add	esi, 16			; -> group 1
		dec	ecx
		jz	exit			; only one group? exit

		lea	ebp, [ebx].OBJ_3D.CurrentFrame
		mov	ebx, [ebx].OBJ_3D.Interpolator
nextgrouprot:
		mov	ax, [esi]
		mov	[ebp], ax
		test	ax, ax
		jnz	trans
rot:
		mov	ax, [esi+2]		; next
		mov	dx, [edi+2]		; last

		sub     eax, edx
		and	eax, 4095
		jz      optequal0

		shl	eax, 20
		sar	eax, 20

		imul    eax, ebx
		sar	eax, 16
		add     edx, eax
		and	edx, 4095
optequal0:
		mov	ax, [esi+4]		; next
		mov	[ebp+2], dx		; Alpha
		mov	dx, [edi+4]		; last

		sub     eax, edx
		and	eax, 4095
		jz      optequal1

		shl	eax, 20
		sar	eax, 20

		imul    eax, ebx
		sar	eax, 16
		add     edx, eax
		and	edx, 4095
optequal1:
		mov	ax, [esi+6]		; next
		mov	[ebp+4], dx		; Beta
		mov	dx, [edi+6]		; last

		sub     eax, edx
		and	eax, 4095
		jz      optequal2

		shl	eax, 20
		sar	eax, 20

		imul    eax, ebx
		sar	eax, 16
		add     edx, eax
		and	edx, 4095
optequal2:
		add	edi, 8
		mov	[ebp+6], dx		; Gamma
		add	esi, 8
		add	ebp, 8

		dec	ecx
		jnz	nextgrouprot

		ret
nextgrouptrans:
		mov	ax, [esi]
		mov	[ebp], ax
		test	ax, ax
		jz	rot
trans:
		movsx	eax, word ptr[esi+2]	; next
		movsx	edx, word ptr[edi+2]	; last

		sub     eax, edx
		jz      optequal3

		imul    eax, ebx
		sar	eax, 16
		add     edx, eax
optequal3:
		movsx	eax, word ptr[esi+4]	; next
		mov	[ebp+2], dx		; X
		movsx	edx, word ptr[edi+4]	; last

		sub     eax, edx
		jz      optequal4

		imul    eax, ebx
		sar	eax, 16
		add     edx, eax
optequal4:
		movsx	eax, word ptr[esi+6]	; next
		mov	[ebp+4], dx		; Y
		movsx	edx, word ptr[edi+6]	; last

		sub     eax, edx
		jz      optequal5

		imul    eax, ebx
		sar	eax, 16
		add     edx, eax
optequal5:
		add	edi, 8
		mov	[ebp+6], dx		; Z
		add	esi, 8
		add	ebp, 8

		dec	ecx
		jnz	nextgrouptrans
exit:
		ret

ObjectSetInterFrame endp
	*/
	if (obj->Status == FLAG_FRAME)
		return;

	if (obj->Status != FLAG_CHANGE)
		return;

	obj->Status = 0;

	void *anim = obj->Anim.Ptr;
	if (TransFctAnim != NULL)
	{
		anim = TransFctAnim(obj->Anim.Num);
	}

	U16 *nextPtr = (U16 *)((U64)anim + (U64)obj->NextOfsFrame + 16);
	U16 *lastPtr = (U16 *)((U64)anim + 16);

	if (obj->LastOfsIsPtr != 1)
	{
		lastPtr = (U16 *)((U64)lastPtr + (U64)obj->LastOfsFrame);
	}

	// only one group? exit
	if (obj->NbGroups == 1)
		return;

	T_GROUP_INFO *group = &obj->CurrentFrame[0];

	for (U32 i = 0; i < obj->NbGroups; i++)
	{
		U16 type = nextPtr[0];
		group->Type = type;

		// Rotations
		if (type == TYPE_ROTATE)
		{
			U16 next = nextPtr[1];
			U16 last = lastPtr[1];
			U32 diffAlpha = (next - last) & 0xFFF; // & 4095
			if (diffAlpha != 0)
			{
				diffAlpha = (diffAlpha << 20) >> 20;
				diffAlpha = (diffAlpha * obj->Interpolator) >> 16;
				last = (last + diffAlpha) & 0xFFF;
			}

			next = nextPtr[2];
			group->Alpha = last;
			last = lastPtr[2];

			U32 diffBeta = (next - last) & 0xFFF;
			if (diffBeta != 0)
			{
				diffBeta = (diffBeta << 20) >> 20;
				diffBeta = (diffBeta * obj->Interpolator) >> 16;
				last = (last + diffBeta) & 0xFFF;
			}

			next = nextPtr[3];
			group->Beta = last;
			last = lastPtr[3];

			U32 diffGamma = (next - last) & 0xFFF;
			if (diffGamma != 0)
			{
				diffGamma = (diffGamma << 20) >> 20;
				diffGamma = (diffGamma * obj->Interpolator) >> 16;
				last = (last + diffGamma) & 0xFFF;
			}

			next = nextPtr[4];
			group->Gamma = last;
			last = lastPtr[4];

			nextPtr = (U16 *)((U64)nextPtr + 8);
			lastPtr = (U16 *)((U64)lastPtr + 8);
			group++;
		}
		else // Translations
		{
			U16 next = nextPtr[1];
			U16 last = lastPtr[1];
			U32 diffX = (next - last);
			if (diffX != 0)
			{
				diffX = (diffX * obj->Interpolator) >> 16;
				last = last + diffX;
			}

			next = nextPtr[2];
			group->Alpha = last; // X
			last = lastPtr[2];

			U32 diffY = (next - last);
			if (diffY != 0)
			{
				diffY = (diffY * obj->Interpolator) >> 16;
				last = last + diffY;
			}

			next = nextPtr[3];
			group->Beta = last; // Y
			last = lastPtr[3];

			U32 diffZ = (next - last);
			if (diffZ != 0)
			{
				diffZ = (diffZ * obj->Interpolator) >> 16;
				last = last + diffZ;
			}

			next = nextPtr[4];
			group->Gamma = last; // Z
			last = lastPtr[4];

			nextPtr = (U16 *)((U64)nextPtr + 8);
			lastPtr = (U16 *)((U64)lastPtr + 8);
			group++;
		}
	}
}
