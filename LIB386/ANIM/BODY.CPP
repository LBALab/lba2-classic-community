#include <ANIM/BODY.H>
#include <ANIM.H>

void	ObjectInitBody(T_OBJ_3D *obj, void *body)
{
	/*
;void	ObjectInitBody(T_OBJ *obj, T_BODY *body)

;#pragma aux ObjectInitBody		\
;	parm		[ebx] [eax]	\
;	modify exact 	[ecx]

ObjectInitBody		PROC \
			uses eax ebx ecx edx edi esi ebp\
			obj: DWORD, \
			body: DWORD
			mov ebx, obj
			mov eax, body

		mov	ecx, [ebx].OBJ_3D.Status
		mov	[ebx].OBJ_3D.NextBody, eax
		or	ecx, STATUS_BODY
		cmp	[ebx].OBJ_3D.Body, -1
		mov	[ebx].OBJ_3D.Status, ecx
		je	firstBody
		ret
firstBody:
		mov	[ebx].OBJ_3D.Body, eax
		ret

ObjectInitBody	endp
	*/
	obj->Status |= FLAG_BODY;

	// TODO: this mix of Ptr and Num is confusing and might not work on 64bit systems
	obj->NextBody.Ptr = body;
	if (obj->Body.Num == -1)
	{
		obj->Body.Ptr = body;
	}
}
