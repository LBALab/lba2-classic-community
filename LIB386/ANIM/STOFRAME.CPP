#include <ANIM/STOFRAME.H>

#include <SYSTEM/TIMER.H>

void	ObjectStoreFrame(T_OBJ_3D *Obj)
{
	/*
;void	ObjectStoreFrame(T_OBJ *Obj)

;#pragma aux ObjectStoreFrame		\
;	parm		[ebx]		\
;	modify exact	[eax ecx esi edi]

ObjectStoreFrame proc

		mov	edi, PtrLib3DBufferAnim	; edi -> looping anim buffer

		mov	eax, TimerRefHR
		mov	[ebx].OBJ_3D.LastTimer, eax	; struct -> LastTimer
		mov	[ebx].OBJ_3D.ObjTime, eax	; struct -> ObjTime
		xor	eax, eax
		mov	ecx, [ebx].OBJ_3D.NbGroups
		inc	eax
		mov	[ebx].OBJ_3D.LastNbGroups, ecx	; struct -> LastNbGroups
		lea	ecx, [ecx*2-2]		; 2 DWORDs per group, no group 0
		mov	[ebx].OBJ_3D.LastOfsFrame, edi	; copy will be last frame
		mov	[ebx].OBJ_3D.LastOfsIsPtr, eax
		add	edi, 16
		dec	eax
		lea	esi, [ebx].OBJ_3D.CurrentFrame	; esi -> CurrentFrame

		mov	[ebx].OBJ_3D.Status, eax
		mov	dword ptr[edi-16], eax
		mov	dword ptr[edi-12], eax
		mov	dword ptr[edi-8], eax
		mov	dword ptr[edi-4], eax
		dec	eax
		; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
				;   is it confusing it with some other instruction? Is it not supported by 486?
		rep	movsd			; do copy...

		cmp	edi, EndLib3DBufferAnim	; check buffer for next time
		jle	okbuf

		mov	edi, Lib3DBufferAnim
okbuf:
		mov	PtrLib3DBufferAnim, edi	; write back buffer pointer
		mov	[ebx].OBJ_3D.LastFrame, eax	; LastFrame = -1

		ret

ObjectStoreFrame endp
	*/
	Obj->LastTimer = TimerRefHR;
	Obj->Time = TimerRefHR;

	Obj->LastNbGroups = Obj->NbGroups;
	Obj->LastOfsFrame = (PTR_U32)PtrLib3DBufferAnim;
	Obj->LastOfsIsPtr = 1;

	Obj->Status = 0;
	U32 *esi = (U32 *)(Obj->CurrentFrame);
	U32 *edi = (U32 *)(PtrLib3DBufferAnim);
	edi[0] = edi[1] = edi[2] = edi[3] = 0; // initialize memory at edi to 0
	edi += 4;

	for (U32 i = 0; i < Obj->NbGroups * 2; i++)
	{
		edi[i] = esi[i];
	}
	edi += Obj->NbGroups * 2;
	esi += Obj->NbGroups * 2;

	if (edi > (PTR_U32)EndLib3DBufferAnim)
	{
		edi = (PTR_U32)Lib3DBufferAnim;
	}

	PtrLib3DBufferAnim = (PTR_U8)edi;
	Obj->LastFrame = -1;
}
