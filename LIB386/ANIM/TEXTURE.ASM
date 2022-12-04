
		.386p

		.MODEL	SMALL, SYSCALL

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	SYSCALL	ObjectInitTexture

;*══════════════════════════════════════════════════════════════════════════*
;void	ObjectInitTexture(T_OBJ *obj, void *texture)

;#pragma aux ObjectInitTexture		\
;	parm		[ebx] [eax]	\
;	modify exact 	[]

ObjectInitTexture proc

		mov	[ebx].NextTexture, eax
		cmp	[ebx].Texture, -1
		jne	oktexture

		mov	[ebx].Texture, eax
oktexture:
		ret

ObjectInitTexture endp

;*══════════════════════════════════════════════════════════════════════════*
; 		The
		END
