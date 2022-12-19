;----------------------------------------------------------------------------
			.386

			.MODEL	SMALL, C

;----------------------------------------------------------------------------
			.CODE

;----------------------------------------------------------------------------
			public	C	FastCopyF

;----------------------------------------------------------------------------
;void	FastCopyF(void *dst, void *src, U32 len) ;

;#pragma aux FastCopyF	       	       	\
;	parm	[edi] [esi] [ecx]	\
;	modify	[eax ebx edx]

FastCopyF		PROC \
			uses eax ebx ecx edx edi esi ebp\
			dst: DWORD, \
			src: DWORD, \
			len: DWORD
			mov edi, dst
			mov esi, src
			mov ecx, len

			cmp	ecx, 8
			jb	cpybyte

			mov	edx, ecx
			and	ecx, 7

			shr	edx, 4
			jnc	loopcpy16

			fld	qword ptr[esi]
			lea	esi, [esi+8]
			fstp	qword ptr[edi]
			lea	edi, [edi+8]
			jz	cpybyte
loopcpy16:
			fld	qword ptr[esi]
			fstp	qword ptr[edi]
			fld	qword ptr[esi+8]
			fstp	qword ptr[edi+8]
			add	esi, 16
			add	edi, 16
			dec	edx
			jnz	loopcpy16
cpybyte:
			rep	movsb

			ret

FastCopyF		ENDP

;----------------------------------------------------------------------------
;			The
			End
