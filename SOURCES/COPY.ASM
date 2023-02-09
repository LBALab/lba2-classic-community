; ╔═════════════════════════════════════════════════════════════════════════════════════════════════════════════╗
; ║                                                                                                             ║
; ║  Module de copie de blocks, avec ou sans stretching pour les changement de resolutions.                     ║
; ║            et de manip                                                                                      ║
; ║  Par un peu tout le monde chez Adeline :)                                                                   ║
; ║                                                                                                             ║
; ╚═════════════════════════════════════════════════════════════════════════════════════════════════════════════╝


YMIN_640x480		EQU	40
DY_IMAGE		EQU	200

;//***************************************************************************
			OPTION SCOPED
			.586
			.MMX
			.MODEL FLAT, SYSCALL


;//***************************************************************************
			.data

;//***************************************************************************
		extrn	SYSCALL	Log	:DWORD

;//***************************************************************************
		PUBLIC	SYSCALL	CopyFrameDoubleXY

;//***************************************************************************
;void	CopyFrameDoubleXY(void *src)

;#pragma aux CopyFrameDoubleXY	"*"	\
;	parm caller	[esi]		\
;	modify		[eax ebx ecx edx edi]

;//***************************************************************************

CopyFrameDoubleXY	dd	offset CopyFrameDoubleXYI

;//***************************************************************************
			.code

;//***************************************************************************
;void	CopyFrameDoubleXYI(void *src)

;#pragma aux CopyFrameDoubleXYI	"*"	\
;	parm caller	[esi]		\
;	modify		[eax ebx ecx edx edi]

CopyFrameDoubleXYI	PROC	USES ebp

		mov	edi, Log
		mov	ebp, DY_IMAGE

		add	edi, YMIN_640x480*640
		jmp	FirstLine
NextLine:
		add	edi,640
FirstLine:	mov	ecx,320/4
NextQuad:
		mov	edx,[esi]
		add	esi,4

		mov	eax,edx
		mov	ebx,edx

		mov	al,ah
		mov	bh,bl

		shl	eax,16

		shr	edx,16
		mov	ax,bx

		mov	ebx,edx
		mov	[edi],eax

		mov	[edi+640],eax
		mov	dl,dh

		shl	edx,16
		mov	bh,bl

		mov	dx,bx
		dec	ecx

		mov	[edi+4],edx
		mov	[edi+640+4],edx

		lea	edi,[edi+8]
		jnz	NextQuad

		dec	ebp
		jne	NextLine

		ret

CopyFrameDoubleXYI	ENDP

;//***************************************************************************
;			The
			End

