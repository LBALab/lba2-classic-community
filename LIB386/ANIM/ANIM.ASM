
		.686p
		.XMM

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.DATA

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	TimerRefHR		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	Lib3DBufferAnim		:DWORD
		EXTRN	C	EndLib3DBufferAnim	:DWORD
		EXTRN	C	PtrLib3DBufferAnim	:DWORD

		EXTRN	C	TransFctAnim		:DWORD

		ASSUME  DS:SEG TimerRefHR

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	ObjectInitAnim

;*══════════════════════════════════════════════════════════════════════════*
;void	ObjectInitAnim(T_OBJ *obj, void *anim)

;#pragma aux ObjectInitAnim			\
;	parm		[ebx] [esi]		\
;	modify exact 	[eax ecx edx esi edi]

ObjectInitAnim	PROC \
			uses eax ebx ecx edx edi esi ebp\
			obj: DWORD, \
			anim: DWORD
			mov ebx, obj
			mov esi, anim

		mov	ecx, [ebx].OBJ_3D.NextBody
		mov	eax, [ebx].OBJ_3D.NextTexture
		cmp	esi, [ebx].OBJ_3D.Anim		; same anim ?
		mov	[ebx].OBJ_3D.Body, ecx
		mov	[ebx].OBJ_3D.Texture, eax
		je	exit

		mov	[ebx].OBJ_3D.Anim, esi		; init struct -> anim
		mov	eax, [TransFctAnim]

		test	eax, eax
		jz	skiptrans

		push	esi
		call	eax
		mov		esi, eax
		add		esp,4
skiptrans:
		xor	ebp, ebp
		movzx	eax, word ptr[esi+16]

		mov	[ebx].OBJ_3D.Interpolator, ebp
		mov	[ebx].OBJ_3D.Master, eax

		movzx	eax, word ptr[esi]	; eax = nb frames
		movzx	ecx, word ptr[esi+2]	; ecx = nb groups in anim
		movzx	edx, word ptr[esi+4]	; edx = loop frame

		mov	[ebx].OBJ_3D.NbFrames, eax	; struct -> NbFrames
		lea	eax, [ecx*8+8]		; infos frame + 4 WORD per group
		mov	[ebx].OBJ_3D.LoopFrame, edx	; struct -> LoopFrame
		imul    edx, eax
		mov	[ebx].OBJ_3D.NextNbGroups, ecx	; struct -> NextNbGroups
		add	edx, 8			; skip header anim

		mov	[ebx].OBJ_3D.LastAnimStepX, ebp; struct -> stepX = 0
		mov	[ebx].OBJ_3D.LastAnimStepY, ebp; struct -> stepY = 0
		mov	[ebx].OBJ_3D.LastAnimStepZ, ebp; struct -> stepZ = 0

		mov	[ebx].OBJ_3D.LoopOfsFrame, edx	; struct -> LoopOfsFrame

		mov	[ebx].OBJ_3D.LastAnimStepAlpha, ebp; struct -> stepAlpha = 0
		mov	[ebx].OBJ_3D.LastAnimStepBeta, ebp ; struct -> stepBeta  = 0
		mov	[ebx].OBJ_3D.LastAnimStepGamma, ebp; struct -> stepGamma = 0

		mov	edx, TimerRefHR
		cmp	[ebx].OBJ_3D.LastOfsFrame, ebp	; First anim ?
		mov	[ebx].OBJ_3D.LastTimer, edx	; struct -> LastTimer
		mov	[ebx].OBJ_3D.ObjTime, edx	; struct -> ObjTime
		je	firstanim

		mov	eax, 8
		mov	[ebx].OBJ_3D.Status, ebp
		mov	[ebx].OBJ_3D.NextOfsFrame, eax	; struct -> NextFrame 0
		mov	eax, [ebx].OBJ_3D.NbGroups
		mov	[ebx].OBJ_3D.NextFrame, ebp	; struct -> NextFrame 0

		mov	[ebx].OBJ_3D.LastNbGroups, ecx	; struct -> LastNbGroups

		cmp	ecx, eax
		jne	changegroups
okgroups:
		movzx	eax, word ptr[esi+8]	; get deltatime for frame
		lea	ecx, [ecx*2-2]		; 2 DWORDs per group, no group 0
		add	edx, eax
		xor	eax, eax
		mov	edi, PtrLib3DBufferAnim	; edi -> looping anim buffer
		inc	eax
		mov	[ebx].OBJ_3D.NextTimer, edx	; struct -> NextTimer
		mov	[ebx].OBJ_3D.LastOfsFrame, edi	; copy will be last frame
		mov	[ebx].OBJ_3D.LastOfsIsPtr, eax
		add	edi, 16
		dec	eax
		lea	esi, [ebx].OBJ_3D.CurrentFrame	; esi -> CurrentFrame

		mov	dword ptr[edi-16], eax
		mov	dword ptr[edi-12], eax
		mov	dword ptr[edi-8], eax
		mov	dword ptr[edi-4], eax
		dec	eax
		mov	[ebx].OBJ_3D.LastFrame, eax	; LastFrame = -1
		; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
        ;   is it confusing it with some other instruction? Is it not supported by 486?
		rep	movsd			; do copy...

		test	ebp, ebp
		jz	noincrease

		mov	eax, [TransFctAnim]
		mov	esi, [ebx].OBJ_3D.Anim

		test	eax, eax
		jz	skiptrans2

		push	esi
		call	eax
		mov		esi, eax
		add		esp,4
skiptrans2:
		add	esi, [ebx].OBJ_3D.NextOfsFrame
		mov	ecx, [ebx].OBJ_3D.NbGroups

		sub	ecx, ebp

		shl	ecx, 1			; *2
		lea	esi, [esi+ebp*8+8]

        ; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
        ;   is it confusing it with some other instruction? Is it not supported by 486?
		rep	movsd
noincrease:
		cmp	edi, EndLib3DBufferAnim	; check buffer for next time
		jg	loopbuf
okbuf:
		mov	PtrLib3DBufferAnim, edi	; write back buffer pointer
exit:
		mov	eax, 1			; ok, exit
		ret
changegroups:
		jb	reducegroups
increasegroups:
		mov	ebp, eax
		mov	[ebx].OBJ_3D.NbGroups, ecx
		mov	ecx, eax
		jmp	okgroups
reducegroups:
		mov	[ebx].OBJ_3D.NbGroups, ecx
		mov	eax, ecx
		jmp	okgroups

loopbuf:
		mov	edi, Lib3DBufferAnim
		jmp	okbuf
lloopframe:
		movzx	eax, word ptr[ebp+8]	; get time
		mov	esi, 8
		jmp	fixedloop
firstanim:
		mov	ebp, 8
		xor	eax, eax
		mov	[ebx].OBJ_3D.Status, STATUS_FRAME
		mov	[ebx].OBJ_3D.LastOfsFrame, ebp	; struct -> LastOfsFrame -> 0
		mov	[ebx].OBJ_3D.LastOfsIsPtr, eax
		mov	[ebx].OBJ_3D.LastFrame, eax	; struct -> LastFrame 0

		mov	[ebx].OBJ_3D.LastNbGroups, ecx	; struct -> LastNbGroups
		mov	[ebx].OBJ_3D.NbGroups, ecx	; struct -> NbGroups

		lea	ecx, [ecx*2-2]		; 2 DWORDs per group, no group 0

		mov	ebp, esi		; save ptr
		add	esi, 16+8		; esi -> group 1
		lea	edi, [ebx].OBJ_3D.CurrentFrame

        ; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
        ;   is it confusing it with some other instruction? Is it not supported by 486?
		rep	movsd			; init frame

		cmp	[ebx].OBJ_3D.NbFrames, 1
		je	lloopframe

		movzx	eax, word ptr[esi]	; get time
		sub	esi, ebp
		inc	ecx
fixedloop:
		mov	[ebx].OBJ_3D.NextOfsFrame, esi	; struct -> NextOfsFrame -> 1
		mov	[ebx].OBJ_3D.NextFrame, ecx	; struct -> NextFrame (0/1)
		add	edx, eax
		mov     eax, 1			; ok
		mov	[ebx].OBJ_3D.NextTimer, edx	; struct -> NextTimer
		ret				; exit

ObjectInitAnim	endp

;*══════════════════════════════════════════════════════════════════════════*
; 		The
		END
