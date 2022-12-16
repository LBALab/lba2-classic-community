
		.386p

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	ObjectSetInterDep	:PROC
		EXTRN	C	ObjectSetInterFrame	:PROC

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	ObjectSetInterAnim

;*──────────────────────────────────────────────────────────────────────────*
;void	ObjectSetInterAnim(T_OBJ *obj)

;#pragma aux ObjectSetInterAnim		\
;	parm	[ebx]			\
;	modify	[eax ecx edx esi edi]


ObjectSetInterAnim PROC \
			uses eax ebx ecx edx edi esi ebp\
			obj: DWORD
			mov ebx, obj

		push	ebx
		push	ebx
		call	ObjectSetInterDep
		add esp, 4
		pop	ebx
		test	eax, STATUS_CHANGE	; any change?
		jz	exit
		test	eax, STATUS_FRAME	; frame ?
		jnz	exit

		push	eax
		push ebx
		call	ObjectSetInterFrame
		add esp, 4
		pop	eax
exit:
		ret

ObjectSetInterAnim	endp

;*══════════════════════════════════════════════════════════════════════════*
; 		The
		END
