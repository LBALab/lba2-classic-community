;*══════════════════════════════════════════════════════════════════════════*

		.386p
		.MODEL SMALL, C
		OPTION	SCOPED

;*══════════════════════════════════════════════════════════════════════════*
MAX_SCREEN_Y_RES	equ	1024

;*══════════════════════════════════════════════════════════════════════════*
		.DATA

;*══════════════════════════════════════════════════════════════════════════*
		PUBLIC	C	Phys
		PUBLIC	C	Log
		PUBLIC	C	Screen
		PUBLIC	C	TabOffLine
		PUBLIC	C	TabOffPhysLine

		PUBLIC	C	ModeResX
		PUBLIC	C	ModeResY
		PUBLIC	C	ModeMemory

		PUBLIC	C	BytesScanLine

		ASSUME DS:SEG TabOffLine

;*══════════════════════════════════════════════════════════════════════════*
;ALIGN	32					; already aligned

Phys		dd	0
Log		dd	0
Screen		dd	0

ModeResX	dd	0
ModeResY	dd	0
ModeMemory	dd	0

BytesScanLine	dd	0

TabOffLine	dd	MAX_SCREEN_Y_RES dup (?)

TabOffPhysLine	dd	MAX_SCREEN_Y_RES dup (?)

;*══════════════════════════════════════════════════════════════════════════*

;*══════════════════════════════════════════════════════════════════════════*
;		The
		END
