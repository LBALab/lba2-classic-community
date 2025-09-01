
		.686p
		.XMM

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.DATA

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	X0			:DWORD
		EXTRN	C	Y0			:DWORD
		EXTRN	C	Z0			:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	MatrixLib2		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	InitMatrixStd		:DWORD
		EXTRN	C	RotatePoint		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	TimerRefHR		:DWORD

		ASSUME  DS:SEG TimerRefHR

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	TransFctAnim		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	ObjectSetInterDep

;*══════════════════════════════════════════════════════════════════════════*
;void	ObjectSetInterDep(T_OBJ *obj)

;#pragma aux ObjectSetInterDep			\
;	parm		[ebx]			\
;	modify exact 	[eax ecx edx esi edi]

ObjectSetInterDep PROC \
			uses ebx ecx edx edi esi ebp\
			obj: DWORD
			mov ebx, obj

		mov	eax, [ebx].OBJ_3D.NextTexture
		mov	ecx, [ebx].OBJ_3D.NextBody

		mov	[ebx].OBJ_3D.Texture, eax
		mov	eax, [TimerRefHR]

		mov	[ebx].OBJ_3D.Body, ecx
		mov	edx, [ebx].OBJ_3D.ObjTime		; same time as before?

		cmp	eax,edx
		je	exit1
		jb	fixerror
forcerecomp:
		mov	[ebx].OBJ_3D.ObjTime, eax		; upgrade time

		mov	esi, [ebx].OBJ_3D.Anim
		mov	eax, [TransFctAnim]

		test	eax, eax
		jz	skiptrans

		push	esi
		call	eax
		mov		esi, eax
		add		esp,4
skiptrans:
		push	esi
		mov	eax, [ebx].OBJ_3D.ObjTime

		mov	edx, [ebx].OBJ_3D.NextOfsFrame
		add	esi, edx

		xor	edx, edx
		mov	ecx, [ebx].OBJ_3D.NextTimer

		mov	ebp, [ebx].OBJ_3D.LastTimer

		cmp	eax, ecx
		jae	timesup

		mov	[ebx].OBJ_3D.Status, STATUS_CHANGE

		sub	ecx, ebp		; ecx = nb steps
		sub	eax, ebp		; eax = delta steps
		mov	ebp, ecx
		inc	ecx
		shl	eax, 16			; delta steps  (1.15.16)
		shr	ecx, 1			; nb steps / 2
		add	eax, ecx		; to get nearest neighbour
		div	ebp			; delta steps / nb steps
		mov	ebp, eax		; factor (1.15.16)

returntime:
		mov	ecx, [ebx].OBJ_3D.Master	; Master
		mov	eax, [ebx].OBJ_3D.ObjAlpha
		mov	edx, [ebx].OBJ_3D.ObjBeta
		test	ecx, 1			; MasterRot ?
		jnz	animmaster
		mov	ecx, [ebx].OBJ_3D.ObjGamma
returnmaster:

		push	ebx
		push	esi

		mov	ebx, edx
		mov	edi, offset MatrixLib2

		call	[InitMatrixStd]

		pop	esi
		pop	ebx

		movsx	edx, word ptr[esi+2]	; X		(17.15.0)
		movsx	edi, word ptr[esi+4]	; Y
		movsx	eax, word ptr[esi+6]	; Z
		imul	edx, ebp		; result	(1.15.16)
		imul	edi, ebp
		sar	edx, 16			; result	(17.15.0)
		imul	eax, ebp
		mov	ecx, edx
		sar	edi, 16
		sub	ecx, [ebx].OBJ_3D.LastAnimStepX
		mov	[ebx].OBJ_3D.LastAnimStepX, edx; X
		push	ecx
		sar	eax, 16
		mov	edx, edi
		mov	ecx, eax
		sub	edx, [ebx].OBJ_3D.LastAnimStepY
		sub	ecx, [ebx].OBJ_3D.LastAnimStepZ
		mov	[ebx].OBJ_3D.LastAnimStepY, edi; Y
		mov	[ebx].OBJ_3D.LastAnimStepZ, eax; Z
		mov	[ebx].OBJ_3D.Interpolator, ebp	; interpolator

		pop	eax
		mov	esi, offset MatrixLib2
		mov	ebp, ebx
		mov	ebx, edx

		call	[RotatePoint]

		mov	ebx, ebp

		mov	eax, [X0]
		mov	edx, [Y0]
		mov	ecx, [Z0]
		add	eax, [ebx].OBJ_3D.ObjX
		add	edx, [ebx].OBJ_3D.ObjY
		add	ecx, [ebx].OBJ_3D.ObjZ
		mov	[ebx].OBJ_3D.ObjX, eax
		mov	eax, [ebx].OBJ_3D.Status
		mov	[ebx].OBJ_3D.ObjY, edx
		cmp	eax, STATUS_CHANGE
		pop	ebp
		mov	[ebx].OBJ_3D.ObjZ, ecx
		jne	frame

		ret
timesup:
		mov	ebp, 10000h
		mov	[ebx].OBJ_3D.Status, STATUS_FRAME+STATUS_CHANGE
		jmp	returntime
animmaster:
		mov	dx, [esi+10]		; Alpha
		shl	edx, 20
		mov	di, [esi+12]		; Beta
		shl	edi, 20
		mov	ax, [esi+14]		; Gamma
		shl	eax, 20
		sar	edx, 20			; data		(17.15.0)
		sar	edi, 20
		sar	eax, 20
		imul	edx, ebp		; result	(1.15.16)
		imul	edi, ebp
		sar	edx, 16			; result	(17.15.0)
		mov	ecx, [ebx].OBJ_3D.ObjAlpha
		imul	eax, ebp
		sar	edi, 16
		add	ecx, edx
		sub	ecx, [ebx].OBJ_3D.LastAnimStepAlpha
		mov	[ebx].OBJ_3D.LastAnimStepAlpha, edx	; Alpha
		and	ecx, 4095
		mov	edx, [ebx].OBJ_3D.ObjBeta
		mov	[ebx].OBJ_3D.ObjAlpha, ecx
		push	ecx
		add	edx, edi
		sar	eax, 16
		mov	ecx, [ebx].OBJ_3D.ObjGamma
		sub	edx, [ebx].OBJ_3D.LastAnimStepBeta
		add	ecx, eax
		and	edx, 4095
		mov	[ebx].OBJ_3D.LastAnimStepBeta, edi	; Beta
		sub	ecx, [ebx].OBJ_3D.LastAnimStepGamma
		mov	[ebx].OBJ_3D.LastAnimStepGamma, eax	; Gamma
		and	ecx, 4095
		mov	[ebx].OBJ_3D.ObjBeta, edx
		mov	[ebx].OBJ_3D.ObjGamma, ecx
		pop	eax
		jmp	returnmaster

		mov	eax, [edi+4]		; next

		and	eax, 4095
		jz      optequal1

		cmp     eax, 2048
		jle     ok1

		sub     eax, 4096
ok1:
		imul    eax, ebx
		sar	eax, 16
		add     edx, eax
		and	edx, 4095
optequal1:

frame:
		xor	eax, eax
		mov	[ebx].OBJ_3D.Interpolator, eax

		mov	[ebx].OBJ_3D.LastAnimStepX, eax; struct -> stepX = 0
		mov	[ebx].OBJ_3D.LastAnimStepY, eax; struct -> stepY = 0
		mov	[ebx].OBJ_3D.LastAnimStepZ, eax; struct -> stepZ = 0

		mov	[ebx].OBJ_3D.LastAnimStepAlpha, eax ; struct -> stepAlpha = 0
		mov	[ebx].OBJ_3D.LastAnimStepBeta, eax  ; struct -> stepBeta  = 0
		mov	[ebx].OBJ_3D.LastAnimStepGamma, eax ; struct -> stepGamma = 0

		mov	edx, [ebx].OBJ_3D.NextFrame
		mov	esi, [ebx].OBJ_3D.NextOfsFrame
		mov	[ebx].OBJ_3D.LastFrame, edx
		mov	[ebx].OBJ_3D.LastOfsFrame, esi
		mov	[ebx].OBJ_3D.LastOfsIsPtr, eax

		inc	edx			; next frame

		mov	eax, [ebx].OBJ_3D.NextNbGroups	; nb groups
		mov	[ebx].OBJ_3D.LastNbGroups, eax	; LastNbGroups
		mov	[ebx].OBJ_3D.NbGroups, eax	; nb groups

		lea	ecx, [eax*2-2]		; 2 dwords per group, no group 0
		cmp	edx, [ebx].OBJ_3D.NbFrames
		jge	lframe

		lea	eax, [esi+ecx*4+8+8]
backframe:
		add	esi, 16			; esi -> group 1
		mov	[ebx].OBJ_3D.NextOfsFrame, eax
		add	eax, ebp
		add	esi, ebp
		mov     edi, [ebx].OBJ_3D.ObjTime
		movzx	ebp, word ptr[eax+8]
		movzx	eax, word ptr[eax]
		mov	[ebx].OBJ_3D.Master, ebp
		mov	[ebx].OBJ_3D.NextFrame, edx
		mov	[ebx].OBJ_3D.LastTimer, edi	; this time
		add	eax, edi
		lea	edi, [ebx].OBJ_3D.CurrentFrame
		mov	[ebx].OBJ_3D.NextTimer, eax	; next time
		mov	eax, [ebx].OBJ_3D.Status

        ; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
        ;   is it confusing it with some other instruction? Is it not supported by 486?
		rep	movsd			; copy frame

		ret				; ok, return
lframe:
		mov	edx, [ebx].OBJ_3D.LoopFrame
		mov	eax, [ebx].OBJ_3D.LoopOfsFrame
		mov	[ebx].OBJ_3D.Status, STATUS_FRAME+STATUS_LAST+STATUS_CHANGE
		jmp	backframe
exit1:
		test	[ebx].OBJ_3D.Status, STATUS_BODY
		jnz	forcerecomp
		jmp	exit0
fixerror:
		mov	edx, [ebx].OBJ_3D.ObjTime
		mov	[ebx].OBJ_3D.ObjTime, eax
		sub	edx, eax
		mov	eax, [ebx].OBJ_3D.NextTimer
		mov	ecx, [ebx].OBJ_3D.LastTimer
		sub	eax, edx
		sub	ecx, edx
		mov	[ebx].OBJ_3D.NextTimer, eax
		mov	[ebx].OBJ_3D.LastTimer, ecx
exit0:
		xor	eax, eax
		mov	[ebx].OBJ_3D.Status, eax	; didn't do anything
		ret

ObjectSetInterDep endp

;*══════════════════════════════════════════════════════════════════════════*
; 		The
		END
