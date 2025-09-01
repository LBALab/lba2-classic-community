
		.386p

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.DATA

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	TimerRefHR		:DWORD

		ASSUME  DS:SEG TimerRefHR

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	TransFctAnim		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	ObjectSetInterFrame

;*──────────────────────────────────────────────────────────────────────────*
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

;*══════════════════════════════════════════════════════════════════════════*
; 		The
		END
