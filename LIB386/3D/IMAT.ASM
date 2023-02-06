;*══════════════════════════════════════════════════════════════════════════*

		.386p

		.model SMALL, C

		include	3D.INC

;*══════════════════════════════════════════════════════════════════════════*

		.DATA

		public	C InitMatrix
InitMatrix	dd	offset InitMatrixInitStd

;*══════════════════════════════════════════════════════════════════════════*

		extrn	C	InitMatrixStd:DWORD

;*══════════════════════════════════════════════════════════════════════════*

		.CODE

;*══════════════════════════════════════════════════════════════════════════*
InitMatrixInitStd proc

		call	[ds:InitMatrixStd]

		push	eax
		mov	eax, [ds:InitMatrixStd]

		mov	[ds:InitMatrix], eax
		pop	eax

		ret

InitMatrixInitStd endp

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
