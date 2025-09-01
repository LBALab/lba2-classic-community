
		.386p

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	ObjectClear

;*══════════════════════════════════════════════════════════════════════════*
;void	ObjectClear(T_OBJ *obj)

;#pragma aux ObjectClear		\
;	parm		[ebx]		\
;	modify exact 	[eax ecx edi]

ObjectClear	PROC \
			uses eax ebx ecx edx edi esi ebp\
			obj: DWORD
			mov ebx, obj

		mov	edi, ebx
		mov	ecx, SIZEOF OBJ_3D / 4	; clear structure
		xor	eax, eax
		rep	stosd

		mov	[ebx].OBJ_3D.Body, -1
		mov	[ebx].OBJ_3D.NextBody, -1
		mov	[ebx].OBJ_3D.Texture, -1
		mov	[ebx].OBJ_3D.NextTexture, -1
		mov	[ebx].OBJ_3D.Anim, -1

		ret

ObjectClear	endp

;*══════════════════════════════════════════════════════════════════════════*
; 		The
		END
