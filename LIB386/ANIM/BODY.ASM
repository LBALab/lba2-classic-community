
		.386p

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	ObjectInitBody

;*══════════════════════════════════════════════════════════════════════════*
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

;*══════════════════════════════════════════════════════════════════════════*
; 		The
		END
