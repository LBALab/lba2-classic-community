
		.386p

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.DATA

		EXTRN	C	printf	:NEAR

fmt_clear_1	DB	"[ASM][ObjectClear][1] obj=%p", 10, 0

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

		; --- debug trace [ASM][ObjectClear][1] params ---
		pushad
		push	ebx
		push	offset fmt_clear_1
		call	printf
		add	esp, 8
		popad
		; --- end debug trace ---

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
