
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

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
IFDEF	REG_CALL
		EXTRN	C	malloc_:PROC
		EXTRN	C	free_:	PROC
		EXTRN	C	atexit_:PROC
ELSE
		EXTRN	C	malloc:	PROC
		EXTRN	C	free:	PROC
		EXTRN	C	atexit:	PROC
ENDIF

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	InitObjects
		PUBLIC	C	ClearObjects

;*──────────────────────────────────────────────────────────────────────────*
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

;*──────────────────────────────────────────────────────────────────────────*
;void	ClearObjects()

;#pragma aux ClearObjects	       	\
;	modify	[eax ecx edx]

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

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
