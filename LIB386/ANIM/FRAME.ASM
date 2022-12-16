
		.686p
		.XMM

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
		PUBLIC	C	ObjectSetFrame

;*══════════════════════════════════════════════════════════════════════════*
;void	ObjectSetFrame(T_OBJ *obj, U32 frame)

;#pragma aux ObjectSetFrame			\
;	parm		[ebx] [edx]		\
;	modify exact 	[eax ecx edx esi edi]


ObjectSetFrame	PROC \
			uses eax ebx ecx edx edi esi ebp\
			obj: DWORD, \
			frame: DWORD
			mov ebx, obj
			mov edx, frame

		mov	ecx, [ebx].OBJ_3D.NextBody
		mov	eax, [ebx].OBJ_3D.NextTexture
		cmp	edx, [ebx].OBJ_3D.NbFrames
		jae	error
		mov	[ebx].OBJ_3D.Body, ecx
		mov	[ebx].OBJ_3D.Texture, eax

		mov	esi, [ebx].OBJ_3D.Anim
		mov	eax, [TransFctAnim]

		test	eax, eax
		jz	skiptrans

		mov	ebp, edx

		push	esi
		call	eax
		mov		esi, eax
		add		esp,4

		mov	edx, ebp
skiptrans:
		push	esi
		xor	eax, eax

		mov	[ebx].OBJ_3D.Interpolator, eax
		mov	[ebx].OBJ_3D.LastAnimStepX, eax; struct -> stepX = 0
		mov	[ebx].OBJ_3D.LastAnimStepY, eax; struct -> stepY = 0
		mov	[ebx].OBJ_3D.LastAnimStepZ, eax; struct -> stepZ = 0

		movzx	ecx, word ptr[esi+2]	; ecx = nb groups in anim

		mov	[ebx].OBJ_3D.LastAnimStepAlpha, eax ; struct -> stepAlpha = 0
		mov	[ebx].OBJ_3D.LastAnimStepBeta, eax  ; struct -> stepBeta  = 0
		mov	[ebx].OBJ_3D.LastAnimStepGamma, eax ; struct -> stepGamma = 0
		mov	[ebx].OBJ_3D.LastOfsIsPtr, eax
		lea	eax, [ecx*8+8]		; infos frame + 4 WORD per group
		mov	[ebx].OBJ_3D.LastNbGroups, ecx	; struct -> LastNbGroups
		mov	[ebx].OBJ_3D.NextNbGroups, ecx	; struct -> NextNbGroups
		mov	[ebx].OBJ_3D.NbGroups, ecx	; struct -> NbGroups
		imul	eax, edx		;  * frame
		mov	ebp, TimerRefHR
		add	eax, 8			; skip header
		mov	[ebx].OBJ_3D.LastTimer, ebp	; struct -> LastTimer
		mov	[ebx].OBJ_3D.ObjTime, ebp	; struct -> ObjTime
		mov	[ebx].OBJ_3D.Status, STATUS_FRAME
		mov	[ebx].OBJ_3D.LastOfsFrame, eax	; struct -> LastOfsFrame
		mov	[ebx].OBJ_3D.LastFrame, edx	; struct -> LastFrame
		lea	ecx, [ecx*2-2]		; 2 DWORDs per group, no group 0
		mov	ebp, esi		; save ptr
		lea	esi, [esi+eax+16]	; esi -> group 1
		lea	edi, [ebx].OBJ_3D.CurrentFrame
		inc	edx

        ; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
        ;   is it confusing it with some other instruction? Is it not supported by 486?
		rep movsd			; init frame

		cmp	edx, [ebx].OBJ_3D.NbFrames
		je	lloopframe

		movzx	eax, word ptr[esi]	; get time
		sub	esi, ebp
fixedloop:
		pop	ecx
		add	eax, [ebx].OBJ_3D.LastTimer
		mov	[ebx].OBJ_3D.NextFrame, edx	; struct -> NextFrame
		mov	[ebx].OBJ_3D.NextOfsFrame, esi	; struct -> NextOfsFrame
		movzx	ecx, word ptr[ecx+esi+8]
		mov	[ebx].OBJ_3D.NextTimer, eax	; struct -> NextTimer
		mov	[ebx].OBJ_3D.Master, ecx
		mov     eax, STATUS_FRAME	; ok
		ret				; exit
lloopframe:
		movzx	eax, word ptr[ebp+8]	; get time
		xor	edx, edx
		mov	esi, 8
		jmp	fixedloop
error:
		xor	eax, eax		; error
		ret

ObjectSetFrame	endp

;*══════════════════════════════════════════════════════════════════════════*
; 		The
		END
