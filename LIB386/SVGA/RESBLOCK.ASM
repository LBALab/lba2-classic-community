;----------------------------------------------------------------------------
			.686P
			.XMM
			.MODEL SMALL, C

;----------------------------------------------------------------------------
			.DATA

;----------------------------------------------------------------------------
			EXTRN	C	TabOffLine:DWORD

			ASSUME DS:SEG TabOffLine

;----------------------------------------------------------------------------
			.CODE

;----------------------------------------------------------------------------
			PUBLIC	C	RestoreBlock

;----------------------------------------------------------------------------
;void	RestoreBlock( void *screen, void *buffer, S32 x0, S32 y0, S32 x1, S32 y1)

;#pragma aux RestoreBlock	"*"			\
;	parm	[edi] [esi] [eax] [ebx] [ecx] [edx]

RestoreBlock		PROC \
			uses eax ebx ecx edx edi esi ebp\
			screen: DWORD, buffer: DWORD, x0: DWORD, y0: DWORD, x1: DWORD, y1: DWORD
			mov edi, screen
			mov esi, buffer
			mov eax, x0
			mov ebx, y0
			mov ecx, x1
			mov edx, y1

			sub	ecx, eax		; ECX = DeltaX-1
			sub	edx, ebx		; EDX = DeltaY-1

			inc	ecx			; ECX = DeltaX
			add	edx, 2			; EDX = DeltaY+1

			mov	ebx, TabOffLine[ebx*4]
			add	edi, eax

			mov	eax, TabOffLine[1*4]
			add	edi, ebx

			push	ebp
			sub	eax, ecx

			mov	ebx, ecx		; EBX = DeltaX
			mov	ebp, ecx

			shr	ebx, 2
			and	ebp, 11b

			shr	edx, 1
			jnc	odd
Again:
			mov	ecx, ebx		; Delta X
			; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
            ;   is it confusing it with some other instruction? Is it not supported by 486?
			rep	movsd
			mov	ecx, ebp
			rep	movsb

			add	edi, eax
odd:
			mov	ecx, ebx		; Delta X
			; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
            ;   is it confusing it with some other instruction? Is it not supported by 486?
			rep	movsd
			mov	ecx, ebp
			rep	movsb

			add	edi, eax

			dec	edx
			jnz	Again

			pop	ebp

			ret
RestoreBlock		endp

;----------------------------------------------------------------------------
;			The
       			End
