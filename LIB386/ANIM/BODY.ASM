
		.386p

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.DATA

		EXTRN	C	printf		:NEAR

fmt_body_1	DB	"[ASM][ObjectInitBody][1] obj=%p body=%p", 10, 0

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

		; --- debug trace [ASM][ObjectInitBody][1] params ---
		pushad
		push	eax
		push	ebx
		push	offset fmt_body_1
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---

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
