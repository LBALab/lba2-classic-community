#include <OBJECT/AFF_OBJ.H>

#include <SVGA/SCREENXY.H>
#include <3D/CAMERA.H>
#include <3D/PROJ.H>
#include <3D/ROT3D.H>
#include <3D/LROT3D.H>
#include <3D/ROTMAT.H>
#include <3D/IMATTRA.H>
#include <3D/ROTRALIS.H>
#include <3D/LITLISTF.H>
#include <3D/COPYMAT.H>
#include <3D/LPROJ.H>
#include <SVGA/CLIP.H>
#include <POLYGON/POLY.H>

#include <stdio.h>
#include <string.h>

#define MAX_NB_POINTS 550
#define MAX_NB_POLYS 550
#define MAX_NB_GROUPES 30
#define SIZE_MATRICE (sizeof(S16) * 15)
#define W_NORM (1 << 30)

typedef S32 (*Poly_Jump_Fn)(U32 typePoly, void *poly);

extern Poly_Jump_Fn Poly_Jump[];

typedef struct {
  U16 OrgGroupe;
  U16 OrgPoint;
  U16 NbPts;
  U16 NbNorm;
} T_OBJ_GROUPE;

T_OBJ_POINT Obj_ListRotatedPoints[MAX_NB_POINTS];
TYPE_PT Obj_ListProjectedPoints[MAX_NB_POINTS];

U8 *ObjPtrMap = 0; // Ptr of the texture page for the current object
U32 *PtrTextures = 0; // Ptr of texture handles

// Ptr of the object
T_BODY_HEADER *lBody = NULL;

S32 *Off_Sort = NULL; // Current offset in sort list
U32 Nb_Sort = 0; // Nb entries in sort list

T_GROUP_INFO *ListGroupAnim = NULL; // Current frame anim info
TYPE_VT16 *PtrPoints = NULL; // Current point pointer
TYPE_VT16 *PtrNormales = NULL; // Current normals pointer
TYPE_VT16 *PtrNormFaces = NULL; // Current norm-faces pointer
TYPE_VT16 *PtrRotPoints = NULL; // Current rotated points pointer
PTR_U16 PtrListLights = NULL; // Current lights pointer
PTR_U16 PtrListLightsFaces = NULL; // Current light-faces pointer
U32 NbGroupes = 0; // Nb of groups in object
T_OBJ_GROUPE *PtrGroupes = 0; // Pointer on groups definition
TYPE_MAT *Current_Matrix = NULL; // Pointer on current matrix
Func_TransNumPtr *TransFctBody = NULL; // Pointer to a conversion fct that converts a body number into its address

// Light list
U16 ListLights[MAX_NB_POINTS+MAX_NB_POLYS];

// Sort list:
// * First half of list contain Z:Number
// * Second half: one pointer for each number
S32 ListSort[MAX_NB_POLYS * 2];

// List of vertex for Fill_Poly
Struc_Point ListFillPoly[4];

// Object position in world space
S32 PosXWr = 0;
S32 PosYWr = 0;
S32 PosZWr = 0;

// Temporary translation components
S32 TransX = 0;
S32 TransY = 0;
S32 TransZ = 0;

// Matrix list
TYPE_MAT TabMat[MAX_NB_GROUPES];

// *** Types des polygones ***
#define POLY_SOLID 0
#define POLY_FLAT 1
#define POLY_TRANSPARENT 2
#define POLY_TRAME 3
#define POLY_GOURAUD 4
#define POLY_DITHER 5
#define POLY_GOURAUD_TABLE 6
#define POLY_DITHER_TABLE 7

#define POLY_TEXTURE_SOLID 8
#define POLY_TEXTURE_FLAT 9
#define POLY_TEXTURE_GOURAUD 10
#define POLY_TEXTURE_DITHER 11
#define POLY_TEXTURE_SOLID_INC 12
#define POLY_TEXTURE_FLAT_INC 13
#define POLY_TEXTURE_GOURAUD_INC 14
#define POLY_TEXTURE_DITHER_INC 15

#define POLY_TEXTUREZ_SOLID 16
#define POLY_TEXTUREZ_FLAT 17
#define POLY_TEXTUREZ_GOURAUD 18
#define POLY_TEXTUREZ_DITHER 19
#define POLY_TEXTUREZ_SOLID_INC 20
#define POLY_TEXTUREZ_FLAT_INC 21
#define POLY_TEXTUREZ_GOURAUD_INC 22
#define POLY_TEXTUREZ_DITHER_INC 23

#define MAX_POLYS_TYPE 24
#define MAX_LINES_TYPE 1

// *** Bits de flag ***
#define MASK_QUADRILATERE (1 << 15)	// Au niveau des faces
#define MASK_ENVIRONMENT (1 << 14)

typedef struct {
  U16 TypePoly;
  U16 NbPoly;
  U32 OffNextType;
} T_POLY_HEADER;

typedef struct {
  U16 Type;
  U16 Coul;
  U16 P1;
  U16 P2;
} T_OBJ_LINE;

typedef struct {
  U16 Type;
  U16 Coul;
  U16 P1;
  U16 Rayon;
} T_OBJ_SPHERE;

typedef struct {
  U16 P1;
  U16 P2;
  U16 P3;
  U16 __padding;
  U16 Couleur;
  U16 Normale;
} STRUC_POLY3_LIGHT;

typedef struct {
  U16 P1;
  U16 P2;
  U16 P3;
  U16 HandleText;
  U16 Couleur;
  U16 Normale;
  U16 U1;
  U16 V1;
  U16 U2;
  U16 V2;
  U16 U3;
  U16 V3;
} STRUC_POLY3_TEXTURE;

typedef struct {
  U16 P1;
  U16 P2;
  U16 P3;
  U16 HandleEnv;
  U16 Couleur;
  U16 Normale;
  U16 Scale;
	U16 __padding;
} STRUC_POLY3_ENV;

typedef struct {
  U16 P1;
  U16 P2;
  U16 P3;
  U16 P4;
  U16 Couleur;
  U16 Normale;
} STRUC_POLY4_LIGHT;

typedef struct {
  U16 P1;
  U16 P2;
  U16 P3;
  U16 P4;
  U16 Couleur;
  U16 Normale;
  U16 U1;
  U16 V1;
  U16 U2;
  U16 V2;
  U16 U3;
  U16 V3;
  U16 U4;
  U16 V4;
  U16 HandleText;
	U16 __padding;
} STRUC_POLY4_TEXTURE;

typedef struct {
  U16 P1;
  U16 P2;
  U16 P3;
  U16 P4;
  U16 Couleur;
  U16 Normale;
  U16 Scale;
  U16 HandleEnv;
} STRUC_POLY4_ENV;

void GetZO3(Struc_Point *dst, U16 index1, U16 index2, U16 index3)
{
  if (!Fill_Flag_ZBuffer || TypeProj != TYPE_3D)
  {
    return;
  }

  U16 indices[3] = { index1, index2, index3 };

  for (int i = 0; i < 3; i++)
  {
    U16 idx = indices[i];
    S32 Z = -Obj_ListRotatedPoints[idx].Z - PosZWr;
    dst[i].Pt_ZO = (Z * Fill_ZBuffer_Factor) >> 16;
  }
}

void GetZO4(Struc_Point *dst, U16 index1, U16 index2, U16 index3, U16 index4)
{
  if (!Fill_Flag_ZBuffer || TypeProj != TYPE_3D)
  {
    return;
  }

  U16 indices[4] = { index1, index2, index3, index4 };

  for (int i = 0; i < 4; i++)
  {
    U16 idx = indices[i];
    S32 Z = -Obj_ListRotatedPoints[idx].Z - PosZWr;
    dst[i].Pt_ZO = (Z * Fill_ZBuffer_Factor) >> 16;
  }
}

/*
; ╔════════════════════════════════════════════════════════════════════════════╗
; ║                                                                            ║
; ║  Les DATA                                                                  ║
; ║                                                                            ║
; ╚════════════════════════════════════════════════════════════════════════════╝

;			.DATA
;DGROUP			GROUP	_DATA
;_DATA			SEGMENT	USE32 PUBLIC PARA 'DATA'

Extrn	C		LongRotatePoint	:	DWORD	; Point rotation (32 bit)
Extrn	C		RotatePoint	:	DWORD	; Point rotation (16 bit)
Extrn	C		RotatePointNoMMX:	DWORD	; Point rotation (16 bit)
Extrn	C		RotateMatrix	:	DWORD	; Matrix rotation
Extrn	C		CopyMatrix	:	DWORD	; Matrix copy
Extrn	C		RotTransList	:	DWORD	; Rotation of list of point (16 bit)
Extrn	C		ProjectList	:	DWORD	; Projection of a list of points
							; The points are first translated:
							; XE = Lx*(Xlist+TX-CameraXr)/(Zlist+TZ-CameraZr)
							; YE = Ly*(Ylist+TY-CameraYr)/(Zlist+TZ-CameraZr)

Extrn	C		LightList	:	DWORD	; Calculates the light at each point of the list

Extrn	C		InitMatrixTrans	:	DWORD	; Initialize the translation scalars for a given matrix



Extrn	C		ScreenXMin	:	DWORD	; Screen size
Extrn	C		ScreenXMax	:	DWORD	; Screen size
Extrn	C		ScreenYMin	:	DWORD	; Screen size
Extrn	C		ScreenYMax	:	DWORD	; Screen size

Extrn	C		ClipXMin	:	DWORD	; Clip coord
Extrn	C		ClipXMax	:	DWORD	; Clip coord
Extrn	C		ClipYMin	:	DWORD	; Clip coord
Extrn	C		ClipYMax	:	DWORD	; Clip coord

Extrn	C		TypeProj	:	DWORD	; Projection type
Extrn	C		MatriceWorld	:	DWORD	; World matrix

Extrn	C		X0		:	DWORD	; Return value of RotatePoint
Extrn	C		Y0		:	DWORD	; Return value of RotatePoint
Extrn	C		Z0		:	DWORD	; Return value of RotatePoint

				; poly.asm
Extrn	C		RepMask		:	DWORD	; Repeat zone for texture mapping
Extrn	C		PtrMap		:	DWORD	; Ptr of texture
Extrn	C		Fill_Flag_ZBuffer:	BYTE	; TRUE if Zbuffer active
Extrn	C		Fill_ZBuffer_Factor:	DWORD	; ZBuffer factor to have a 16 bit zbuf
Extrn	C		Fill_Flag_NZW	:	BYTE	; TRUE if no Z Buffer updates


				; Camera.asm
Extrn	C		CameraXr	:	DWORD	; Camera coord in observer space
Extrn	C		CameraYr	:	DWORD
Extrn	C		CameraZr	:	DWORD
Extrn	C		NearClip	:	DWORD
Extrn	C		CameraZrClip	:	DWORD

Extrn	C		LFactorX	:	DWORD


			ALIGN	4
TestVisible		dd	TestVisibleI
;TestVisible		dd	TestVisibleF

PtrTextures		dd	0		; Ptr of texture handles
ObjPtrMap		dd	0		; Ptr of the texture page for the current object

lBody			dd	0		; Ptr of the object

Off_Sort		dd	0		; Current offset in sort list
Nb_Sort			dd	0		; Nb entries in sort list


PosXWr			dd	0		; Object position in world space
PosYWr			dd	0
PosZWr			dd	0

			ALIGN	8
				; Matrix list
TabMat			db	SIZE_MATRICE*MAX_NB_GROUPES dup (?)

			ALIGN	8
				; Rotated points list
Obj_ListRotatedPoints	STRUC_OBJ_POINT MAX_NB_POINTS dup (<>)

			ALIGN	8
				; Projected points list
Obj_ListProjectedPoints	STRUC_PT 	MAX_NB_POINTS dup (<>)

			ALIGN	8
				; 4 first are source normals; 4 last are rotated ones
				; (used for env. mapping)
ListNormEnv		dq	8 dup (?)


				; Light list
ListLights		dw	MAX_NB_POINTS+MAX_NB_POLYS dup (?)

			ALIGN	4
				; Sort list:
				;	* First half of list contain Z:Number
				;	* Second half: one pointer for each number
ListSort		dd	MAX_NB_POLYS*2 dup (?)

			ALIGN	8
				; List of vertex for Fill_Poly
ListFillPoly		STRUC_POINT 4 dup (<>)


			ALIGN	4
TransX			dd	0	; Temporary translation components
TransY			dd	0
TransZ			dd	0

ListGroupAnim		dd	0	; Current frame anim info
PtrPoints		dd	0	; Current point pointer
PtrNormales		dd	0	; Current normals pointer
PtrNormFaces		dd	0	; Current norm-faces pointer
PtrRotPoints		dd	0	; Current rotated points pointer
PtrListLights		dd	0	; Current lights pointer
PtrListLightsFaces	dd	0	; Current light-faces pointer
NbGroupes		dd	0	; Nb of groups in object
PtrGroupes		dd	0	; Pointer on groups definition
Current_Matrix		dd	0	; Pointer on current matrix
TransFctBody		dd	0	; Pointer to a conversion fct that converts a body number into its address






Table_Matrix		Label	DWORD
AA			=	0
REPT	MAX_GROUPS
			dd	Offset TabMat + AA
AA			=	AA+SIZE_MATRICE
ENDM



;_DATA			ENDS


; ╔════════════════════════════════════════════════════════════════════════════╗
; ║                                                                            ║
; ║  Le CODE                                                                   ║
; ║                                                                            ║
; ╚════════════════════════════════════════════════════════════════════════════╝

;_TEXT			SEGMENT	USE32 PARA PUBLIC 'CODE'
;			ASSUME	CS:_TEXT, DS:DGROUP, ES:DGROUP, SS:DGROUP
			.CODE


Extrn	C		Fill_PolyFast	:	PROC
Extrn	C		Fill_Sphere	:	PROC
Extrn	C		Line_A		:	PROC

*/

// *************
// *** SOLID ***
// *************
S32 Triangle_Solid(U32 typePoly, void *poly)
{
  STRUC_POLY3_LIGHT *poly3 = (STRUC_POLY3_LIGHT *)poly;
  GetZO3(ListFillPoly, poly3->P1, poly3->P2, poly3->P3);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly3->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly3->P1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly3->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly3->P2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly3->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly3->P3].Y;

  return Fill_Poly(typePoly >> 2, poly3->Couleur & 0xFF, 3, ListFillPoly);
}

S32 Quad_Solid(U32 typePoly, void *poly)
{
  STRUC_POLY4_LIGHT *poly4 = (STRUC_POLY4_LIGHT *)poly;
  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;

  return Fill_Poly(typePoly >> 2, poly4->Couleur & 0xFF, 4, ListFillPoly);
}

// ************
// *** FLAT ***
// ************
S32 Triangle_Flat(U32 typePoly, void *poly)
{
  STRUC_POLY3_LIGHT *poly3 = (STRUC_POLY3_LIGHT *)poly;

  GetZO3(ListFillPoly, poly3->P1, poly3->P2, poly3->P3);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly3->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly3->P1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly3->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly3->P2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly3->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly3->P3].Y;

  U16 normal = ListLights[poly3->Normale] >> 8;
  U8 color = (poly3->Couleur + normal) & 0xFF;

  return Fill_Poly(1, color, 3, ListFillPoly);
}

S32 Quad_Flat(U32 typePoly, void *poly)
{
  STRUC_POLY4_LIGHT *poly4 = (STRUC_POLY4_LIGHT *)poly;

  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;

  U16 normal = ListLights[poly4->Normale] >> 8;
  U8 color = (poly4->Couleur + normal) & 0xFF;

  return Fill_Poly(1, color, 4, ListFillPoly);
}

// ***************
// *** GOURAUD ***
// ***************
S32 Triangle_Gouraud(U32 typePoly, void *poly)
{
  STRUC_POLY3_LIGHT *poly3 = (STRUC_POLY3_LIGHT *)poly;

  GetZO3(ListFillPoly, poly3->P1, poly3->P2, poly3->P3);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly3->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly3->P1].Y;
  ListFillPoly[0].Pt_Light = ListLights[poly3->P1];
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly3->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly3->P2].Y;
  ListFillPoly[1].Pt_Light = ListLights[poly3->P2];
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly3->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly3->P3].Y;
  ListFillPoly[2].Pt_Light = ListLights[poly3->P3];

  return Fill_Poly(typePoly >> 2, poly3->Couleur, 3, ListFillPoly);
}

S32 Quad_Gouraud(U32 typePoly, void *poly)
{
  STRUC_POLY4_LIGHT *poly4 = (STRUC_POLY4_LIGHT *)poly;
  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[0].Pt_Light = ListLights[poly4->P1];
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[1].Pt_Light = ListLights[poly4->P2];
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[2].Pt_Light = ListLights[poly4->P3];
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;
  ListFillPoly[3].Pt_Light = ListLights[poly4->P4];

  return Fill_Poly(typePoly >> 2, poly4->Couleur & 0xFF, 4, ListFillPoly);
}

// *********************
// *** TEXTURE SOLID ***
// *********************
S32 Triangle_Texture_Solid(U32 typePoly, void *poly)
{
  STRUC_POLY3_TEXTURE *poly3 = (STRUC_POLY3_TEXTURE *)poly;
  GetZO3(ListFillPoly, poly3->P1, poly3->P2, poly3->P3);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly3->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly3->P1].Y;
  ListFillPoly[0].Pt_MapU = poly3->U1;
  ListFillPoly[0].Pt_MapV = poly3->V1;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly3->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly3->P2].Y;
  ListFillPoly[1].Pt_MapU = poly3->U2;
  ListFillPoly[1].Pt_MapV = poly3->V2;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly3->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly3->P3].Y;
  ListFillPoly[2].Pt_MapU = poly3->U3;
  ListFillPoly[2].Pt_MapV = poly3->V3;

  U32 textureInfo = PtrTextures[poly3->HandleText];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);

  return Fill_Poly(typePoly >> 2, 0, 3, ListFillPoly);
}

S32 Quad_Texture_Solid(U32 typePoly, void *poly)
{
  STRUC_POLY4_TEXTURE *poly4 = (STRUC_POLY4_TEXTURE *)poly;
  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[0].Pt_MapU = poly4->U1;
  ListFillPoly[0].Pt_MapV = poly4->V1;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[1].Pt_MapU = poly4->U2;
  ListFillPoly[1].Pt_MapV = poly4->V2;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[2].Pt_MapU = poly4->U3;
  ListFillPoly[2].Pt_MapV = poly4->V3;
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;
  ListFillPoly[3].Pt_MapU = poly4->U4;
  ListFillPoly[3].Pt_MapV = poly4->V4;

  U32 textureInfo = PtrTextures[poly4->HandleText];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);

  return Fill_Poly(typePoly >> 2, 0, 4, ListFillPoly);
}

// ********************
// *** TEXTURE FLAT ***
// ********************
S32 Triangle_Texture_Flat(U32 typePoly, void *poly)
{
  STRUC_POLY3_TEXTURE *poly3 = (STRUC_POLY3_TEXTURE *)poly;
  GetZO3(ListFillPoly, poly3->P1, poly3->P2, poly3->P3);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly3->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly3->P1].Y;
  ListFillPoly[0].Pt_MapU = poly3->U1;
  ListFillPoly[0].Pt_MapV = poly3->V1;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly3->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly3->P2].Y;
  ListFillPoly[1].Pt_MapU = poly3->U2;
  ListFillPoly[1].Pt_MapV = poly3->V2;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly3->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly3->P3].Y;
  ListFillPoly[2].Pt_MapU = poly3->U3;
  ListFillPoly[2].Pt_MapV = poly3->V3;

  U32 textureInfo = PtrTextures[poly3->HandleText];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);

  U16 normal = ListLights[poly3->Normale] >> 8;

  return Fill_Poly(typePoly >> 2, normal, 3, ListFillPoly);
}

S32 Quad_Texture_Flat(U32 typePoly, void *poly)
{
  STRUC_POLY4_TEXTURE *poly4 = (STRUC_POLY4_TEXTURE *)poly;
  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[0].Pt_MapU = poly4->U1;
  ListFillPoly[0].Pt_MapV = poly4->V1;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[1].Pt_MapU = poly4->U2;
  ListFillPoly[1].Pt_MapV = poly4->V2;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[2].Pt_MapU = poly4->U3;
  ListFillPoly[2].Pt_MapV = poly4->V3;
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;
  ListFillPoly[3].Pt_MapU = poly4->U4;
  ListFillPoly[3].Pt_MapV = poly4->V4;

  U32 textureInfo = PtrTextures[poly4->HandleText];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);

  U16 normal = ListLights[poly4->Normale] >> 8;

  return Fill_Poly(typePoly >> 2, normal, 4, ListFillPoly);
}

// ***********************
// *** TEXTURE GOURAUD ***
// ***********************
S32 Triangle_Texture_Gouraud(U32 typePoly, void *poly)
{
  STRUC_POLY3_TEXTURE *poly3 = (STRUC_POLY3_TEXTURE *)poly;
  GetZO3(ListFillPoly, poly3->P1, poly3->P2, poly3->P3);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly3->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly3->P1].Y;
  ListFillPoly[0].Pt_MapU = poly3->U1;
  ListFillPoly[0].Pt_MapV = poly3->V1;
  ListFillPoly[0].Pt_Light = ListLights[poly3->P1];
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly3->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly3->P2].Y;
  ListFillPoly[1].Pt_MapU = poly3->U2;
  ListFillPoly[1].Pt_MapV = poly3->V2;
  ListFillPoly[1].Pt_Light = ListLights[poly3->P2];
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly3->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly3->P3].Y;
  ListFillPoly[2].Pt_MapU = poly3->U3;
  ListFillPoly[2].Pt_MapV = poly3->V3;
  ListFillPoly[2].Pt_Light = ListLights[poly3->P3];

  U32 textureInfo = PtrTextures[poly3->HandleText];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);

  return Fill_Poly(typePoly >> 2, ListLights[poly3->P3], 3, ListFillPoly);
}

// Verified
S32 Quad_Texture_Gouraud(U32 typePoly, void *poly)
{
  STRUC_POLY4_TEXTURE *poly4 = (STRUC_POLY4_TEXTURE *)poly;
  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[0].Pt_MapU = poly4->U1;
  ListFillPoly[0].Pt_MapV = poly4->V1;
  ListFillPoly[0].Pt_Light = ListLights[poly4->P1];
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[1].Pt_MapU = poly4->U2;
  ListFillPoly[1].Pt_MapV = poly4->V2;
  ListFillPoly[1].Pt_Light = ListLights[poly4->P2];
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[2].Pt_MapU = poly4->U3;
  ListFillPoly[2].Pt_MapV = poly4->V3;
  ListFillPoly[2].Pt_Light = ListLights[poly4->P3];
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;
  ListFillPoly[3].Pt_MapU = poly4->U4;
  ListFillPoly[3].Pt_MapV = poly4->V4;
  ListFillPoly[3].Pt_Light = ListLights[poly4->P4];

  U32 textureInfo = PtrTextures[poly4->HandleText];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);

  return Fill_Poly(typePoly >> 2, RepMask, 4, ListFillPoly);
}

// ***********************
// *** TEXTURE Z SOLID ***
// ***********************
S32 Triangle_TextureZ_Solid(U32 typePoly, void *poly)
{
  STRUC_POLY3_TEXTURE *poly3 = (STRUC_POLY3_TEXTURE *)poly;
  
  // Extract polygon type and vertex indices
  typePoly >>= 2;
  U16 p1 = poly3->P1;
  U16 p2 = poly3->P2;
  U16 p3 = poly3->P3;
  
  // Setup Z-buffer information for the 3 vertices
  GetZO3(ListFillPoly, p1, p2, p3);
  
  // Set screen coordinates for all 3 vertices
  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[p1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[p1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[p2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[p2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[p3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[p3].Y;
  
  // Extract texture handle from high word of P3 and setup texture info
  U32 textureHandle = (poly3->P3 >> 16) & ~3; // Clear bottom 2 bits
  U32 textureInfo = PtrTextures[textureHandle >> 2];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);
  
  // Set texture coordinates (U,V) for all 3 vertices
  ListFillPoly[0].Pt_MapU = poly3->U1;
  ListFillPoly[0].Pt_MapV = poly3->V1;
  ListFillPoly[1].Pt_MapU = poly3->U2;
  ListFillPoly[1].Pt_MapV = poly3->V2;
  ListFillPoly[2].Pt_MapU = poly3->U3;
  ListFillPoly[2].Pt_MapV = poly3->V3;
  
  // Calculate perspective correction values (W) for each vertex
  // W = W_NORM / (Z + PosZWr) where Z is the rotated Z coordinate
  S32 z1 = Obj_ListRotatedPoints[p1].Z;
  S32 z2 = Obj_ListRotatedPoints[p2].Z;
  S32 z3 = Obj_ListRotatedPoints[p3].Z;
  
  // Sign extend Z values from 16-bit to 32-bit
  z1 = (z1 << 16) >> 16;
  z2 = (z2 << 16) >> 16;
  z3 = (z3 << 16) >> 16;
  
  // Add world Z position offset
  z1 += PosZWr;
  z2 += PosZWr;
  z3 += PosZWr;
  
  // Calculate perspective correction factors
  ListFillPoly[0].Pt_W = W_NORM / z1;
  ListFillPoly[1].Pt_W = W_NORM / z2;
  ListFillPoly[2].Pt_W = W_NORM / z3;
  
  // Call polygon fill function with perspective-corrected texture mapping
  return Fill_Poly(typePoly, 0, 3, ListFillPoly);
}

S32 Quad_TextureZ_Solid(U32 typePoly, void *poly)
{
  STRUC_POLY4_TEXTURE *poly4 = (STRUC_POLY4_TEXTURE *)poly;
  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[0].Pt_MapU = poly4->U1;
  ListFillPoly[0].Pt_MapV = poly4->V1;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[1].Pt_MapU = poly4->U2;
  ListFillPoly[1].Pt_MapV = poly4->V2;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[2].Pt_MapU = poly4->U3;
  ListFillPoly[2].Pt_MapV = poly4->V3;
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;
  ListFillPoly[3].Pt_MapU = poly4->U4;
  ListFillPoly[3].Pt_MapV = poly4->V4;

  U32 textureInfo = PtrTextures[poly4->HandleText];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);

  ListFillPoly[0].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P1].Z + PosZWr);
  ListFillPoly[1].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P2].Z + PosZWr);
  ListFillPoly[2].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P3].Z + PosZWr);
  ListFillPoly[3].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P4].Z + PosZWr);

  return Fill_Poly(typePoly >> 2, 0, 4, ListFillPoly);
}

// **********************
// *** TEXTURE Z FLAT ***
// **********************
// Verified against original, but not tested
S32 Triangle_TextureZ_Flat(U32 typePoly, void *poly)
{
  STRUC_POLY3_TEXTURE *poly3 = (STRUC_POLY3_TEXTURE *)poly;
  GetZO3(ListFillPoly, poly3->P1, poly3->P2, poly3->P3);

  U16 intensity = ListLights[poly3->Normale] >> 8;

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly3->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly3->P1].Y;
  ListFillPoly[0].Pt_MapU = poly3->U1;
  ListFillPoly[0].Pt_MapV = poly3->V1;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly3->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly3->P2].Y;
  ListFillPoly[1].Pt_MapU = poly3->U2;
  ListFillPoly[1].Pt_MapV = poly3->V2;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly3->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly3->P3].Y;
  ListFillPoly[2].Pt_MapU = poly3->U3;
  ListFillPoly[2].Pt_MapV = poly3->V3;

  U32 textureInfo = PtrTextures[poly3->HandleText];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);

  ListFillPoly[0].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P1].Z + PosZWr);
  ListFillPoly[1].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P2].Z + PosZWr);
  ListFillPoly[2].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P3].Z + PosZWr);

  return Fill_Poly(typePoly >> 2, intensity, 3, ListFillPoly);
}

S32 Quad_TextureZ_Flat(U32 typePoly, void *poly)
{
  STRUC_POLY4_TEXTURE *poly4 = (STRUC_POLY4_TEXTURE *)poly;
  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);

  U16 intensity = ListLights[poly4->Normale] >> 8;

  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[0].Pt_MapU = poly4->U1;
  ListFillPoly[0].Pt_MapV = poly4->V1;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[1].Pt_MapU = poly4->U2;
  ListFillPoly[1].Pt_MapV = poly4->V2;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[2].Pt_MapU = poly4->U3;
  ListFillPoly[2].Pt_MapV = poly4->V3;
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;
  ListFillPoly[3].Pt_MapU = poly4->U4;
  ListFillPoly[3].Pt_MapV = poly4->V4;

  U32 textureInfo = PtrTextures[poly4->HandleText];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);

  ListFillPoly[0].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P1].Z + PosZWr);
  ListFillPoly[1].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P2].Z + PosZWr);
  ListFillPoly[2].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P3].Z + PosZWr);
  ListFillPoly[3].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P4].Z + PosZWr);

  return Fill_Poly(typePoly >> 2, intensity, 4, ListFillPoly);
}

// *************************
// *** TEXTURE Z GOURAUD ***
// *************************
// Converted from ASM - Textured triangle rendering with Z-buffer and Gouraud shading
S32 Triangle_TextureZ_Gouraud(U32 typePoly, void *poly)
{
  STRUC_POLY3_TEXTURE *poly3 = (STRUC_POLY3_TEXTURE *)poly;
  
  // Extract point indices from the polygon structure
  U16 p1 = poly3->P1;
  U16 p2 = poly3->P2; 
  U16 p3 = poly3->P3;
  
  // Setup Z-buffer for the triangle
  GetZO3(ListFillPoly, p1, p2, p3);
  
  // Set per-vertex lighting values from ListLights array (Gouraud shading)
  // This creates smooth lighting interpolation across the triangle surface
  ListFillPoly[0].Pt_Light = ListLights[p1];
  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[p1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[p1].Y;
  
  ListFillPoly[1].Pt_Light = ListLights[p2];
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[p2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[p2].Y;
  
  ListFillPoly[2].Pt_Light = ListLights[p3];
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[p3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[p3].Y;
  
  // Setup texture mapping data
  U32 textureIndex = poly3->HandleText & 0xFFFF;
  U32 textureInfo = PtrTextures[textureIndex];
  RepMask = textureInfo >> 16;  // Texture repeat mask
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);  // Texture data pointer
  
  // Set texture coordinates for each vertex
  ListFillPoly[0].Pt_MapU = poly3->U1;
  ListFillPoly[0].Pt_MapV = poly3->V1;
  ListFillPoly[1].Pt_MapU = poly3->U2;
  ListFillPoly[1].Pt_MapV = poly3->V2;
  ListFillPoly[2].Pt_MapU = poly3->U3;
  ListFillPoly[2].Pt_MapV = poly3->V3;
  
  // Calculate perspective correct W values (1/Z) for each vertex
  // This ensures proper perspective correction during texture mapping
  S32 z1 = Obj_ListRotatedPoints[p1].Z + PosZWr;
  S32 z2 = Obj_ListRotatedPoints[p2].Z + PosZWr;
  S32 z3 = Obj_ListRotatedPoints[p3].Z + PosZWr;
  
  ListFillPoly[0].Pt_W = W_NORM / z1;
  ListFillPoly[1].Pt_W = W_NORM / z2;
  ListFillPoly[2].Pt_W = W_NORM / z3;
  
  return Fill_Poly(typePoly >> 2, 0, 3, ListFillPoly);
}

S32 Quad_TextureZ_Gouraud(U32 typePoly, void *poly)
{
  STRUC_POLY4_TEXTURE *poly4 = (STRUC_POLY4_TEXTURE *)poly;
  
  // Extract point indices from the polygon structure
  U16 p1 = poly4->P1;
  U16 p2 = poly4->P2; 
  U16 p3 = poly4->P3;
  U16 p4 = poly4->P4;
  
  // Setup Z-buffer for the quad
  GetZO4(ListFillPoly, p1, p2, p3, p4);
  
  // Set per-vertex lighting values from ListLights array (Gouraud shading)
  ListFillPoly[0].Pt_Light = ListLights[p1];
  ListFillPoly[1].Pt_Light = ListLights[p2];
  ListFillPoly[2].Pt_Light = ListLights[p3];
  ListFillPoly[3].Pt_Light = ListLights[p4];
  
  // Set screen coordinates
  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[p1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[p1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[p2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[p2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[p3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[p3].Y;
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[p4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[p4].Y;
  
  // Setup texture mapping data
  U32 textureInfo = PtrTextures[poly4->HandleText];
  RepMask = textureInfo >> 16;
  PtrMap = ObjPtrMap + (textureInfo & 0xFFFF);
  
  // Set texture coordinates for each vertex
  ListFillPoly[0].Pt_MapU = poly4->U1;
  ListFillPoly[0].Pt_MapV = poly4->V1;
  ListFillPoly[1].Pt_MapU = poly4->U2;
  ListFillPoly[1].Pt_MapV = poly4->V2;
  ListFillPoly[2].Pt_MapU = poly4->U3;
  ListFillPoly[2].Pt_MapV = poly4->V3;
  ListFillPoly[3].Pt_MapU = poly4->U4;
  ListFillPoly[3].Pt_MapV = poly4->V4;
  
  // Calculate perspective correct W values (1/Z) for each vertex
  ListFillPoly[0].Pt_W = W_NORM / (Obj_ListRotatedPoints[p1].Z + PosZWr);
  ListFillPoly[1].Pt_W = W_NORM / (Obj_ListRotatedPoints[p2].Z + PosZWr);
  ListFillPoly[2].Pt_W = W_NORM / (Obj_ListRotatedPoints[p3].Z + PosZWr);
  ListFillPoly[3].Pt_W = W_NORM / (Obj_ListRotatedPoints[p4].Z + PosZWr);
  
  return Fill_Poly(typePoly >> 2, 0, 4, ListFillPoly);
}

// *************************
// *** TEXTURE ENV SOLID ***
// *************************
S32 Triangle_EnvTexture_Solid(U32 typePoly, void *poly)
{
  STRUC_POLY3_ENV *poly3 = (STRUC_POLY3_ENV *)poly;
  GetZO3(ListFillPoly, poly3->P1, poly3->P2, poly3->P3);
  
  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly3->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly3->P1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly3->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly3->P2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly3->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly3->P3].Y;
  
  // Environment mapping uses surface normals for texture coordinates
  U32 envInfo = PtrTextures[poly3->HandleEnv];
  RepMask = envInfo >> 16;
  PtrMap = ObjPtrMap + (envInfo & 0xFFFF);
  
  // Calculate environment texture coordinates based on normals
  // This is simplified - real env mapping would use transformed normals
  ListFillPoly[0].Pt_MapU = 0;
  ListFillPoly[0].Pt_MapV = 0;
  ListFillPoly[1].Pt_MapU = 0;
  ListFillPoly[1].Pt_MapV = 0;
  ListFillPoly[2].Pt_MapU = 0;
  ListFillPoly[2].Pt_MapV = 0;
  
  ListFillPoly[0].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P1].Z + PosZWr);
  ListFillPoly[1].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P2].Z + PosZWr);
  ListFillPoly[2].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P3].Z + PosZWr);
  
  return Fill_Poly(typePoly >> 2, poly3->Couleur & 0xFF, 3, ListFillPoly);
}

S32 Quad_EnvTexture_Solid(U32 typePoly, void *poly)
{
  STRUC_POLY4_ENV *poly4 = (STRUC_POLY4_ENV *)poly;
  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);
  
  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;
  
  U32 envInfo = PtrTextures[poly4->HandleEnv];
  RepMask = envInfo >> 16;
  PtrMap = ObjPtrMap + (envInfo & 0xFFFF);
  
  // Environment texture coordinates (simplified)
  for(int i = 0; i < 4; i++) {
    ListFillPoly[i].Pt_MapU = 0;
    ListFillPoly[i].Pt_MapV = 0;
  }
  
  ListFillPoly[0].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P1].Z + PosZWr);
  ListFillPoly[1].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P2].Z + PosZWr);
  ListFillPoly[2].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P3].Z + PosZWr);
  ListFillPoly[3].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P4].Z + PosZWr);
  
  return Fill_Poly(typePoly >> 2, poly4->Couleur & 0xFF, 4, ListFillPoly);
}

// ************************
// *** TEXTURE ENV FLAT ***
// ************************
S32 Triangle_EnvTexture_Flat(U32 typePoly, void *poly)
{
  STRUC_POLY3_ENV *poly3 = (STRUC_POLY3_ENV *)poly;
  GetZO3(ListFillPoly, poly3->P1, poly3->P2, poly3->P3);
  
  U16 intensity = ListLights[poly3->Normale] >> 8;
  
  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly3->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly3->P1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly3->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly3->P2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly3->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly3->P3].Y;
  
  U32 envInfo = PtrTextures[poly3->HandleEnv];
  RepMask = envInfo >> 16;
  PtrMap = ObjPtrMap + (envInfo & 0xFFFF);
  
  // Environment texture coordinates (simplified)
  for(int i = 0; i < 3; i++) {
    ListFillPoly[i].Pt_MapU = 0;
    ListFillPoly[i].Pt_MapV = 0;
  }
  
  ListFillPoly[0].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P1].Z + PosZWr);
  ListFillPoly[1].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P2].Z + PosZWr);
  ListFillPoly[2].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P3].Z + PosZWr);
  
  return Fill_Poly(typePoly >> 2, intensity, 3, ListFillPoly);
}

S32 Quad_EnvTexture_Flat(U32 typePoly, void *poly)
{
  STRUC_POLY4_ENV *poly4 = (STRUC_POLY4_ENV *)poly;
  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);
  
  U16 intensity = ListLights[poly4->Normale] >> 8;
  
  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;
  
  U32 envInfo = PtrTextures[poly4->HandleEnv];
  RepMask = envInfo >> 16;
  PtrMap = ObjPtrMap + (envInfo & 0xFFFF);
  
  // Environment texture coordinates (simplified)
  for(int i = 0; i < 4; i++) {
    ListFillPoly[i].Pt_MapU = 0;
    ListFillPoly[i].Pt_MapV = 0;
  }
  
  ListFillPoly[0].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P1].Z + PosZWr);
  ListFillPoly[1].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P2].Z + PosZWr);
  ListFillPoly[2].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P3].Z + PosZWr);
  ListFillPoly[3].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P4].Z + PosZWr);
  
  return Fill_Poly(typePoly >> 2, intensity, 4, ListFillPoly);
}

// ***************************
// *** TEXTURE ENV GOURAUD ***
// ***************************
S32 Triangle_EnvTexture_Gouraud(U32 typePoly, void *poly)
{
  STRUC_POLY3_ENV *poly3 = (STRUC_POLY3_ENV *)poly;
  GetZO3(ListFillPoly, poly3->P1, poly3->P2, poly3->P3);
  
  // Set per-vertex lighting values (Gouraud shading)
  ListFillPoly[0].Pt_Light = ListLights[poly3->P1];
  ListFillPoly[1].Pt_Light = ListLights[poly3->P2];
  ListFillPoly[2].Pt_Light = ListLights[poly3->P3];
  
  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly3->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly3->P1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly3->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly3->P2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly3->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly3->P3].Y;
  
  U32 envInfo = PtrTextures[poly3->HandleEnv];
  RepMask = envInfo >> 16;
  PtrMap = ObjPtrMap + (envInfo & 0xFFFF);
  
  // Environment texture coordinates (simplified)
  for(int i = 0; i < 3; i++) {
    ListFillPoly[i].Pt_MapU = 0;
    ListFillPoly[i].Pt_MapV = 0;
  }
  
  ListFillPoly[0].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P1].Z + PosZWr);
  ListFillPoly[1].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P2].Z + PosZWr);
  ListFillPoly[2].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly3->P3].Z + PosZWr);
  
  return Fill_Poly(typePoly >> 2, 0, 3, ListFillPoly);
}

S32 Quad_EnvTexture_Gouraud(U32 typePoly, void *poly)
{
  STRUC_POLY4_ENV *poly4 = (STRUC_POLY4_ENV *)poly;
  GetZO4(ListFillPoly, poly4->P1, poly4->P2, poly4->P3, poly4->P4);
  
  // Set per-vertex lighting values (Gouraud shading)
  ListFillPoly[0].Pt_Light = ListLights[poly4->P1];
  ListFillPoly[1].Pt_Light = ListLights[poly4->P2];
  ListFillPoly[2].Pt_Light = ListLights[poly4->P3];
  ListFillPoly[3].Pt_Light = ListLights[poly4->P4];
  
  ListFillPoly[0].Pt_XE = Obj_ListProjectedPoints[poly4->P1].X;
  ListFillPoly[0].Pt_YE = Obj_ListProjectedPoints[poly4->P1].Y;
  ListFillPoly[1].Pt_XE = Obj_ListProjectedPoints[poly4->P2].X;
  ListFillPoly[1].Pt_YE = Obj_ListProjectedPoints[poly4->P2].Y;
  ListFillPoly[2].Pt_XE = Obj_ListProjectedPoints[poly4->P3].X;
  ListFillPoly[2].Pt_YE = Obj_ListProjectedPoints[poly4->P3].Y;
  ListFillPoly[3].Pt_XE = Obj_ListProjectedPoints[poly4->P4].X;
  ListFillPoly[3].Pt_YE = Obj_ListProjectedPoints[poly4->P4].Y;
  
  U32 envInfo = PtrTextures[poly4->HandleEnv];
  RepMask = envInfo >> 16;
  PtrMap = ObjPtrMap + (envInfo & 0xFFFF);
  
  // Environment texture coordinates (simplified)
  for(int i = 0; i < 4; i++) {
    ListFillPoly[i].Pt_MapU = 0;
    ListFillPoly[i].Pt_MapV = 0;
  }
  
  ListFillPoly[0].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P1].Z + PosZWr);
  ListFillPoly[1].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P2].Z + PosZWr);
  ListFillPoly[2].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P3].Z + PosZWr);
  ListFillPoly[3].Pt_W = W_NORM / (Obj_ListRotatedPoints[poly4->P4].Z + PosZWr);
  
  return Fill_Poly(typePoly >> 2, 0, 4, ListFillPoly);
}

// ************
// *** LINE ***
// ************
S32 Line(U32 typePoly, void *poly)
{
  T_OBJ_LINE *line = (T_OBJ_LINE *)poly;

  TYPE_PT p1 = Obj_ListProjectedPoints[line->P1];
  TYPE_PT p2 = Obj_ListProjectedPoints[line->P2];

  S32 z1 = 0;
  S32 z2 = 0;

  if (Fill_Flag_ZBuffer && TypeProj != TYPE_3D)
  {
    T_OBJ_POINT rotatedP1 = Obj_ListRotatedPoints[line->P1];
    T_OBJ_POINT rotatedP2 = Obj_ListRotatedPoints[line->P2];

    // For some reason, z1 is calculated with p2 and z2 with p1
    z1 = Fill_ZBuffer_Factor * (~rotatedP1.Z - (PosZWr - 1)) >> 16;
    z2 = Fill_ZBuffer_Factor * (~rotatedP2.Z - (PosZWr - 1)) >> 16;
  }

  if ((p1.X == 0x8000 && p1.Y == 0x8000) || (p2.X == 0x8000 && p2.Y == 0x8000))
  {
    return 0;
  }

  Line_A(p1.X, p1.Y, p2.X, p2.Y, line->Coul, z1, z2);
  return 0;
}

// **************
// *** SPHERE ***
// **************
S32 Sphere(U32 typePoly, void *poly)
{
  T_OBJ_SPHERE *sphere = (T_OBJ_SPHERE *)poly;

  S32 radius = sphere->Rayon;

  if (TypeProj != TYPE_3D)
  {
    radius += radius + (radius << 5);
    radius >>= 9;
    radius = ~radius;
  }
  else
  {
    // @@Radius_Proj:
    S16 z = Obj_ListRotatedPoints[sphere->P1].Z;
    radius *= LFactorX;
    radius /= z + PosZWr;
  }

  // @@Return_Radius:
  TYPE_PT p1 = Obj_ListProjectedPoints[sphere->P1];

  radius = ((radius ^ -1) + 1);
  S32 topY = p1.Y - radius;
  if (topY < ScreenYMin)
  {
    ScreenYMin = topY;
  }

  S32 bottomY = topY + 2*radius;
  if (bottomY > ScreenYMax)
  {
    ScreenYMax = bottomY;
  }

  S32 leftX = p1.X - radius;
  if (leftX < ScreenXMin)
  {
    ScreenXMin = leftX;
  }

  S32 rightX = leftX + 2*radius;
  if (rightX > ScreenXMax)
  {
    ScreenXMax = rightX;
  }

  S32 z = 0;

  if (Fill_Flag_ZBuffer && TypeProj == TYPE_3D)
  {
    T_OBJ_POINT rotatedP1 = Obj_ListRotatedPoints[sphere->P1];
    z = (Fill_ZBuffer_Factor * (~rotatedP1.Z - PosZWr)) >> 16;
  }
  // @@NoZBufSphere:
  Fill_Sphere(sphere->Type, sphere->Coul, p1.X, p1.Y, radius, z);

  return 0;
}

// *********************
// *** SPHERE TRANSP ***
// *********************
S32 Sphere_Transp(U32 typePoly, void *poly)
{
  T_OBJ_SPHERE *sphere = (T_OBJ_SPHERE *)poly;
  
  S32 radius = sphere->Rayon;
  
  if (TypeProj == TYPE_3D)
  {
    // @@Radius_3D:
    radius = (radius * Fill_ZBuffer_Factor) >> 16;
  }
  else
  {
    // @@Radius_Proj:
    S16 z = Obj_ListRotatedPoints[sphere->P1].Z;
    radius *= LFactorX;
    radius /= z + PosZWr;
  }

  TYPE_PT p1 = Obj_ListProjectedPoints[sphere->P1];

  radius = ((radius ^ -1) + 1);
  S32 topY = p1.Y - radius;
  if (topY < ScreenYMin)
  {
    ScreenYMin = topY;
  }

  S32 bottomY = topY + 2*radius;
  if (bottomY > ScreenYMax)
  {
    ScreenYMax = bottomY;
  }

  S32 leftX = p1.X - radius;
  if (leftX < ScreenXMin)
  {
    ScreenXMin = leftX;
  }

  S32 rightX = leftX + 2*radius;
  if (rightX > ScreenXMax)
  {
    ScreenXMax = rightX;
  }

  S32 z = 0;
  if (Fill_Flag_ZBuffer && TypeProj == TYPE_3D)
  {
    T_OBJ_POINT rotatedP1 = Obj_ListRotatedPoints[sphere->P1];
    z = (Fill_ZBuffer_Factor * (~rotatedP1.Z - PosZWr)) >> 16;
  }
  
  Fill_Sphere(2, sphere->Coul, p1.X, p1.Y, radius, z);
  return 0;
}

bool TestVisible(STRUC_POLY3_ENV *poly)
{
  TYPE_PT projectedP1 = Obj_ListProjectedPoints[poly->P1];
  TYPE_PT projectedP2 = Obj_ListProjectedPoints[poly->P2];
  TYPE_PT projectedP3 = Obj_ListProjectedPoints[poly->P3];

  if ((projectedP1.X == 0x8000 && projectedP1.Y == 0x8000) || (projectedP2.X == 0x8000 && projectedP2.Y == 0x8000) || (projectedP3.X == 0x8000 && projectedP3.Y == 0x8000))
  {
    return false;
  }

  U32 value = (projectedP2.X - projectedP1.X) * (projectedP1.Y - projectedP3.Y) - (projectedP2.Y - projectedP1.Y) * (projectedP1.X - projectedP3.X);
  return (value & 0x80000000) == 0;
}

void QuickSort(S32 *beginning, S32 *end)
{
	if (end <= beginning)
	{
		return;
	}

	S32 *left = beginning;
	S32 *next = left + 1;
	S32 pivot = *beginning;

	do
	{
		// @Next:
		S32 nextValue = *next;
		next++;

		if (pivot >= nextValue)
		{
			*left = nextValue;
			*(next - 1) = *(left + 1);
			*(left + 1) = pivot;
			left++;
		}

		// @Ordre:
	} while (next <= end);

	QuickSort(beginning, left);
	QuickSort(left + 1, end);
}

void QuickSortInv(S32 *beginning, S32 *end)
{
	if (end <= beginning)
	{
		return;
	}

	S32 *left = beginning;
	S32 *next = left + 1;
	S32 pivot = *beginning;

	do
	{
		// @Next:
		S32 nextValue = *next;
		next++;

		if (pivot <= nextValue)
		{
			*left = nextValue;
			*(next - 1) = *(left + 1);
			*(left + 1) = pivot;
			left += 1;
		}

		// @Ordre:
	} while (next <= end);

	QuickSortInv(beginning, left);
	QuickSortInv(next + 1, end);
}

S16 TestZFar4(STRUC_POLY3_ENV *poly)
{
  S16 zFar = Obj_ListRotatedPoints[poly->P1].Z;

  if (zFar > Obj_ListRotatedPoints[poly->P2].Z)
  {
    zFar = Obj_ListRotatedPoints[poly->P2].Z;
  }

  if (zFar > Obj_ListRotatedPoints[poly->P3].Z)
  {
    zFar = Obj_ListRotatedPoints[poly->P3].Z;
  }

  U16 poly4P4 = ((STRUC_POLY4_LIGHT *)poly)->P4;

  if (zFar > Obj_ListRotatedPoints[poly4P4].Z)
  {
    zFar = Obj_ListRotatedPoints[poly4P4].Z;
  }

  return zFar;
}

S16 TestZFar3(STRUC_POLY3_ENV *poly)
{
  S16 zFar = Obj_ListRotatedPoints[poly->P1].Z;

  if (zFar > Obj_ListRotatedPoints[poly->P2].Z)
  {
    zFar = Obj_ListRotatedPoints[poly->P2].Z;
  }

  if (zFar > Obj_ListRotatedPoints[poly->P3].Z)
  {
    zFar = Obj_ListRotatedPoints[poly->P3].Z;
  }

  return zFar;
}

void InsertTriangle(U8 type, T_POLY_HEADER *header, void **poly, U32 *polyOffset)
{
  for (int i = 0; i < header->NbPoly; i++)
  {
    // @@Do_Triangle:
    STRUC_POLY3_ENV *poly3 = (STRUC_POLY3_ENV *)*poly;

    if (TestVisible(poly3))
    {
      U32 zFar = TestZFar3(poly3);

      // | ZFar | Poly Number |
      Off_Sort[0] = (zFar << 16) | (Nb_Sort & 0xFFFF);

      U32 polyType = header->TypePoly << 26;
      if (type == 2)
      {
        polyType |= 0b10 << 24; // Bit environment
      }
        // | PolyType (8 bit) | Poly Offset (24 bit) |
      Off_Sort[MAX_NB_POLYS] = polyType | (*polyOffset & 0xFFFFFF);

      Nb_Sort++;
      Off_Sort++;
    }

    // @@Next_Triangle:
    U32 size = 0;
    if (type == 0)
    {
      size = sizeof(STRUC_POLY3_LIGHT);
    }
    else if (type == 1)
    {
      size = sizeof(STRUC_POLY3_TEXTURE);
    }
    else
    {
      size = sizeof(STRUC_POLY3_ENV);
    }

    *poly = ((U8 *)*poly) + size;
    *polyOffset += size;
  }
}

void InsertQuad(U8 type, T_POLY_HEADER *header, void **poly, U32 *polyOffset)
{
  for (int i = 0; i < header->NbPoly; i++)
  {
    // @@Do_Quad:
    STRUC_POLY3_ENV *poly3 = (STRUC_POLY3_ENV *)*poly;

    if (TestVisible(poly3))
    {
      STRUC_POLY4_LIGHT *poly4 = (STRUC_POLY4_LIGHT *)*poly;

      S16 zFar = TestZFar4(poly3);

      TYPE_PT p4 = Obj_ListProjectedPoints[poly4->P4];
      if (p4.X != 0x8000 || p4.Y != 0x8000)
      {
        U32 polyType = (header->TypePoly << 26);
        if (type == 2)
        {
          polyType |= (0b11 << 24); // Bit quad+environment
        }
        else
        {
          polyType |= (0b01 << 24); // Bit quad
        }

        // | ZFar | Poly Number |
        Off_Sort[0] = (zFar << 16) | (Nb_Sort & 0xFFFF);

        // | PolyType (8 bit) | Poly Offset (24 bit) |
        Off_Sort[MAX_NB_POLYS] = polyType | (*polyOffset & 0xFFFFFF);

        Nb_Sort++;
        Off_Sort++;
      }
    }
    // @@Next_Quad:
    U32 size = 0;
    if (type == 0)
    {
      size = sizeof(STRUC_POLY4_LIGHT);
    }
    else if (type == 1)
    {
      size = sizeof(STRUC_POLY4_TEXTURE);
    }
    else
    {
      size = sizeof(STRUC_POLY4_ENV);
    }

    *poly = ((U8 *)*poly) + size;
    *polyOffset += size;
  }
}

S32 EntityCreation(T_BODY_HEADER *body)
{
  // *****************
  // *** POLYGONES ***
  // *****************

  Off_Sort = ListSort;
  Nb_Sort = 0;

  void *lines = (void *)((U8 *)body + body->OffLines);
  void *currentPoly = (void *)((U8 *)body + body->OffPolys);
  U32 bodyOffset = body->OffPolys;

  // @@Loop_Polys:
  while (currentPoly != lines)
  {
    T_POLY_HEADER *header = (T_POLY_HEADER *)currentPoly;
    U32 mask = 0;
    memcpy(&mask, header, sizeof(U32));
    U8 typePoly = header->TypePoly & 0xFF;
    currentPoly = (void *)((U8 *)currentPoly + sizeof(T_POLY_HEADER));
    bodyOffset += sizeof(T_POLY_HEADER);

    // *** Type dispatch ***
    if (mask & MASK_QUADRILATERE)
    {
      // Dispatch a quad
      if (mask & MASK_ENVIRONMENT)
      {
        InsertQuad(2, header, &currentPoly, &bodyOffset);
      }
      else if (typePoly <= POLY_DITHER_TABLE)
      {
        InsertQuad(0, header, &currentPoly, &bodyOffset);
      }
      else
      {
        InsertQuad(1, header, &currentPoly, &bodyOffset);
      }
    }
    // Dispatch a triangle
    else if (mask & MASK_ENVIRONMENT)
    {
      InsertTriangle(2, header, &currentPoly, &bodyOffset);
    }
    else if (typePoly <= POLY_DITHER_TABLE)
    {
      InsertTriangle(0, header, &currentPoly, &bodyOffset);
    }
    else
    {
      InsertTriangle(1, header, &currentPoly, &bodyOffset);
    }
  }

  // *************
  // *** LINES ***
  // *************
  // @@Do_Lines:
  if (lBody->NbLines != 0)
  {
    T_OBJ_LINE *line = (T_OBJ_LINE *)lines;

    for (int i = 0; i < body->NbLines; ++i)
    {
      // @@Loop_Lines:
      T_OBJ_POINT p1 = Obj_ListRotatedPoints[line->P1];
      T_OBJ_POINT p2 = Obj_ListRotatedPoints[line->P2];
      S16 minZ = p1.Z;

      if (p1.Z > p2.Z)
      {
        minZ = p2.Z;
      }

      // @@Line_Sup:
      // | ZFar | Poly Number |
      Off_Sort[0] = (minZ << 16) | Nb_Sort;
      U8 polyType = line->Type;
      polyType += MAX_POLYS_TYPE * 4; // Get correct pos for a line
      // | PolyType (8 bit) | Poly Offset (24 bit) |
      Off_Sort[MAX_NB_POLYS] = (polyType << 24) | (bodyOffset & 0xFFFFFF);

      Off_Sort++;
      Nb_Sort++;
      line++;
      bodyOffset += sizeof(T_OBJ_LINE);
    }
  }

  // ***************
  // *** SPHERES ***
  // ***************
  // @@Do_Spheres:
  if (lBody->NbSpheres != 0)
  {
    T_OBJ_SPHERE *sphere = (T_OBJ_SPHERE *)((U8 *)lBody + lBody->OffSpheres);

    for (int i = 0; i < body->NbSpheres; ++i)
    {
      // @@Loop_Spheres:
      S16 zFar = Obj_ListRotatedPoints[sphere->P1].Z;
      // | ZFar | Poly Number |
      Off_Sort[0] = (zFar << 16) | Nb_Sort;

      U8 polyType = sphere->Type + MAX_POLYS_TYPE * 4 + MAX_LINES_TYPE;
      // | PolyType (8 bit) | Poly Offset (24 bit) |
      Off_Sort[MAX_NB_POLYS] = (polyType << 24) | (bodyOffset & 0xFFFFFF);

      Off_Sort++;
      Nb_Sort++;
      sphere++;
      bodyOffset += sizeof(T_OBJ_SPHERE);
    }
  }

  // ┌─────────────────────────────────────────────────────────────────┐
  // │                                                                 │
  // │                      ENTITIES SORTING                           │
  // │                                                                 │
  // └─────────────────────────────────────────────────────────────────┘

	// @@Do_Sort:
	// Anything to do ?
	if (Off_Sort == ListSort)
	{
		return 0;
	}

	if (!(lBody->Info & MASK_OBJECT_NO_SORT))
	{
		if (!Fill_Flag_ZBuffer
				|| (lBody->Info & MASK_OBJECT_TRANSPARENT)
				|| Fill_Flag_NZW)
		{
			// @@SortNormal:
			QuickSort(ListSort, Off_Sort - 1);
		}
		else
		{
			// @@SortZBuffer:
			QuickSortInv(ListSort, Off_Sort - 1);
		}
	}
	// @@NoSort:

  // ┌─────────────────────────────────────────────────────────────────┐
  // │                                                                 │
  // │                             DISPLAY                             │
  // │                                                                 │
  // └─────────────────────────────────────────────────────────────────┘

  PtrTextures = (U32 *)((U8 *)lBody + lBody->OffTextures);
  S32 *list = ListSort;
  S32 *polyInfo = ListSort + MAX_NB_POLYS;
  for (int i = 0; i < Nb_Sort; i++)
  {
    // @@ Do_Display:
    U16 number = *list & 0xFFFF;
    S32 info = polyInfo[number];
    list++;

    U8 type = info >> 24;
    S32 offset = info & 0xFFFFFF;

    void *poly = (void *)((U8 *)lBody + offset);
    Poly_Jump[type](type, poly);
    // @@Return_Display:
  }

  return 1;
}

S32 ObjectDisplay(T_OBJ_3D *obj)
{
	// Must we change the texture pointer ?
	if (obj->Texture != NULL && obj->Texture != (void *)-1)
	{
		ObjPtrMap = (U8 *)obj->Texture;
	}

	// Init 2D box
	ScreenXMin = 0x7FFFFFFF;
	ScreenYMin = 0x7FFFFFFF;

	ScreenXMax = 0x80000000;
	ScreenYMax = 0x80000000;

	S32 x = obj->X;
	S32 y = obj->Y;
	S32 z = obj->Z;

	if (TypeProj == TYPE_3D)
	{
		LongRotatePoint(&MatriceWorld, x, y, z);

		x = X0;
		y = Y0;
		z = Z0;
	}

	x -= CameraXr;
	y -= CameraYr;
	z -= CameraZr;

	// Object world position
	PosXWr = x;
	PosYWr = y;
	PosZWr = z;

	// Get either body's number or body's ptr
	void *body = obj->Body.Ptr;

	// Convert it to a ptr
	if (TransFctBody != NULL)
	{
		body = TransFctBody(obj->Body.Num);
	}

// ┌─────────────────────────────────────────────────────────────────┐
// │                                                                 │
// │                          GEOMETRY                               │
// │                                                                 │
// └─────────────────────────────────────────────────────────────────┘

	lBody = (T_BODY_HEADER *)body;
	ListGroupAnim = obj->CurrentFrame - 1;
	PtrPoints = (TYPE_VT16 *)((U8 *)lBody + lBody->OffPoints);
	PtrNormales = (TYPE_VT16 *)((U8 *)lBody + lBody->OffNormales);
	PtrNormFaces = (TYPE_VT16 *)((U8 *)lBody + lBody->OffNormFaces);
	PtrRotPoints = (TYPE_VT16 *)Obj_ListRotatedPoints;
	PtrListLights = ListLights;

	// Skip vertex light
	// to calculate the offset of face-lights
	PtrListLightsFaces = ListLights + lBody->NbPoints * 2;
	NbGroupes = lBody->NbGroupes;

	PtrGroupes = (T_OBJ_GROUPE *)((U8 *)lBody + lBody->OffGroupes);
	Current_Matrix = TabMat;

	TransX = 0;
	TransY = 0;
	TransZ = 0;

	RotateMatrix(Current_Matrix, &MatriceWorld,
							obj->Alpha, obj->Beta, obj->Gamma);

	InitMatrixTrans(Current_Matrix, TransX, TransY, TransZ);

	// *****************
	// *** TRANSFORM ***
	// *****************

	U32 groupeNbPts = PtrGroupes->NbPts;
	U32 groupeNbNorm = PtrGroupes->NbNorm;

	RotTransList(Current_Matrix, PtrRotPoints, PtrPoints,
							groupeNbPts);

	PtrRotPoints = (TYPE_VT16 *)((U8 *)PtrRotPoints + (groupeNbPts << 3));
	PtrPoints = (TYPE_VT16 *)((U8 *)PtrPoints + (groupeNbPts << 3));

	// *****************
	// *** LIGHTING  ***
	// *****************

	// *** Calculate the intensities ***

	LightList(Current_Matrix, PtrListLights,
						PtrNormales, groupeNbPts);

	PtrNormales = (TYPE_VT16 *)((U8 *)PtrNormales + groupeNbPts * 8);
	PtrListLights = (PTR_U16)((U8 *)PtrListLights + groupeNbPts * 2);

	if (groupeNbNorm != 0)
	{
		LightList(Current_Matrix, PtrListLightsFaces,
							PtrNormFaces, groupeNbNorm);

		PtrNormFaces = (TYPE_VT16 *)((U8 *)PtrNormFaces + groupeNbNorm * 8);
		PtrListLightsFaces = (PTR_U16)((U8 *)PtrListLightsFaces + groupeNbNorm * 2);
	}

	while (--NbGroupes > 0)
	{
		PtrGroupes = PtrGroupes + 1;
		ListGroupAnim = ListGroupAnim + 1;

		Current_Matrix = Current_Matrix + 1;

		U16 orgGroupe = PtrGroupes->OrgGroupe;
		U16 orgPoint = PtrGroupes->OrgPoint;

		T_OBJ_POINT *point = &Obj_ListRotatedPoints[orgPoint];
		TransX = point->X;
		TransY = point->Y;
		TransZ = point->Z;

		TYPE_MAT *mat = &TabMat[orgGroupe];

		if (ListGroupAnim->Type & TYPE_TRANSLATE)
		{
			RotatePoint(mat, ListGroupAnim->Alpha,
									ListGroupAnim->Beta, ListGroupAnim->Gamma);

			CopyMatrix(Current_Matrix, mat);

			InitMatrixTrans(Current_Matrix, X0 + TransX,
											Y0 + TransY, Z0 + TransZ);
		}
		else
		{
			RotateMatrix(Current_Matrix, mat, ListGroupAnim->Alpha,
									ListGroupAnim->Beta, ListGroupAnim->Gamma);

			InitMatrixTrans(Current_Matrix, TransX, TransY, TransZ);
		}

		// *****************
		// *** TRANSFORM ***
		// *****************

		U32 groupeNbPts = PtrGroupes->NbPts;
		U32 groupeNbNorm = PtrGroupes->NbNorm;

		RotTransList(Current_Matrix, PtrRotPoints, PtrPoints,
								 groupeNbPts);

		PtrRotPoints = (TYPE_VT16 *)((U8 *)PtrRotPoints + (groupeNbPts << 3));
		PtrPoints = (TYPE_VT16 *)((U8 *)PtrPoints + (groupeNbPts << 3));

		// *****************
		// *** LIGHTING  ***
		// *****************

		// *** Calculate the intensities ***

		LightList(Current_Matrix, PtrListLights,
							PtrNormales, groupeNbPts);

		PtrNormales = (TYPE_VT16 *)((U8 *)PtrNormales + groupeNbPts * 8);
		PtrListLights = (PTR_U16)((U8 *)PtrListLights + groupeNbPts * 2);

		if (groupeNbNorm != 0)
		{
			LightList(Current_Matrix, PtrListLightsFaces,
								PtrNormFaces, groupeNbNorm);

			PtrNormFaces = (TYPE_VT16 *)((U8 *)PtrNormFaces + groupeNbNorm * 8);
			PtrListLightsFaces = (PTR_U16)((U8 *)PtrListLightsFaces + groupeNbNorm * 2);
		}
	}

	// ******************
	// *** PROJECTION ***
	// ******************

	ProjectList(Obj_ListProjectedPoints, (TYPE_VT16 *)Obj_ListRotatedPoints,
							lBody->NbPoints, -PosXWr, -PosYWr, -PosZWr);

	// Is the object visible?
	if (ScreenXMin > ClipXMax || ScreenYMin > ClipYMax ||
		ScreenXMax < ClipXMin || ScreenYMax < ClipYMin)
	{
		return 0;
	}

	return EntityCreation(lBody);
}

S32 Return_BodyDisplay(TYPE_MAT *srcMatrix, TYPE_MAT *dstMatrix, S32 alpha, S32 beta, S32 gamma, void *obj)
{
  RotateMatrix(dstMatrix, srcMatrix, alpha, beta, gamma);

  // **************
  // *** POINTS ***
  // **************
  T_BODY_HEADER *body = (T_BODY_HEADER *)obj;
  TYPE_VT16 *bodyPoints = (TYPE_VT16 *)((U8 *)body + body->OffPoints);
  RotTransList(TabMat, (TYPE_VT16 *)Obj_ListRotatedPoints, bodyPoints, body->NbPoints);

  ProjectList(Obj_ListProjectedPoints, (TYPE_VT16 *)Obj_ListRotatedPoints,
            lBody->NbPoints, -PosXWr, -PosYWr, -PosZWr);

  // Is the object visible?
  if (ScreenXMin > ClipXMax || ScreenYMin > ClipYMax ||
      ScreenXMax < ClipXMin || ScreenYMax < ClipYMin)
  {
    return 0;
  }

  // *****************
	// *** LIGHTING  ***
	// *****************

	// *** Calculate the intensities ***

  TYPE_VT16 *srcLights = (TYPE_VT16 *)((U8 *)body + body->OffNormales);
	LightList(TabMat, ListLights, srcLights, body->NbNormales);

  srcLights = (TYPE_VT16 *)((U8 *)body + body->OffNormFaces);
  LightList(TabMat, ListLights + body->NbNormales,
            srcLights, body->NbNormFaces);

	return EntityCreation(lBody);
}

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ BodyDisplay: Affiche un objet non animé                                    ║
// ╟────────────────────────────────────────────────────────────────────────────╢
// ║ Appel : (EAX, EBX, ECX) = (X, Y, Z)                                        ║
// ║         (EDX, ESI, EDI) = (Alpha, Beta, Gamma)                             ║
// ║         Stack           = Off_Body                                         ║
// ║                                                                            ║
// ║                                                                            ║
// ╚════════════════════════════════════════════════════════════════════════════╝
S32 BodyDisplay(S32 x, S32 y, S32 z, S32 alpha, S32 beta, S32 gamma,
                       void *obj)
{
  lBody = (T_BODY_HEADER *)obj;
  // Init 2D box
  ScreenXMin = 0x7FFFFFFF;
  ScreenYMin = 0x7FFFFFFF;
  ScreenXMax = 0x80000000;
  ScreenYMax = 0x80000000;

  if (TypeProj == TYPE_3D)
  {
    LongRotatePoint(&MatriceWorld, x, y, z);

		PosXWr = X0 - CameraXr;
		PosYWr = Y0 - CameraYr;
		PosZWr = Z0 - CameraZr;
  }
  else
  {
    // Object world position
    PosXWr = x - CameraXr;
    PosYWr = y - CameraYr;
    PosZWr = z - CameraZr;
  }

// ┌─────────────────────────────────────────────────────────────────┐
// │                                                                 │
// │                          GEOMETRY                               │
// │                                                                 │
// └─────────────────────────────────────────────────────────────────┘

  // **************
  // *** MATRIX ***
  // **************

	return Return_BodyDisplay(&MatriceWorld, TabMat, alpha, beta, gamma, obj);
}

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ BodyDisplay_AlphaBeta: Display a non-animated object (rotation Alp*Bet*Gam)║
// ╟────────────────────────────────────────────────────────────────────────────╢
// ║ Appel : (EAX, EBX, ECX) = (X, Y, Z)                                        ║
// ║         (EDX, ESI, EDI) = (Alpha, Beta, Gamma)                             ║
// ║         Stack           = Off_Body                                         ║
// ║                                                                            ║
// ║                                                                            ║
// ╚════════════════════════════════════════════════════════════════════════════╝
S32 BodyDisplay_AlphaBeta(S32 x, S32 y, S32 z, S32 alpha, S32 beta,
                                 S32 gamma, void *obj)
{
	lBody = (T_BODY_HEADER *)obj;
	ScreenXMin = 0x7FFFFFFF; // Init 2D box
	ScreenYMin = 0x7FFFFFFF;
	ScreenXMax = 0x80000000;
	ScreenYMax = 0x80000000;

	// Object position
	if (TypeProj == TYPE_3D)
	{
		LongRotatePoint(&MatriceWorld, x, y, z); // If perspective
		PosXWr = X0 - CameraXr;
		PosYWr = Y0 - CameraYr;
		PosZWr = Z0 - CameraZr;
	}
	else
	{
		PosXWr = x - CameraXr;
		PosYWr = y - CameraYr;
		PosZWr = z - CameraZr;
	}

// ; ┌─────────────────────────────────────────────────────────────────┐
// ; │                                                                 │
// ; │                          GEOMETRY                               │
// ; │                                                                 │
// ; └─────────────────────────────────────────────────────────────────┘

// ; **************
// ; *** MATRIX ***
// ; **************

	TYPE_MAT *mat = &TabMat[1];
	RotateMatrix(mat, &MatriceWorld, 0, beta, gamma);

	return Return_BodyDisplay(&TabMat[1], TabMat, alpha, 0, 0, obj);
}

Poly_Jump_Fn Poly_Jump[] = {
  Triangle_Solid, //  0
  Quad_Solid, //  0
  Triangle_Solid, //  0
  Quad_Solid, //  0

  Triangle_Flat, //  1
  Quad_Flat, //  1
  Triangle_Flat, //  1
  Quad_Flat, //  1

  Triangle_Solid, //  2
  Quad_Solid, //  2
  Triangle_Solid, //  2
  Quad_Solid, //  2

  Triangle_Solid, //  3
  Quad_Solid, //  3
  Triangle_Solid, //  3
  Quad_Solid, //  3

  Triangle_Gouraud, //  4
  Quad_Gouraud, //  4
  Triangle_Gouraud, //  4
  Quad_Gouraud, //  4

  Triangle_Gouraud, //  5
  Quad_Gouraud, //  5
  Triangle_Gouraud, //  5
  Quad_Gouraud, //  5

  Triangle_Gouraud, //  6
  Quad_Gouraud, //  6
  Triangle_Gouraud, //  6
  Quad_Gouraud, //  6

  Triangle_Gouraud, //  7
  Quad_Gouraud, //  7
  Triangle_Gouraud, //  7
  Quad_Gouraud, //  7

  Triangle_Texture_Solid, //  8
  Quad_Texture_Solid, //  8
  Triangle_EnvTexture_Solid, //  8
  Quad_EnvTexture_Solid, //  8

  Triangle_Texture_Flat, //  9
  Quad_Texture_Flat, //  9
  Triangle_EnvTexture_Flat, //  9
  Quad_EnvTexture_Flat, //  9

  Triangle_Texture_Gouraud, // 10
  Quad_Texture_Gouraud, // 10
  Triangle_EnvTexture_Gouraud, // 10
  Quad_EnvTexture_Gouraud, // 10

  Triangle_Texture_Gouraud, // 11
  Quad_Texture_Gouraud, // 11
  Triangle_EnvTexture_Gouraud, // 11
  Quad_EnvTexture_Gouraud, // 11

  Triangle_Texture_Solid, // 12
  Quad_Texture_Solid, // 12
  Triangle_EnvTexture_Solid, // 12
  Quad_EnvTexture_Solid, // 12

  Triangle_Texture_Flat, // 13
  Quad_Texture_Flat, // 13
  Triangle_EnvTexture_Flat, // 13
  Quad_EnvTexture_Flat, // 13

  Triangle_Texture_Gouraud, // 14
  Quad_Texture_Gouraud, // 14
  Triangle_EnvTexture_Gouraud, // 14
  Quad_EnvTexture_Gouraud, // 14

  Triangle_Texture_Gouraud, // 15
  Quad_Texture_Gouraud, // 15
  Triangle_EnvTexture_Gouraud, // 15
  Quad_EnvTexture_Gouraud, // 15

  Triangle_TextureZ_Solid, // 16
  Quad_TextureZ_Solid, // 16
  Triangle_EnvTexture_Solid, // 16
  Quad_EnvTexture_Solid, // 16

  Triangle_TextureZ_Flat, // 17
  Quad_TextureZ_Flat, // 17
  Triangle_EnvTexture_Flat, // 17
  Quad_EnvTexture_Flat, // 17

  Triangle_TextureZ_Gouraud, // 18
  Quad_TextureZ_Gouraud, // 18
  Triangle_EnvTexture_Gouraud, // 18
  Quad_EnvTexture_Gouraud, // 18

  Triangle_TextureZ_Gouraud, // 19
  Quad_TextureZ_Gouraud, // 19
  Triangle_EnvTexture_Gouraud, // 19
  Quad_EnvTexture_Gouraud, // 19

  Triangle_TextureZ_Solid, // 20
  Quad_TextureZ_Solid, // 20
  Triangle_EnvTexture_Solid, // 20
  Quad_EnvTexture_Solid, // 20

  Triangle_TextureZ_Flat, // 21
  Quad_TextureZ_Flat, // 21
  Triangle_EnvTexture_Flat, // 21
  Quad_EnvTexture_Flat, // 21

  Triangle_TextureZ_Gouraud, // 22
  Quad_TextureZ_Gouraud, // 22
  Triangle_EnvTexture_Gouraud, // 22
  Quad_EnvTexture_Gouraud, // 22

  Triangle_TextureZ_Gouraud, // 23
  Quad_TextureZ_Gouraud, // 23
  Triangle_EnvTexture_Gouraud, // 23
  Quad_EnvTexture_Gouraud, // 23

  Line, // Line
  Sphere, // Sphere
  Sphere_Transp, // Sphere transparent
};
