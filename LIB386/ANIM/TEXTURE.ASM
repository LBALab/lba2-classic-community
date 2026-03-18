
		.386p

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.DATA

		EXTRN	C	printf	:NEAR

fmt_texture_1	DB	"[ASM][ObjectInitTexture][1] obj=%p tex=%p", 10, 0

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	ObjectInitTexture

;*══════════════════════════════════════════════════════════════════════════*
;void	ObjectInitTexture(T_OBJ *obj, void *texture)

;#pragma aux ObjectInitTexture		\
;	parm		[ebx] [eax]	\
;	modify exact 	[]

ObjectInitTexture proc

		; --- debug trace [ASM][ObjectInitTexture][1] params ---
		pushad
		push	eax
		push	ebx
		push	offset fmt_texture_1
		call	printf
		add	esp, 12
		popad
		; --- end debug trace ---

		mov	[ebx].OBJ_3D.NextTexture, eax
		cmp	[ebx].OBJ_3D.Texture, -1
		jne	oktexture

		mov	[ebx].OBJ_3D.Texture, eax
oktexture:
		ret

ObjectInitTexture endp

;*══════════════════════════════════════════════════════════════════════════*
; 		The
		END
