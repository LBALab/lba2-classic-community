/*
; ╔═══════════════════════════════════════════╗
; ║ ┌─────┐ ┌─────┐ │      │    │             ║
; ║ │     │ │     │ │      │    │             ║
; ║ │     │ │     │ │      │    │             ╚═══════════════════════╤═══════════╗
; ║ │     │ │     │ │      │    │    ┌──── ─                          │Début:     ║
; ║ ├─────┘ │     │ │      └────┤    │       │    │    ┌─── ├──┐      │le 02/08/95║
; ║ │       │     │ │           │    ├──   │ │    │    │    │         │           ║
; ║ │       │     │ │           │ /  │     │ │    │    ├─   │         │           ║
; ║ │       │     │ │           │ \  │     │ │    │    │    │         │           ║
; ║ │       └─────┘ └─────      │ /  │     │ └─── └─── └─── │         │           ║
; ║                                                                   │           ║
; ║                                                                   │           ║
; ╟────────────────────────────────────────┐                          │           ║
; ║Module: CONSTANTES ET MACROS            │                          │           ║
; ╚════════════════════════════════════════╧╗                         │ MAJ:      ║
;                                           ║                         │ 09/11/95  ║
;                                           ╚═════════════════════════╧═══════════╝





; ╔═══════════════════════════════════════════════════════════════════════════╗
; ║                                                                           ║
; ║  Structures, macros et constantes                                         ║
; ║                                                                           ║
; ╚═══════════════════════════════════════════════════════════════════════════╝

Struc_EntryPoint	EQU	STRUC_POINT
Pt_Coord		EQU	0

						; Structure d'un point tel
STRUC_POINT		STRUC			; qu'on peut le trouver dans
  Pt_XE			dw	0		; la table passee en parametre
  Pt_YE			dw	0		; aux routines de remplissage
  Pt_MapU		dw	0		; Coord de mapping
  Pt_MapV		dw	0
  Pt_Light		dw	0		; Lumiere
  Pt_ZO			dw	0		; Z norme sur 16 bits
  Pt_W			dd	0		; 1/Z norme sur 32 bits
STRUC_POINT		ENDS



STRUC_CLIPVERTEX	STRUC
  V_X0			dd	0
  V_Y0			dd	0
  V_Z0			dd	0
  V_MapU		dw	0
  V_MapV		dw	0
  V_Light		dw	0
  V_Dummy		dw	0
STRUC_CLIPVERTEX	ENDS


OUI			EQU	1
NON			EQU	0
TRUE			EQU	1
FALSE			EQU	0


SIZE_VIDEOLINE		EQU	640
*/
// Nb de cotes max apres clipping
#define NB_MAX_COTES	16
#define NB_MAX_EDGES	NB_MAX_COTES

#define PERSP_SHIFT 4 // Shift value (log2(PERSP_VALUE))
#define PERSP_VALUE (1 << PERSP_SHIFT) // Nb of pixels rendered between two corrections

// Pour le Flag_Display
// Champ de bits
#define READ_NEXT_G 1		// Bit 0 : Lire le prochain sommet a gauche?
#define READ_NEXT_D 2		// Bit 1	: idem a droite ?
#define END_READ 4		// Bit 2	: quitter ?
#define READ_NEXT_L 1		// Bit 0 : Read next left point
#define READ_NEXT_R 2		// Bit 1	: Read next right point

#define CLIP_FLAT	1		// Clipper les bords des polys
#define CLIP_LIGHT	2		// Clipper la couleur
#define CLIP_TEXTURE	4		// Clipper la texture
#define CLIP_TEXTUREZ	8		// Clipper la texture plus le Z (cas d'une perspective corrected)
#define CLIP_ZBUFFER	16		// Heu, juste pour dire ZBuffer On
#define CLIP_ZFOG	32		// Flag de Fog pour le cas du TextureZ Fog
#define CLIP_DITHER	64		// If dithering On

/*

FOG_DECAL		EQU	16+4		; Decalage "a la mano" pour le fog
						; dans notre cas: 16+4 pour 16 bits
						; de virgule et 4 bits de partie entiere.


ROUND_CHOP		MACRO	R1:req, R2:req
;	IFNB	R2
;			add	R1,R1
;			adc	R1,1
;			sar	R1,1
;	ELSE
			mov	R2,R1
			shr	R1,31
			add	R1,R2
;	ENDIF
			ENDM
*/