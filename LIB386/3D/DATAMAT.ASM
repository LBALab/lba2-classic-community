;*══════════════════════════════════════════════════════════════════════════*
		.386p

		include	MAT.INC

;*══════════════════════════════════════════════════════════════════════════*
;DGROUP		GROUP	_DATA

;_DATA		SEGMENT USE32 PARA PUBLIC 'DATA'
		.model FLAT, C
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
ALIGN 8
		public	C	MatriceWorld

MatriceWorld	Struc_MatriceMAT	<>

ALIGN 8
		public	C	MatrixLib1

MatrixLib1	Struc_MatriceMAT	<>

ALIGN 8
		public	C MatrixLib2

MatrixLib2	Struc_MatriceMAT	<>

ALIGN 8
		public	C MatriceRot

MatriceRot	Struc_MatriceMAT	<>

;*══════════════════════════════════════════════════════════════════════════*
_DATA		ENDS

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
